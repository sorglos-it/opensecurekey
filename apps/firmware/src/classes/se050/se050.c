/* Die Vertragsfunktionen aus apps/firmware/include/osk_se050.h.
 *
 * Diese Datei baut nur noch Befehle aus den Bausteinen darunter. Alles
 * Geheime bleibt im SE050: dieser Treiber bekommt nie einen privaten
 * Schluessel zu sehen, er nennt nur Kennungen.
 *
 * Quelle fuer jeden Befehl: AN12413 Rev. 2.12; der Abschnitt steht je
 * Funktion dabei. */
#include <string.h>
#include "osk_se050.h"
#include "se050_apdu.h"
#include "se050_tlv.h"
#include "se050_platform.h"

/* Kennung des PIN-Objekts im Baustein. Fest, weil es genau eines gibt. */
#define OSK_PIN_OBJECT_ID 0x0F000001u

/* Hoechstzahl Fehlversuche, die der SE050 selbst zaehlt. Der Zaehler liegt
 * im Chip, nicht im Flash - Flash loeschen setzt ihn nicht zurueck. Das
 * ist genau die Zusage aus docs/threat-model.md. */
#define OSK_PIN_MAX_ATTEMPTS 8u

/* PIN-Laenge. Untergrenze nach der Hausregel "Mindestlaenge in Zeichen",
 * Obergrenze aus AN12413 4.7.1.5: ein UserID-Wert ist 4 bis 16 Byte. */
#define OSK_PIN_MIN_LEN 4u
#define OSK_PIN_MAX_LEN 16u

/* Merkmalbits aus VersionInfo (AN12413 Tabelle 5). Nur die, die wir
 * wirklich abfragen. */
#define CONFIG_ECDSA_ECDH_ECDHE 0x0002u
#define CONFIG_EDDSA 0x0004u
#define CONFIG_DH_MONT 0x0008u

/* Genau ein Baustein je Geraet, darum ein Zustand. Nicht mehrfach
 * benutzbar zu machen ist hier richtig: zwei offene Kanaele auf denselben
 * Chip waeren ein Fehler, kein Merkmal. */
static osk_se050_apdu_t g_apdu;
static bool g_open;
static uint16_t g_applet_config;

/* ------------------------------------------------------------- Hilfen */

/* Laenge des oeffentlichen Schluessels je Kurve (AN12413 Tabelle 1):
 * Weierstrass unkomprimiert 0x04 mit X und Y, Curve25519 glatt 32 Byte. */
static size_t pub_len_of(osk_curve_t c)
{
	switch (c) {
	case OSK_CURVE_P256:
	case OSK_CURVE_SECP256K1:
		return 65u;
	case OSK_CURVE_P384:
		return 97u;
	case OSK_CURVE_ED25519:
	case OSK_CURVE_X25519:
		return 32u;
	default:
		return 0u;
	}
}

static osk_result_t curve_id_of(osk_curve_t c, uint8_t *out)
{
	switch (c) {
	case OSK_CURVE_P256:
		*out = EC_NIST_P256;
		return OSK_OK;
	case OSK_CURVE_P384:
		*out = EC_NIST_P384;
		return OSK_OK;
	case OSK_CURVE_SECP256K1:
		*out = EC_SECP256K1;
		return OSK_OK;
	case OSK_CURVE_ED25519:
		*out = EC_ED25519;
		return OSK_OK;
	case OSK_CURVE_X25519:
		*out = EC_MONT_DH_25519;
		return OSK_OK;
	default:
		return OSK_ERR_ARG;
	}
}

/* Objekttyp eines Schluessels erfragen (AN12413 4.7.4.1 ReadType).
 * Daran erkennt der Treiber die Kurve, ohne sie sich merken zu muessen -
 * ein Zustand, der zum Chip auseinanderlaufen koennte, waere schlimmer
 * als ein zusaetzlicher Befehl. Es liefert allerdings NUR den Typ, nicht
 * die Kurve; fuer das Signaturverfahren brauchen wir die Kurve, darum
 * fragen wir sie ueber die Laenge des oeffentlichen Teils ab. */
static osk_result_t curve_of_key(osk_key_id_t id, osk_curve_t *out)
{
	uint8_t pub[128];
	size_t n = 0;

	const osk_result_t r = osk_se050_key_public(id, pub, sizeof(pub), &n);
	if (r != OSK_OK) {
		return r;
	}

	/* Die Laenge ist eindeutig, bis auf P256 und secp256k1: beide 65
	 * Byte. Fuer das Signaturverfahren macht das keinen Unterschied -
	 * beide sind Weierstrass-Kurven und nehmen SIG_ECDSA_SHA_256. */
	switch (n) {
	case 65u:
		*out = OSK_CURVE_P256;
		return OSK_OK;
	case 97u:
		*out = OSK_CURVE_P384;
		return OSK_OK;
	case 32u:
		*out = OSK_CURVE_ED25519;
		return OSK_OK;
	default:
		return OSK_ERR_FORMAT;
	}
}

/* -------------------------------------------------------- Auf und zu */

osk_result_t osk_se050_open(void)
{
	if (g_open) {
		return OSK_ERR_EXISTS;
	}

	osk_se050_bus_t *bus = NULL;
	osk_result_t r = osk_se050_platform_bus(&bus);
	if (r != OSK_OK) {
		return r;
	}

	/* Die Antwort des Auswahlbefehls ist 7 Byte VersionInfo
	 * (AN12413 Tabelle 52 und Abschnitt 4.3.33):
	 *   Haupt-, Neben-, Fehlerstand | 2 Byte Merkmale | 2 Byte SecureBox */
	uint8_t ver[16];
	size_t ver_len = 0;
	r = osk_se050_apdu_open(&g_apdu, bus, ver, sizeof(ver), &ver_len);
	if (r != OSK_OK) {
		return r;
	}
	if (ver_len < 5u) {
		osk_se050_apdu_close(&g_apdu);
		return OSK_ERR_FORMAT;
	}
	g_applet_config = (uint16_t)(((uint16_t)ver[3] << 8) | ver[4]);

	/* Jetzt der verschluesselte Kanal. Ohne ihn geht es nicht weiter:
	 * ein offener I2C-Bus zwischen den zwei Chips waere genau das Loch,
	 * das der SE050 schliessen soll. */
	osk_scp03_static_keys_t keys;
	r = osk_se050_platform_scp_keys(&keys);
	if (r == OSK_OK) {
		r = osk_se050_apdu_open_channel(&g_apdu, &keys);
	}
	/* Die Kopie der festen Schluessel verschwindet sofort - der
	 * nRF52840 soll kein Geheimnis halten. */
	osk_wipe(&keys, sizeof(keys));

	if (r != OSK_OK) {
		osk_se050_apdu_close(&g_apdu);
		return r;
	}

	g_open = true;
	return OSK_OK;
}

void osk_se050_close(void)
{
	if (!g_open) {
		return;
	}
	osk_se050_apdu_close(&g_apdu);
	osk_wipe(&g_apdu, sizeof(g_apdu));
	g_applet_config = 0;
	g_open = false;
}

/* ------------------------------------------------------------- Kurven */

osk_result_t osk_se050_curves(uint32_t *mask_out)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (mask_out == NULL) {
		return OSK_ERR_ARG;
	}
	*mask_out = 0;

	/* ReadECCurveList (AN12413 4.8.4) liefert in TAG_1 je Kurve mit
	 * Kennung unter 0x40 ein Byte SetIndicator - also ein Feld, dessen
	 * Platz die Kurvenkennung ist. Ed25519 (0x40) und X25519 (0x41)
	 * stehen dort NICHT drin; die kommen aus den Merkmalbits. */
	uint8_t rsp[64];
	size_t n = 0;
	osk_result_t r = osk_se050_apdu_exchange(&g_apdu, INS_READ, P1_CURVE, P2_LIST, NULL, 0, rsp,
						 sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}

	osk_tlv_reader_t rd;
	osk_tlv_reader_init(&rd, rsp, n);

	const uint8_t *list = NULL;
	size_t list_len = 0;
	r = osk_tlv_get(&rd, TAG_1, &list, &list_len);
	if (r != OSK_OK) {
		return r;
	}

	/* Die Liste beginnt bei Kurvenkennung 0x00 (UNUSED). */
	if (list_len > EC_NIST_P256 && list[EC_NIST_P256] == SE050_SET) {
		*mask_out |= 1u << OSK_CURVE_P256;
	}
	if (list_len > EC_NIST_P384 && list[EC_NIST_P384] == SE050_SET) {
		*mask_out |= 1u << OSK_CURVE_P384;
	}
	if (list_len > EC_SECP256K1 && list[EC_SECP256K1] == SE050_SET) {
		*mask_out |= 1u << OSK_CURVE_SECP256K1;
	}

	/* Weierstrass-Kurven brauchen zusaetzlich das Merkmal fuer ECDSA
	 * und DH: eine angelegte Kurve, mit der das Applet nicht rechnen
	 * darf, hilft uns nicht. */
	if ((g_applet_config & CONFIG_ECDSA_ECDH_ECDHE) == 0) {
		*mask_out &= ~((1u << OSK_CURVE_P256) | (1u << OSK_CURVE_P384) |
			       (1u << OSK_CURVE_SECP256K1));
	}

	if (g_applet_config & CONFIG_EDDSA) {
		*mask_out |= 1u << OSK_CURVE_ED25519;
	}
	if (g_applet_config & CONFIG_DH_MONT) {
		*mask_out |= 1u << OSK_CURVE_X25519;
	}
	return OSK_OK;
}

/* ------------------------------------------------------------- Zufall */

osk_result_t osk_se050_random(uint8_t *out, size_t len)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (out == NULL || len == 0) {
		return OSK_ERR_ARG;
	}

	/* GetRandom (AN12413 4.19.4). Der Baustein gibt je Aufruf nur so
	 * viel, wie in eine Antwort passt - darum in Haeppchen, bis der
	 * Aufrufer sein Mass hat. */
	size_t done = 0;
	while (done < len) {
		size_t want = len - done;
		if (want > 128u) {
			want = 128u;
		}

		uint8_t payload[8];
		osk_tlv_writer_t w;
		osk_tlv_init(&w, payload, sizeof(payload));
		osk_tlv_put_u16(&w, TAG_1, (uint16_t)want);

		size_t pl = 0;
		osk_result_t r = osk_tlv_done(&w, &pl);
		if (r != OSK_OK) {
			return r;
		}

		uint8_t rsp[160];
		size_t n = 0;
		r = osk_se050_apdu_exchange(&g_apdu, INS_MGMT, P1_DEFAULT, P2_RANDOM, payload, pl,
					    rsp, sizeof(rsp), &n);
		if (r != OSK_OK) {
			osk_wipe(out, len);
			return r;
		}

		osk_tlv_reader_t rd;
		osk_tlv_reader_init(&rd, rsp, n);

		size_t got = 0;
		r = osk_tlv_copy(&rd, TAG_1, &out[done], want, &got);
		osk_wipe(rsp, sizeof(rsp));
		if (r != OSK_OK) {
			osk_wipe(out, len);
			return r;
		}
		if (got == 0) {
			/* Kein Fortschritt - sonst dreht die Schleife ewig. */
			osk_wipe(out, len);
			return OSK_ERR_IO;
		}
		done += got;
	}
	return OSK_OK;
}

/* ------------------------------------------------------- Schluessel */

osk_result_t osk_se050_key_create(osk_key_id_t id, osk_curve_t curve,
				  const osk_key_policy_t *policy)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}

	uint8_t curve_id = 0;
	osk_result_t r = curve_id_of(curve, &curve_id);
	if (r != OSK_OK) {
		return r;
	}

	/* Was dieses Projekt nicht tut: Schluessel exportierbar anlegen.
	 * Der Vertrag kennt das Feld, damit der Aufrufer es sieht - aber
	 * eine Zusage, die das Geraet gar nicht halten soll, lehnen wir
	 * ab, statt sie still zu uebergehen. */
	if (policy != NULL && policy->exportable) {
		return OSK_ERR_UNSUPPORTED;
	}

	/* WriteECKey (AN12413 4.7.1.1): ohne TAG_3 und TAG_4 erzeugt der
	 * Baustein das Paar selbst - der private Teil entsteht also drin
	 * und kommt nie heraus. Genau so soll es sein.
	 *
	 * needs_touch gehoert nicht hierher: der Tastendruck wird vor dem
	 * Aufruf von osk_se050_sign() verlangt, von der Anzeige- und
	 * Freigabeschicht. Der SE050 kann keinen Taster lesen. */
	uint8_t payload[16];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);
	osk_tlv_put_u8(&w, TAG_2, curve_id);

	size_t pl = 0;
	r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	return osk_se050_apdu_exchange(&g_apdu, INS_WRITE, (uint8_t)(P1_KEY_PAIR | P1_EC),
				       P2_DEFAULT, payload, pl, NULL, 0, &pl);
}

osk_result_t osk_se050_key_delete(osk_key_id_t id)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}

	/* DeleteSecureObject (AN12413 4.7.4.5). */
	uint8_t payload[8];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);

	size_t pl = 0;
	const osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	return osk_se050_apdu_exchange(&g_apdu, INS_MGMT, P1_DEFAULT, P2_DELETE_OBJECT, payload, pl,
				       NULL, 0, &pl);
}

osk_result_t osk_se050_key_public(osk_key_id_t id, uint8_t *out, size_t cap, size_t *len_out)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}

	/* ReadObject (AN12413 4.7.3.1): bei einem Schluesselpaar kommt der
	 * oeffentliche Teil - der private kann gar nicht kommen, das lehnt
	 * der Baustein von sich aus ab. */
	uint8_t payload[8];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);

	size_t pl = 0;
	osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t rsp[192];
	size_t n = 0;
	r = osk_se050_apdu_exchange(&g_apdu, INS_READ, P1_DEFAULT, P2_DEFAULT, payload, pl, rsp,
				    sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}

	osk_tlv_reader_t rd;
	osk_tlv_reader_init(&rd, rsp, n);
	return osk_tlv_copy(&rd, TAG_1, out, cap, len_out);
}

/* ---------------------------------------------------------- Signieren */

osk_result_t osk_se050_sign(osk_key_id_t id, const uint8_t *hash, size_t hash_len, uint8_t *sig_out,
			    size_t cap, size_t *len_out)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (hash == NULL || hash_len == 0 || sig_out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}

	osk_curve_t curve;
	osk_result_t r = curve_of_key(id, &curve);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t algo;
	uint8_t p1;
	if (curve == OSK_CURVE_ED25519) {
		/* Ed25519 hasht selbst: hier geht die NACHRICHT hinein, nicht
		 * ihr Hash (AN12413 4.10.1.2). Der Vertrag nennt den
		 * Parameter "hash" - fuer diese Kurve ist es die Nachricht,
		 * und der Aufrufer muss das wissen. */
		algo = SIG_ED25519PURE;
		p1 = P1_SIGNATURE;
	} else {
		/* ECDSA: die Laenge des Hashes muss zum genannten Verfahren
		 * passen, sonst verrechnet sich der Baustein still
		 * (AN12413 4.10.1.1). Also aus der Laenge ableiten. */
		switch (hash_len) {
		case 32u:
			algo = SIG_ECDSA_SHA_256;
			break;
		case 48u:
			algo = SIG_ECDSA_SHA_384;
			break;
		case 64u:
			algo = SIG_ECDSA_SHA_512;
			break;
		default:
			return OSK_ERR_ARG;
		}
		p1 = P1_SIGNATURE;
	}

	if (hash_len > 128u) {
		return OSK_ERR_ARG;
	}

	uint8_t payload[16u + 128u];
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

	uint8_t rsp[160];
	size_t n = 0;
	r = osk_se050_apdu_exchange(&g_apdu, INS_CRYPTO, p1, P2_SIGN, payload, pl, rsp, sizeof(rsp),
				    &n);
	if (r != OSK_OK) {
		return r;
	}

	osk_tlv_reader_t rd;
	osk_tlv_reader_init(&rd, rsp, n);
	return osk_tlv_copy(&rd, TAG_1, sig_out, cap, len_out);
}

/* --------------------------------------------------------------- ECDH */

osk_result_t osk_se050_ecdh(osk_key_id_t id, const uint8_t *peer_pub, size_t peer_len,
			    uint8_t *secret_out, size_t cap, size_t *len_out)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (peer_pub == NULL || peer_len == 0 || secret_out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (peer_len > 128u) {
		return OSK_ERR_ARG;
	}

	/* ECDHGenerateSharedSecret (AN12413 4.10.3.1). */
	uint8_t payload[16u + 128u];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);
	osk_tlv_put(&w, TAG_2, peer_pub, peer_len);

	size_t pl = 0;
	osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t rsp[96];
	size_t n = 0;
	r = osk_se050_apdu_exchange(&g_apdu, INS_CRYPTO, P1_EC, P2_DH, payload, pl, rsp,
				    sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}

	osk_tlv_reader_t rd;
	osk_tlv_reader_init(&rd, rsp, n);
	r = osk_tlv_copy(&rd, TAG_1, secret_out, cap, len_out);
	osk_wipe(rsp, sizeof(rsp));
	return r;
}

/* ------------------------------------------------------ Ableitung, MAC */

osk_result_t osk_se050_hkdf(osk_key_id_t id, const uint8_t *info, size_t info_len, uint8_t *out,
			    size_t out_len)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (out == NULL || out_len == 0 || (info_len > 0 && info == NULL)) {
		return OSK_ERR_ARG;
	}
	/* AN12413 4.14.1: Kontext 1 bis 448 Byte, Ausgabe bis zur
	 * Nutzlastgrenze. Wir begrenzen auf das, was in einen Puffer passt. */
	if (info_len > 192u || out_len > 192u) {
		return OSK_ERR_ARG;
	}

	/* HKDF nach RFC 5869, nur im Modus Extract-And-Expand. Das Salz
	 * (TAG_3) lassen wir weg - dann nimmt der Baustein sein Vorgabesalz.
	 * Das ist richtig: das Grundgeheimnis liegt im Chip, und die
	 * Trennung der abgeleiteten Schluessel macht der Kontext. */
	uint8_t payload[16u + 192u];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);
	osk_tlv_put_u8(&w, TAG_2, DIGEST_SHA256);
	if (info_len > 0) {
		osk_tlv_put(&w, TAG_4, info, info_len);
	}
	osk_tlv_put_u16(&w, TAG_5, (uint16_t)out_len);

	size_t pl = 0;
	osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t rsp[256];
	size_t n = 0;
	r = osk_se050_apdu_exchange(&g_apdu, INS_CRYPTO, P1_DEFAULT, P2_HKDF, payload, pl, rsp,
				    sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}

	osk_tlv_reader_t rd;
	osk_tlv_reader_init(&rd, rsp, n);

	size_t got = 0;
	r = osk_tlv_copy(&rd, TAG_1, out, out_len, &got);
	osk_wipe(rsp, sizeof(rsp));
	if (r != OSK_OK) {
		osk_wipe(out, out_len);
		return r;
	}
	if (got != out_len) {
		/* Kuerzer als bestellt ist ein Fehler, nicht ein Teilergebnis:
		 * ein halb gefuellter Schluessel waere schwach und wuerde
		 * nicht auffallen. */
		osk_wipe(out, out_len);
		return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}

osk_result_t osk_se050_hmac(osk_key_id_t id, const uint8_t *msg, size_t msg_len, uint8_t *mac_out,
			    size_t cap, size_t *len_out)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (mac_out == NULL || len_out == NULL || (msg_len > 0 && msg == NULL)) {
		return OSK_ERR_ARG;
	}
	if (msg_len > 192u) {
		/* Laengere Nachrichten gehen beim SE050 ueber
		 * MACInit/Update/Final. Das braucht hier niemand, darum ist
		 * es nicht da - und es wird abgelehnt, nicht abgeschnitten. */
		return OSK_ERR_ARG;
	}

	/* MACOneShot (AN12413 4.13.4). */
	uint8_t payload[16u + 192u];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, id);
	osk_tlv_put_u8(&w, TAG_2, HMAC_SHA256);
	osk_tlv_put(&w, TAG_3, msg, msg_len);

	size_t pl = 0;
	osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t rsp[96];
	size_t n = 0;
	r = osk_se050_apdu_exchange(&g_apdu, INS_CRYPTO, P1_MAC, P2_GENERATE_ONESHOT, payload, pl,
				    rsp, sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}

	osk_tlv_reader_t rd;
	osk_tlv_reader_init(&rd, rsp, n);
	r = osk_tlv_copy(&rd, TAG_1, mac_out, cap, len_out);
	osk_wipe(rsp, sizeof(rsp));
	return r;
}

/* ------------------------------------------------------------------ PIN */

/* Die PIN liegt als UserID-Objekt im SE050 (AN12413 4.7.1.5), angelegt mit
 * INS_AUTH_OBJECT und einer Hoechstzahl von Versuchen. Geprueft wird sie
 * mit VerifySessionUserID (4.5.2.1): der Vergleich findet IM CHIP statt,
 * der Wirt erfaehrt nur Ja oder Nein, und der Fehlversuchszaehler liegt
 * ebenfalls dort. Flash loeschen setzt ihn nicht zurueck. */

osk_result_t osk_se050_pin_verify(const uint8_t *pin, size_t len)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (pin == NULL || len < OSK_PIN_MIN_LEN || len > OSK_PIN_MAX_LEN) {
		return OSK_ERR_ARG;
	}

	/* Erst eine Sitzung auf dem PIN-Objekt oeffnen, dann darin die PIN
	 * vorlegen (AN12413 4.5.1.1 und 4.5.2.1). */
	uint8_t payload[8];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, OSK_PIN_OBJECT_ID);

	size_t pl = 0;
	osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t rsp[32];
	size_t n = 0;
	r = osk_se050_apdu_exchange(&g_apdu, INS_MGMT, P1_DEFAULT, P2_SESSION_CREATE, payload, pl,
				    rsp, sizeof(rsp), &n);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t session[8];
	size_t session_len = 0;
	{
		osk_tlv_reader_t rd;
		osk_tlv_reader_init(&rd, rsp, n);
		r = osk_tlv_copy(&rd, TAG_1, session, sizeof(session), &session_len);
	}
	if (r != OSK_OK) {
		return r;
	}
	if (session_len != sizeof(session)) {
		return OSK_ERR_FORMAT;
	}

	/* VerifySessionUserID, eingepackt in ProcessSessionCmd. */
	uint8_t inner[5u + 2u + OSK_PIN_MAX_LEN];
	size_t inner_len = 0;
	{
		uint8_t ip[2u + OSK_PIN_MAX_LEN];
		osk_tlv_writer_t iw;
		osk_tlv_init(&iw, ip, sizeof(ip));
		osk_tlv_put(&iw, TAG_1, pin, len);

		size_t ipl = 0;
		r = osk_tlv_done(&iw, &ipl);
		if (r != OSK_OK) {
			osk_wipe(ip, sizeof(ip));
			return r;
		}

		inner[0] = SE050_CLA;
		inner[1] = INS_MGMT;
		inner[2] = P1_DEFAULT;
		inner[3] = P2_SESSION_USERID;
		inner[4] = (uint8_t)ipl;
		memcpy(&inner[5], ip, ipl);
		inner_len = 5u + ipl;
		osk_wipe(ip, sizeof(ip));
	}

	uint8_t outer[16u + sizeof(inner)];
	size_t outer_len = 0;
	{
		osk_tlv_writer_t ow;
		osk_tlv_init(&ow, outer, sizeof(outer));
		osk_tlv_put(&ow, TAG_SESSION_ID, session, session_len);
		osk_tlv_put(&ow, TAG_1, inner, inner_len);
		r = osk_tlv_done(&ow, &outer_len);
	}
	osk_wipe(inner, sizeof(inner));
	if (r != OSK_OK) {
		osk_wipe(outer, sizeof(outer));
		return r;
	}

	n = 0;
	r = osk_se050_apdu_exchange(&g_apdu, INS_PROCESS, P1_DEFAULT, P2_DEFAULT, outer, outer_len,
				    rsp, sizeof(rsp), &n);
	osk_wipe(outer, sizeof(outer));

	/* Die Sitzung wird in jedem Fall geschlossen - auch bei falscher
	 * PIN. Eine offen gelassene Sitzung waere eine halb offene Tuer,
	 * und der Baustein hat nur zwei davon (Tabelle 16). */
	{
		uint8_t cp[16];
		osk_tlv_writer_t cw;
		osk_tlv_init(&cw, cp, sizeof(cp));
		osk_tlv_put(&cw, TAG_SESSION_ID, session, session_len);

		size_t cl = 0;
		if (osk_tlv_done(&cw, &cl) == OSK_OK) {
			size_t dummy = 0;
			(void)osk_se050_apdu_exchange(&g_apdu, INS_MGMT, P1_DEFAULT,
						      P2_SESSION_CLOSE, cp, cl, NULL, 0, &dummy);
		}
	}
	osk_wipe(session, sizeof(session));

	/* Der Baustein meldet eine falsche PIN als
	 * SW_CONDITIONS_NOT_SATISFIED, was die APDU-Schicht zu
	 * OSK_ERR_DENIED macht - genau der richtige Wert. Ist der Zaehler
	 * abgelaufen, kommt dasselbe Statuswort; wir koennen die zwei
	 * Faelle nicht unterscheiden, und das ist in Ordnung: der Nutzer
	 * fragt dann osk_se050_pin_tries_left(). */
	return r;
}

osk_result_t osk_se050_pin_set(const uint8_t *old_pin, size_t old_len, const uint8_t *new_pin,
			       size_t new_len)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (new_pin == NULL || new_len < OSK_PIN_MIN_LEN || new_len > OSK_PIN_MAX_LEN) {
		return OSK_ERR_ARG;
	}

	/* Gibt es schon eine PIN, muss die alte stimmen, bevor die neue
	 * gilt. Beim ersten Einrichten ist old_pin NULL - dann wird das
	 * Objekt neu angelegt, und der Baustein lehnt ein Ueberschreiben
	 * eines vorhandenen Objekts ohne Freigabe ohnehin ab. */
	if (old_pin != NULL) {
		if (old_len < OSK_PIN_MIN_LEN || old_len > OSK_PIN_MAX_LEN) {
			return OSK_ERR_ARG;
		}
		const osk_result_t vr = osk_se050_pin_verify(old_pin, old_len);
		if (vr != OSK_OK) {
			return vr;
		}
	}

	/* WriteUserID (AN12413 4.7.1.5). INS_AUTH_OBJECT macht daraus ein
	 * Anmeldeobjekt - nur dann zaehlt der Baustein Fehlversuche
	 * (Abschnitt 3.2.4.4). TAG_MAX_ATTEMPTS und TAG_POLICY greifen nur
	 * beim Anlegen; bei einem vorhandenen Objekt werden sie uebergangen. */
	uint8_t payload[16u + OSK_PIN_MAX_LEN];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u16(&w, TAG_MAX_ATTEMPTS, OSK_PIN_MAX_ATTEMPTS);
	osk_tlv_put_u32(&w, TAG_1, OSK_PIN_OBJECT_ID);
	osk_tlv_put(&w, TAG_2, new_pin, new_len);

	size_t pl = 0;
	osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		osk_wipe(payload, sizeof(payload));
		return r;
	}

	size_t dummy = 0;
	r = osk_se050_apdu_exchange(&g_apdu, (uint8_t)(INS_WRITE | INS_AUTH_OBJECT), P1_USERID,
				    P2_DEFAULT, payload, pl, NULL, 0, &dummy);
	osk_wipe(payload, sizeof(payload));
	return r;
}

osk_result_t osk_se050_pin_tries_left(uint8_t *tries_out)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (tries_out == NULL) {
		return OSK_ERR_ARG;
	}
	*tries_out = 0;

	/* HIER GEHT ES NICHT WEITER, und zwar nicht aus Bequemlichkeit:
	 *
	 * AN12413 Abschnitt 3.2.4.4 sagt ausdruecklich, dass der SE050 den
	 * Fehlversuchszaehler fuer Objekte des Typs UserID NICHT meldet -
	 * der Wert bleibt 0, auch beim beglaubigten Lesen. Das Erratasheet
	 * fuehrt dasselbe als APP.3 und nennt als einzigen Ausweg, ein
	 * AESKey-Objekt zur Anmeldung zu nehmen statt einer UserID.
	 *
	 * Der Zaehler EXISTIERT und wirkt: nach OSK_PIN_MAX_ATTEMPTS
	 * Fehlversuchen sperrt der Baustein das Objekt. Nur ABLESEN laesst
	 * er ihn nicht. Eine erratene Zahl zurueckzugeben waere das
	 * Schlimmste von allem - der Nutzer saehe "3 Versuche uebrig" und
	 * waere beim naechsten Versuch ausgesperrt.
	 *
	 * Richtige Loesung, sobald jemand sie entscheidet: PIN als
	 * AESKey-Anmeldeobjekt (Schluessel aus der PIN abgeleitet) statt als
	 * UserID. Dann liefert ein beglaubigtes ReadObject die Attribute mit
	 * Zaehler und Hoechstzahl. Das aendert den PIN-Pfad vollstaendig und
	 * ist dem Master gemeldet - siehe docs/se050.md. */
	return OSK_ERR_UNSUPPORTED;
}

/* ------------------------------------------- Fluechtige Ableitung */

osk_result_t osk_se050_key_derive_transient(osk_key_id_t root, osk_curve_t curve,
					    const uint8_t *info, size_t info_len, osk_key_id_t slot)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (info == NULL || info_len == 0) {
		return OSK_ERR_ARG;
	}

	uint8_t curve_id = 0;
	const osk_result_t r = curve_id_of(curve, &curve_id);
	if (r != OSK_OK) {
		return r;
	}
	(void)root;
	(void)slot;

	/* HIER GEHT ES NICHT WEITER - und das ist ein Befund, keine Luecke.
	 *
	 * Der Vertrag verlangt genau das Richtige: der private Teil darf den
	 * Chip nicht verlassen. Der SE05x kann das aber nicht leisten. Seine
	 * einzigen Ableitungsbefehle sind HKDF (AN12413 4.14.1) und PBKDF2
	 * (4.14.2); BEIDE liefern das Ergebnis in TLV[TAG_1] der Antwort,
	 * also an den Wirt. Es gibt keinen Befehl, der ein abgeleitetes
	 * Schluesselpaar INNEN in ein Objekt legt, und WriteECKey nimmt den
	 * privaten Wert nur als Eingabe vom Wirt (4.7.1.1, TAG_3).
	 *
	 * Der naheliegende Weg - HKDF holen, dann WriteECKey damit - fuehrt
	 * den privaten Schluessel durch den nRF52840. Dessen Auslesesperre
	 * gilt uns als gebrochen (docs/architecture.md), also waere das
	 * genau die Zusage, die wir nicht halten koennen. Eine Funktion
	 * anzubieten, die sicher aussieht und es nicht ist, ist schlimmer
	 * als eine, die ehrlich ablehnt.
	 *
	 * Was der Baustein STATT DESSEN kann (ExportObject/ImportObject,
	 * AN12413 4.7.3.2 und 4.7.1.8): ein Schluesselpaar im Chip erzeugen,
	 * als verschluesseltes Abbild heraus- und spaeter wieder
	 * hineingeben. Der private Teil ist dabei nie im Klartext draussen.
	 * Das ist aber "im Chip erzeugt und vom Chip verpackt", nicht "aus
	 * einem Grundgeheimnis abgeleitet" - und der Schluessel, mit dem der
	 * Chip verpackt, ist geraetegebunden, womit das zweite Geraet aus
	 * docs/architecture.md nicht mehr aufgeht.
	 *
	 * Das ist eine Entscheidung ueber den Aufbau, nicht ueber den
	 * Treiber. Sie liegt beim Master-Thread und beim CTAP-Strang und ist
	 * gemeldet; die Fassung mit Begruendung steht in docs/se050.md. */
	return OSK_ERR_UNSUPPORTED;
}

/* --------------------------------------------------------- PIN loeschen */

osk_result_t osk_se050_pin_clear(void)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}

	/* DeleteSecureObject auf das PIN-Objekt (AN12413 4.7.4.5). Der
	 * Fehlversuchszaehler verschwindet mit dem Objekt - das ist beim
	 * Zuruecksetzen auf Werkszustand gewollt und genau der Grund, warum
	 * diese Funktion eine Freigabe am Geraet voraussetzt. Geprueft wird
	 * sie hier nicht, so steht es im Vertrag.
	 *
	 * Ein nicht vorhandenes Objekt ist kein Fehler: nach dem Loeschen
	 * soll zweimal Loeschen denselben Zustand ergeben wie einmal. */
	uint8_t payload[8];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u32(&w, TAG_1, OSK_PIN_OBJECT_ID);

	size_t pl = 0;
	osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	size_t dummy = 0;
	r = osk_se050_apdu_exchange(&g_apdu, INS_MGMT, P1_DEFAULT, P2_DELETE_OBJECT, payload, pl,
				    NULL, 0, &dummy);
	if (r == OSK_ERR_DENIED) {
		/* Der Baustein meldet "Bedingung nicht erfuellt" auch, wenn
		 * das Objekt gar nicht da ist. Erst nachsehen, dann
		 * entscheiden - sonst meldet ein zweiter Aufruf einen
		 * Fehler, obwohl der Zustand stimmt. */
		uint8_t exists[8];
		osk_tlv_writer_t ew;
		osk_tlv_init(&ew, exists, sizeof(exists));
		osk_tlv_put_u32(&ew, TAG_1, OSK_PIN_OBJECT_ID);

		size_t el = 0;
		if (osk_tlv_done(&ew, &el) != OSK_OK) {
			return r;
		}

		uint8_t rsp[32];
		size_t n = 0;
		const osk_result_t tr = osk_se050_apdu_exchange(
		    &g_apdu, INS_READ, P1_DEFAULT, P2_TYPE, exists, el, rsp, sizeof(rsp), &n);
		if (tr != OSK_OK) {
			/* Typ nicht lesbar heisst: kein solches Objekt. */
			return OSK_OK;
		}
	}
	return r;
}

/* ------------------------------------ Werksschluessel ersetzen */

osk_result_t osk_se050_rotate_scp_keys(uint8_t key_version, const osk_scp_keyset_t *current,
				       const osk_scp_keyset_t *replacement)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}
	if (current == NULL || replacement == NULL) {
		return OSK_ERR_ARG;
	}
	/* Der neue Satz darf nicht der alte sein - das waere ein Aufruf, der
	 * aussieht wie Sicherheit und nichts tut. */
	if (memcmp(current, replacement, sizeof(*current)) == 0) {
		return OSK_ERR_ARG;
	}

	osk_scp03_static_keys_t cur;
	osk_scp03_static_keys_t new_keys;
	memcpy(cur.enc, current->enc, OSK_AES_KEY_LEN);
	memcpy(cur.mac, current->mac, OSK_AES_KEY_LEN);
	memcpy(cur.dek, current->dek, OSK_AES_KEY_LEN);
	memcpy(new_keys.enc, replacement->enc, OSK_AES_KEY_LEN);
	memcpy(new_keys.mac, replacement->mac, OSK_AES_KEY_LEN);
	memcpy(new_keys.dek, replacement->dek, OSK_AES_KEY_LEN);

	uint8_t data[1u + 3u * 23u];
	size_t data_len = 0;
	osk_result_t r = osk_scp03_build_put_key_data(&cur, &new_keys, key_version, data,
						      sizeof(data), &data_len);
	osk_wipe(&cur, sizeof(cur));
	osk_wipe(&new_keys, sizeof(new_keys));
	if (r != OSK_OK) {
		osk_wipe(data, sizeof(data));
		return r;
	}

	/* OFFEN UND UNBESTAETIGT, genau wie die Werte in se050_t1_wire.h:
	 *
	 * Die Bausteine des Befehls unten sind beleggenau - Datenfeld nach
	 * GPCS Tabelle 11-67 und Amendment D Tabelle 7-10, Verpacken mit
	 * Key-DEK nach Amendment D 6.2.8, Pruefwert nach 7.2.2. Was NICHT
	 * belegt ist: ob der SE05x PUT KEY fuer die Platform-SCP-Saetze
	 * ueber DIESEN Kanal annimmt. Dieser Treiber baut den
	 * Applet-AESKey-Kanal auf (AN12413 4.5.3, CLA 0x80 INS 0x50 nach der
	 * Applet-Auswahl). Platform SCP liegt eine Ebene darunter, beim
	 * Kartenverwalter, und AN12413 beschreibt nur Applet-Befehle.
	 * AN12973 verweist fuer das Ersetzen auf ein Beispiel in NXPs
	 * Middleware - fremder Code, den wir nicht lesen.
	 *
	 * Zu klaeren ist also die Ebene, nicht der Inhalt: eventuell braucht
	 * es eine eigene Sitzung mit Schluesselfassung 11 statt 00 und ohne
	 * ausgewaehltes Applet. Die SCP03-Schicht kann das - sie ist nicht
	 * auf eine Ebene festgelegt; es fehlt nur die Bestaetigung, welche
	 * es ist. Am Geraet ist das eine Messung: Befehl schicken und sehen,
	 * ob 0x9000 kommt. Gemeldet, steht in docs/se050.md.
	 *
	 * PUT KEY (GPCS 11.8.2): P1 ist die zu ersetzende Schluesselfassung,
	 * b8 = 0 heisst "letzter Befehl". P2 nennt die Kennung des ersten
	 * Schluessels mit gesetztem b8 fuer "mehrere Schluessel" - die drei
	 * kommen in der Reihenfolge ENC, MAC, DEK, wie
	 * osk_scp03_build_put_key_data() sie schreibt.
	 *
	 * Der Befehl geht durch den bestehenden Kanal, ist also schon
	 * verschluesselt; der neue Schluessel ist darin ZUSAETZLICH mit
	 * Key-DEK verpackt (Amendment D 6.2.8). */
	size_t dummy = 0;
	r = osk_se050_apdu_exchange(&g_apdu, 0xD8u, key_version, 0x81u, data, data_len, NULL, 0,
				    &dummy);
	osk_wipe(data, sizeof(data));
	return r;
}

osk_result_t osk_se050_require_scp(void)
{
	if (!g_open) {
		return OSK_ERR_IO;
	}

	/* SetPlatformSCPRequest mit SCP_REQUIRED (AN12413 4.6.2).
	 * Dauerhaft: der Baustein lehnt danach jeden Befehl ab, der nicht
	 * voll verschluesselt und gesichert ankommt. Zuruecknehmen kann das
	 * nur eine Sitzung mit RESERVED_ID_PLATFORM_SCP - also nicht wir im
	 * laufenden Betrieb, und das ist gewollt. */
	uint8_t payload[8];
	osk_tlv_writer_t w;
	osk_tlv_init(&w, payload, sizeof(payload));
	osk_tlv_put_u8(&w, TAG_1, SE050_SCP_REQUIRED);

	size_t pl = 0;
	const osk_result_t r = osk_tlv_done(&w, &pl);
	if (r != OSK_OK) {
		return r;
	}

	size_t dummy = 0;
	return osk_se050_apdu_exchange(&g_apdu, INS_MGMT, P1_DEFAULT, P2_SCP, payload, pl, NULL, 0,
				       &dummy);
}
