/* RSA im SE050: anlegen, oeffentlichen Teil lesen, signieren,
 * entschluesseln.
 *
 * Eigene Datei und nicht in se050.c, weil RSA nichts mit der
 * Kurventabelle zu tun hat: ein RSA-Schluessel hat eine Laenge statt einer
 * Kurve, sein oeffentlicher Teil sind zwei Zahlen statt eines Punktes, und
 * signiert wird nur mit einer ausdruecklich genannten Auffuellung. So
 * steht es im Vertrag (osk_se050.h), und so liegt es hier.
 *
 * Quelle fuer jeden Befehl: AN12413 Rev. 2.12; der Abschnitt steht je
 * Funktion dabei.
 *
 * WOFUER DAS DA IST, damit beim Kuerzen niemand das Entschluesseln
 * weglaesst: PIV-Steckplatz 9D und der OpenPGP-Verschluesselungsschluessel
 * brauchen osk_se050_rsa_decrypt(). Ohne das bleibt verschluesselte Post
 * ungelesen - signieren allein genuegt nicht. */
#include <string.h>
#include "osk_se050.h"
#include "osk_mem.h"
#include "se050_apdu.h"
#include "se050_tlv.h"
#include "se050_state.h"

/* Laengster Modulus, den der Vertrag kennt: 4096 bit. Jede Zahl, die so
 * lang werden kann - Modulus, Signatur, Chiffrat - bekommt dieses Mass. */
#define RSA_MAX_BYTES 512u

/* Nutzlast eines Befehls: Kennung (6) + Verfahren (3) + Daten
 * (bis 512 + 4 Byte TLV-Kopf). Bleibt unter MAX_APDU_PAYLOAD_LENGTH
 * (889 Byte, AN12413 Tabelle 16). */
#define RSA_PAYLOAD_BUF 540u

/* Antwort: eine Zahl von Modulusgroesse plus TLV-Kopf. */
#define RSA_RSP_BUF (RSA_MAX_BYTES + 8u)

/* ------------------------------------------------------------- Hilfen */

static osk_result_t bits_value_of(osk_rsa_bits_t bits, uint16_t *out)
{
	switch (bits) {
	case OSK_RSA_2048:
		*out = RSA_2048;
		return OSK_OK;
	case OSK_RSA_3072:
		*out = RSA_3072;
		return OSK_OK;
	case OSK_RSA_4096:
		*out = RSA_4096;
		return OSK_OK;
	default:
		return OSK_ERR_ARG;
	}
}

/* Die Hashlaenge, die zu diesem Verfahren gehoert.
 *
 * Geprueft wird sie, weil sie NICHT aus der Laenge ableitbar ist: der
 * Vertrag sagt es selbst, SHA-512/256 und SHA-256 sind beide 32 Byte lang.
 * Wer die Laenge statt des Verfahrens glaubt, laesst den Baustein ein
 * DigestInfo mit der falschen Kennung bauen - und die Signatur ist dann
 * gueltig geformt und trotzdem falsch. Genau dieses Muster ist in diesem
 * Projekt schon sechsmal aufgetreten: zwei Groessen, die gleich heissen
 * und Verschiedenes messen. */
static osk_result_t hash_len_of(osk_hash_t h, size_t *out)
{
	switch (h) {
	case OSK_HASH_SHA256:
		*out = 32u;
		return OSK_OK;
	case OSK_HASH_SHA384:
		*out = 48u;
		return OSK_OK;
	case OSK_HASH_SHA512:
		*out = 64u;
		return OSK_OK;
	default:
		return OSK_ERR_ARG;
	}
}

/* Signaturverfahren aus Auffuellung und Hashverfahren (Tabelle 31). */
static osk_result_t sign_algo_of(osk_rsa_pad_t pad, osk_hash_t h, uint8_t *out)
{
	if (pad == OSK_RSA_PAD_PKCS1_V15) {
		switch (h) {
		case OSK_HASH_SHA256:
			*out = RSA_SHA_256_PKCS1;
			return OSK_OK;
		case OSK_HASH_SHA384:
			*out = RSA_SHA_384_PKCS1;
			return OSK_OK;
		case OSK_HASH_SHA512:
			*out = RSA_SHA_512_PKCS1;
			return OSK_OK;
		default:
			return OSK_ERR_ARG;
		}
	}
	if (pad == OSK_RSA_PAD_PSS) {
		switch (h) {
		case OSK_HASH_SHA256:
			*out = RSA_SHA256_PKCS1_PSS;
			return OSK_OK;
		case OSK_HASH_SHA384:
			*out = RSA_SHA384_PKCS1_PSS;
			return OSK_OK;
		case OSK_HASH_SHA512:
			*out = RSA_SHA512_PKCS1_PSS;
			return OSK_OK;
		default:
			return OSK_ERR_ARG;
		}
	}
	if (pad == OSK_RSA_PAD_OAEP) {
		/* OAEP ist zum Entschluesseln da, nicht zum Signieren. Das
		 * ist ein Fehler im Aufruf, keine fehlende Faehigkeit. */
		return OSK_ERR_ARG;
	}
	if (pad == OSK_RSA_PAD_NONE) {
		/* BEFUND: Tabelle 31 kennt keine Fassung ohne Auffuellung.
		 * RSA_NO_PAD steht in Tabelle 32 und gilt nur fuer
		 * Ver- und Entschluesseln. Rohes Signieren kann der Baustein
		 * also nicht - abgelehnt, statt es ueber das Entschluesseln
		 * nachzubauen. Das waere eine selbstgebaute
		 * Signaturoperation, und genau die will dieses Projekt
		 * nicht. */
		return OSK_ERR_UNSUPPORTED;
	}
	return OSK_ERR_ARG;
}

/* Eine Zahl aus der Antwort kopieren. */
static osk_result_t read_tag1(const uint8_t *rsp, size_t n, uint8_t *out, size_t cap,
			      size_t *len_out)
{
	osk_tlv_reader_t rd;
	osk_tlv_reader_init(&rd, rsp, n);
	return osk_tlv_copy(&rd, TAG_1, out, cap, len_out);
}

/* ------------------------------------------------------ Welche Laengen */

osk_result_t osk_se050_rsa_sizes(uint32_t *mask_out)
{
	osk_se050_apdu_t *a = NULL;
	const osk_result_t r = osk_se050_state(&a);
	if (r != OSK_OK) {
		return r;
	}
	if (mask_out == NULL) {
		return OSK_ERR_ARG;
	}
	*mask_out = 0;

	/* Gefragt wird der Baustein, nicht die Verpackung. Das Merkmalbild
	 * aus VersionInfo (AN12413 Tabelle 5) sagt, ob RSA-Objekte
	 * ueberhaupt angelegt werden duerfen - CONFIG_RSA_PLAIN fuer rohe,
	 * CONFIG_RSA_CRT fuer CRT-Schluessel.
	 *
	 * Das ist nicht Vorsicht um ihrer selbst willen: der SE051A2 kann
	 * RSA ueberhaupt nicht, der SE051C2 kann es, und beide antworten
	 * auf dieselbe I2C-Adresse. Kommt hier 0 heraus, ist der falsche
	 * Baustein verbaut, und die Firmware meldet das, statt beim ersten
	 * Schluessel zu scheitern.
	 *
	 * GRENZE DIESER ABFRAGE, und sie gehoert dazu: der Baustein sagt
	 * OB, nicht WELCHE LAENGEN. Fuer Kurven gibt es ReadECCurveList
	 * (4.8.4), fuer RSA-Laengen gibt es keinen Befehl. Die drei Laengen
	 * unten stehen in AN12413 Tabelle 33 ("Other values are not
	 * supported", 4.7.1.2) - das ist eine Belegstelle, aber keine
	 * Messung am gelieferten Teil. Am Geraet ist die Gegenprobe die
	 * Kennung: A8FA oder A564 = C2 (richtig), A920 oder A565 = A2
	 * (falsch). Siehe docs/se050.md. */
	const uint16_t cfg = osk_se050_applet_config();
	if ((cfg & (CONFIG_RSA_PLAIN | CONFIG_RSA_CRT)) == 0u) {
		return OSK_OK;
	}

	*mask_out = (1u << OSK_RSA_2048) | (1u << OSK_RSA_3072) | (1u << OSK_RSA_4096);
	return OSK_OK;
}

/* ------------------------------------------------------ Schluessel anlegen */

osk_result_t osk_se050_rsa_key_create(osk_key_id_t id, osk_rsa_bits_t bits,
				      const osk_key_policy_t *policy)
{
	osk_se050_apdu_t *a = NULL;
	osk_result_t r = osk_se050_state(&a);
	if (r != OSK_OK) {
		return r;
	}

	uint16_t bits_value = 0;
	r = bits_value_of(bits, &bits_value);
	if (r != OSK_OK) {
		return r;
	}

	/* Wie bei den Kurven: eine Zusage, die das Geraet nicht halten
	 * soll, wird abgelehnt statt still uebergangen. */
	if (policy != NULL && policy->exportable) {
		return OSK_ERR_UNSUPPORTED;
	}

	/* Erst fragen, ob der Baustein RSA darf. Sonst kaeme ein nacktes
	 * "Bedingung nicht erfuellt" zurueck, und jemand suchte den Fehler
	 * bei der Freigabe statt beim verbauten Teil. */
	uint32_t sizes = 0;
	r = osk_se050_rsa_sizes(&sizes);
	if (r != OSK_OK) {
		return r;
	}
	if ((sizes & (1u << bits)) == 0u) {
		return OSK_ERR_UNSUPPORTED;
	}

	/* CRT oder roh - zwei verschiedene Formate, und das Format liegt
	 * nach dem Anlegen fest ("its format remains fixed and cannot be
	 * updated", 4.7.1.2). CRT ist die Wahl, wo der Baustein es kann:
	 * es rechnet schneller, und das zaehlt bei 4096 bit auf einem
	 * Baustein, der an einem I2C-Bus haengt. Kann er nur roh, wird roh
	 * angelegt - mit P2_RAW, das AN12413 ausdruecklich nur fuer
	 * "Schluesselpaar im Baustein erzeugen" erlaubt. */
	const uint16_t cfg = osk_se050_applet_config();
	const uint8_t p2 = (cfg & CONFIG_RSA_CRT) ? P2_DEFAULT : P2_RAW;

	/* WriteRSAKey (AN12413 4.7.1.2): ohne TAG_3 bis TAG_10 erzeugt der
	 * Baustein das Paar selbst - der private Teil entsteht also drin
	 * und kommt nie heraus.
	 *
	 * needs_touch gehoert nicht hierher: den Tastendruck verlangt die
	 * Anzeige- und Freigabeschicht vor dem Aufruf. Der SE050 kann
	 * keinen Taster lesen. */
	uint8_t payload[16];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);
	osk_tlv_put_u16(&w, TAG_2, bits_value);

	size_t pl = 0;
	r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	size_t dummy = 0;
	return osk_se050_apdu_exchange(a, INS_WRITE, (uint8_t)(P1_KEY_PAIR | P1_RSA), p2, payload,
				       pl, NULL, 0, &dummy);
}

/* -------------------------------------------------- Oeffentlicher Teil */

/* Einen Schluesselteil lesen (ReadObject, AN12413 4.7.3.1: TAG_4 traegt
 * den RSAKeyComponent, und nur damit liefert der Baustein Modulus oder
 * Exponent einzeln statt des ganzen Schluessels). */
static osk_result_t read_component(osk_se050_apdu_t *a, osk_key_id_t id, uint8_t comp,
				   uint8_t *out, size_t cap, size_t *len_out)
{
	uint8_t payload[16];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);
	osk_tlv_put_u8(&w, TAG_4, comp);

	size_t pl = 0;
	osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t rsp[RSA_RSP_BUF];
	size_t n = 0;
	r = osk_se050_apdu_exchange(a, INS_READ, P1_DEFAULT, P2_DEFAULT, payload, pl, rsp,
				    sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}
	return read_tag1(rsp, n, out, cap, len_out);
}

osk_result_t osk_se050_rsa_public(osk_key_id_t id, uint8_t *mod_out, size_t mod_cap,
				  size_t *mod_len_out, uint8_t *exp_out, size_t exp_cap,
				  size_t *exp_len_out)
{
	osk_se050_apdu_t *a = NULL;
	osk_result_t r = osk_se050_state(&a);
	if (r != OSK_OK) {
		return r;
	}
	if (mod_out == NULL || mod_len_out == NULL || exp_out == NULL || exp_len_out == NULL) {
		return OSK_ERR_ARG;
	}
	*mod_len_out = 0;
	*exp_len_out = 0;

	/* Zwei Befehle, weil der Baustein zwei Teile getrennt herausgibt.
	 * Beide Laengen werden gemeldet und keine angenommen: der Exponent
	 * ist je Baustein und Schluessel 3 oder 4 Byte lang, und wer 65537
	 * annimmt, baut irgendwann ein falsches Zertifikat. So steht es im
	 * Vertrag. */
	r = read_component(a, id, RSA_COMP_MOD, mod_out, mod_cap, mod_len_out);
	if (r != OSK_OK) {
		return r;
	}
	r = read_component(a, id, RSA_COMP_PUB_EXP, exp_out, exp_cap, exp_len_out);
	if (r != OSK_OK) {
		*mod_len_out = 0;
		return r;
	}
	return OSK_OK;
}

/* ---------------------------------------------------------- Signieren */

osk_result_t osk_se050_rsa_sign(osk_key_id_t id, osk_rsa_pad_t pad, osk_hash_t hash_alg,
				const uint8_t *hash, size_t hash_len, uint8_t *sig_out, size_t cap,
				size_t *len_out)
{
	osk_se050_apdu_t *a = NULL;
	osk_result_t r = osk_se050_state(&a);
	if (r != OSK_OK) {
		return r;
	}
	if (hash == NULL || hash_len == 0 || sig_out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	*len_out = 0;

	uint8_t algo = 0;
	r = sign_algo_of(pad, hash_alg, &algo);
	if (r != OSK_OK) {
		return r;
	}

	/* Das genannte Hashverfahren und die uebergebene Laenge muessen
	 * zusammenpassen. Siehe hash_len_of(). */
	size_t want = 0;
	r = hash_len_of(hash_alg, &want);
	if (r != OSK_OK) {
		return r;
	}
	if (hash_len != want) {
		return OSK_ERR_ARG;
	}

	/* RSASign (AN12413 4.11.1.1). Gehasht hat der Aufrufer - wie bei
	 * osk_se050_sign(), damit die Anzeige zeigt, was wirklich
	 * unterschrieben wird. */
	uint8_t payload[RSA_PAYLOAD_BUF];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);
	osk_tlv_put_u8(&w, TAG_2, algo);
	osk_tlv_put(&w, TAG_3, hash, hash_len);

	size_t pl = 0;
	r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t rsp[RSA_RSP_BUF];
	size_t n = 0;
	r = osk_se050_apdu_exchange(a, INS_CRYPTO, P1_SIGNATURE, P2_SIGN, payload, pl, rsp,
				    sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}
	return read_tag1(rsp, n, sig_out, cap, len_out);
}

/* ------------------------------------------------------ Entschluesseln */

osk_result_t osk_se050_rsa_decrypt(osk_key_id_t id, osk_rsa_pad_t pad, osk_hash_t hash_alg,
				   const uint8_t *in, size_t in_len, uint8_t *out, size_t cap,
				   size_t *len_out)
{
	osk_se050_apdu_t *a = NULL;
	osk_result_t r = osk_se050_state(&a);
	if (r != OSK_OK) {
		return r;
	}
	if (in == NULL || in_len == 0 || out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	*len_out = 0;
	if (in_len > RSA_MAX_BYTES) {
		/* Laenger als der laengste Modulus kann kein Chiffrat sein.
		 * Abgelehnt, nicht abgeschnitten. */
		return OSK_ERR_ARG;
	}

	uint8_t algo = 0;
	switch (pad) {
	case OSK_RSA_PAD_PKCS1_V15:
		/* Das ist der Weg, den PIV 9D und der
		 * OpenPGP-Verschluesselungsschluessel gehen. */
		algo = RSA_PKCS1;
		break;
	case OSK_RSA_PAD_NONE:
		algo = RSA_NO_PAD;
		break;
	case OSK_RSA_PAD_OAEP:
		/* BEFUND, gemeldet: AN12413 Tabelle 32 kennt OAEP nur
		 * "using SHA1 as digest" - eine einzige Fassung. osk_hash_t
		 * im Vertrag kennt kein SHA-1, also gibt es kein hash_alg,
		 * mit dem dieser Aufruf richtig waere. Still SHA-1 zu nehmen
		 * hiesse, eine andere Auffuellung zu rechnen als der
		 * Aufrufer bestellt hat; das ist der Fehler, den man erst
		 * beim Gegenrechnen auf der anderen Seite sieht.
		 *
		 * Offen bleibt, ob das SE051-Applet (6.0 oder 7.2) mehr
		 * kann: dessen Beschreibung AN12543 liegt hinter einer
		 * NXP-Anmeldung. Solange sie fehlt, gilt hier AN12413.
		 * Gebraucht wird OAEP fuer PIV und OpenPGP nicht - beide
		 * entschluesseln mit PKCS#1 v1.5. */
		return OSK_ERR_UNSUPPORTED;
	case OSK_RSA_PAD_PSS:
		/* PSS ist zum Signieren da. Fehler im Aufruf. */
		return OSK_ERR_ARG;
	default:
		return OSK_ERR_ARG;
	}
	/* hash_alg gilt nur bei OAEP, und OAEP kommt hier nicht durch.
	 * Darum wird es nicht angesehen - so steht es im Vertrag. */
	(void)hash_alg;

	/* RSADecrypt (AN12413 4.11.3.2). */
	uint8_t payload[RSA_PAYLOAD_BUF];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);
	osk_tlv_put_u8(&w, TAG_2, algo);
	osk_tlv_put(&w, TAG_3, in, in_len);

	size_t pl = 0;
	r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t rsp[RSA_RSP_BUF];
	size_t n = 0;
	r = osk_se050_apdu_exchange(a, INS_CRYPTO, P1_RSA, P2_DECRYPT_ONESHOT, payload, pl, rsp,
				    sizeof(rsp), &n);
	osk_wipe(payload, sizeof(payload));
	if (r != OSK_OK) {
		osk_wipe(rsp, sizeof(rsp));
		return r;
	}

	r = read_tag1(rsp, n, out, cap, len_out);
	/* Der Klartext darf nicht im Puffer liegenbleiben - er ist genau
	 * das Geheimnis, um das es hier geht. */
	osk_wipe(rsp, sizeof(rsp));
	return r;
}
