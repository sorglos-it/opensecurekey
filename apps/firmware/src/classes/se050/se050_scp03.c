#include <string.h>
#include "se050_scp03.h"

/* Ableitungskonstanten, Amendment D Tabelle 4-1. */
#define DD_CARD_CRYPTOGRAM 0x00u
#define DD_HOST_CRYPTOGRAM 0x01u
#define DD_S_ENC 0x04u
#define DD_S_MAC 0x06u
#define DD_S_RMAC 0x07u

/* Laenge der abgeleiteten Daten in BIT, Amendment D 4.1.5. */
#define DD_L_64 0x0040u	 /* Kryptogramm, 8 Byte  */
#define DD_L_128 0x0080u /* Schluessel, 16 Byte  */

/* CLA mit gesetztem Bit fuer die eigene gesicherte Uebertragung
 * (Amendment D 6.2.4). */
#define CLA_SECURE 0x84u

/* ----------------------------------------------------------- Ableitung */

/* Die "fixed input data" der Ableitung nach Amendment D 4.1.5:
 *   11 Byte 0x00 | Konstante | 0x00 | L (2 Byte) | i (1 Byte) | Kontext
 *
 * Fuer 64 und 128 Bit reicht ein Durchlauf, also ist i immer 0x01. Laengen
 * ueber 128 Bit braucht SCP03 hier nicht: der SE050 nimmt fuer
 * Anmeldeobjekte nur AES-128 (AN12413 3.2.3), und die Kryptogramme sind
 * 8 Byte. Darum gibt es die zweite Runde absichtlich nicht - toter Code
 * waere hier eine Falle, nicht Vorsorge. */
osk_result_t osk_scp03_derivation_data(uint8_t out[32], size_t *out_len, uint8_t constant,
				       uint16_t l_bits, const uint8_t *context, size_t context_len)
{
	if (out == NULL || out_len == NULL) {
		return OSK_ERR_ARG;
	}
	if (context_len > 16u || (context_len > 0 && context == NULL)) {
		return OSK_ERR_ARG;
	}
	memset(out, 0, 11);
	out[11] = constant;
	out[12] = 0x00u;
	out[13] = (uint8_t)(l_bits >> 8);
	out[14] = (uint8_t)(l_bits & 0xFFu);
	out[15] = 0x01u;
	if (context_len > 0) {
		memcpy(&out[16], context, context_len);
	}
	*out_len = 16u + context_len;
	return OSK_OK;
}

/* Einen Schluessel oder ein Kryptogramm ableiten. take ist 16 fuer einen
 * Schluessel und 8 fuer ein Kryptogramm - in beiden Faellen die vorderen
 * Bytes des CMAC-Ergebnisses. */
static osk_result_t derive_one(const uint8_t key[OSK_AES_KEY_LEN], uint8_t constant,
			       uint16_t l_bits, const uint8_t *context, size_t context_len,
			       uint8_t *out, size_t take)
{
	/* 16 Byte Kopf plus der Kontext aus zwei Zufallszahlen. */
	uint8_t dd[32];
	size_t dd_len = 0;

	osk_result_t r =
	    osk_scp03_derivation_data(dd, &dd_len, constant, l_bits, context, context_len);
	if (r != OSK_OK) {
		return r;
	}

	uint8_t mac[OSK_AES_BLOCK];
	r = osk_aes_cmac(key, dd, dd_len, mac);
	osk_wipe(dd, sizeof(dd));
	if (r != OSK_OK) {
		return r;
	}
	memcpy(out, mac, take);
	osk_wipe(mac, sizeof(mac));
	return OSK_OK;
}

osk_result_t osk_scp03_derive(osk_scp03_t *s, const osk_scp03_static_keys_t *keys,
			      const uint8_t host_challenge[SCP03_CHALLENGE_LEN],
			      const uint8_t card_challenge[SCP03_CHALLENGE_LEN],
			      uint8_t card_cryptogram_out[SCP03_CRYPTOGRAM_LEN],
			      uint8_t host_cryptogram_out[SCP03_CRYPTOGRAM_LEN])
{
	if (s == NULL || keys == NULL || host_challenge == NULL || card_challenge == NULL ||
	    card_cryptogram_out == NULL || host_cryptogram_out == NULL) {
		return OSK_ERR_ARG;
	}

	memset(s, 0, sizeof(*s));

	/* Kontext ist beidemal die eigene Zufallszahl, dann die des
	 * Bausteins (Amendment D 6.2.1 und 6.2.2). */
	uint8_t ctx[2u * SCP03_CHALLENGE_LEN];
	memcpy(&ctx[0], host_challenge, SCP03_CHALLENGE_LEN);
	memcpy(&ctx[SCP03_CHALLENGE_LEN], card_challenge, SCP03_CHALLENGE_LEN);

	osk_result_t r =
	    derive_one(keys->enc, DD_S_ENC, DD_L_128, ctx, sizeof(ctx), s->s_enc, sizeof(s->s_enc));
	if (r == OSK_OK) {
		r = derive_one(keys->mac, DD_S_MAC, DD_L_128, ctx, sizeof(ctx), s->s_mac,
			       sizeof(s->s_mac));
	}
	if (r == OSK_OK) {
		r = derive_one(keys->mac, DD_S_RMAC, DD_L_128, ctx, sizeof(ctx), s->s_rmac,
			       sizeof(s->s_rmac));
	}
	/* Beide Kryptogramme haengen am Sitzungsschluessel S-MAC, nicht am
	 * festen Schluessel (Amendment D 6.2.2.2 und 6.2.2.3). */
	if (r == OSK_OK) {
		r = derive_one(s->s_mac, DD_CARD_CRYPTOGRAM, DD_L_64, ctx, sizeof(ctx),
			       card_cryptogram_out, SCP03_CRYPTOGRAM_LEN);
	}
	if (r == OSK_OK) {
		r = derive_one(s->s_mac, DD_HOST_CRYPTOGRAM, DD_L_64, ctx, sizeof(ctx),
			       host_cryptogram_out, SCP03_CRYPTOGRAM_LEN);
	}

	osk_wipe(ctx, sizeof(ctx));

	if (r != OSK_OK) {
		/* Halbe Schluessel sind schlimmer als keine. */
		osk_scp03_close(s);
		osk_wipe(card_cryptogram_out, SCP03_CRYPTOGRAM_LEN);
		osk_wipe(host_cryptogram_out, SCP03_CRYPTOGRAM_LEN);
		return r;
	}

	/* Verkettungswert fuer EXTERNAL AUTHENTICATE: 16 Byte Null
	 * (Amendment D 6.2.3). memset() reicht, das ist kein Geheimnis. */
	memset(s->chain, 0, sizeof(s->chain));
	s->established = false;
	return OSK_OK;
}

void osk_scp03_close(osk_scp03_t *s)
{
	if (s == NULL) {
		return;
	}
	/* Nicht memset(): die Sitzungsschluessel sind das Geheimnis, das hier
	 * wirklich verschwinden muss. */
	osk_wipe(s, sizeof(*s));
}

/* ---------------------------------------------------------- C-MAC */

/* C-MAC nach Amendment D 6.2.4: ueber Verkettungswert, dann den ganzen
 * Befehl mit Kopf und Nutzlast - OHNE Le. Es gelten die vorderen 8 Byte;
 * die vollen 16 werden der neue Verkettungswert.
 *
 * apdu zeigt auf den Befehl mit bereits richtigem CLA und Lc. */
static osk_result_t cmac_over_command(osk_scp03_t *s, const uint8_t *apdu, size_t apdu_len,
				      uint8_t mac_out[OSK_AES_BLOCK])
{
	/* Der groesste Befehl, den der SE050 annimmt, ist 894 Byte
	 * (AN12413 4.3.2); mit 16 Byte Verkettung bleibt das knapp unter 1 kB. */
	uint8_t buf[SCP03_CHAIN_LEN + 1024u];

	if (apdu_len > sizeof(buf) - SCP03_CHAIN_LEN) {
		return OSK_ERR_FULL;
	}
	memcpy(buf, s->chain, SCP03_CHAIN_LEN);
	memcpy(&buf[SCP03_CHAIN_LEN], apdu, apdu_len);

	const osk_result_t r = osk_aes_cmac(s->s_mac, buf, SCP03_CHAIN_LEN + apdu_len, mac_out);
	osk_wipe(buf, SCP03_CHAIN_LEN + apdu_len);
	return r;
}

/* --------------------------------------------- EXTERNAL AUTHENTICATE */

osk_result_t osk_scp03_build_external_auth(osk_scp03_t *s,
					   const uint8_t host_cryptogram[SCP03_CRYPTOGRAM_LEN],
					   uint8_t *apdu, size_t cap, size_t *len_out)
{
	if (s == NULL || host_cryptogram == NULL || apdu == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (s->established) {
		/* Zweimal anmelden gibt es nicht - das waere ein Weg, den
		 * Zaehler zurueckzudrehen. */
		return OSK_ERR_EXISTS;
	}

	/* Kopf 5 + 8 Byte Kryptogramm + 8 Byte MAC (Card Specification D.4.2). */
	const size_t total = 5u + SCP03_CRYPTOGRAM_LEN + SCP03_MAC_LEN;
	if (cap < total) {
		return OSK_ERR_FULL;
	}

	apdu[0] = CLA_SECURE;
	apdu[1] = 0x82u; /* EXTERNAL AUTHENTICATE */
	apdu[2] = SCP03_SEC_LEVEL_FULL;
	apdu[3] = 0x00u;
	apdu[4] = (uint8_t)(SCP03_CRYPTOGRAM_LEN + SCP03_MAC_LEN);
	memcpy(&apdu[5], host_cryptogram, SCP03_CRYPTOGRAM_LEN);

	/* Der MAC laeuft ueber Kopf und Kryptogramm, noch ohne den MAC
	 * selbst - Lc steht aber schon auf dem Endwert. */
	uint8_t mac[OSK_AES_BLOCK];
	const osk_result_t r = cmac_over_command(s, apdu, 5u + SCP03_CRYPTOGRAM_LEN, mac);
	if (r != OSK_OK) {
		osk_wipe(apdu, total);
		return r;
	}
	memcpy(&apdu[5 + SCP03_CRYPTOGRAM_LEN], mac, SCP03_MAC_LEN);

	/* Ab jetzt ist der volle C-MAC der Verkettungswert. */
	memcpy(s->chain, mac, SCP03_CHAIN_LEN);
	osk_wipe(mac, sizeof(mac));

	/* Zaehler beginnt bei 1 fuer den ersten Befehl danach
	 * (Amendment D 6.2.6). */
	memset(s->counter, 0, sizeof(s->counter));
	s->counter[OSK_AES_BLOCK - 1u] = 1u;

	s->established = true;
	*len_out = total;
	return OSK_OK;
}

/* ------------------------------------------------------ Zaehler und ICV */

static void counter_inc(uint8_t c[OSK_AES_BLOCK])
{
	for (size_t i = OSK_AES_BLOCK; i-- > 0;) {
		if (++c[i] != 0u) {
			return;
		}
		/* Ueberlauf: naechstes Byte weiter. Nach 2^128 Befehlen waere
		 * der Zaehler rund - das erreicht kein Geraet. */
	}
}

/* Startwert der CBC-Kette: der Zaehlerblock mit S-ENC verschluesselt.
 * Fuer die Antwort wird vorher das hoechstwertige Byte auf 0x80 gesetzt,
 * damit Befehl und Antwort nie denselben Startwert benutzen
 * (Amendment D 6.2.7). */
static osk_result_t make_icv(const osk_scp03_t *s, bool for_response, uint8_t icv[OSK_AES_BLOCK])
{
	uint8_t block[OSK_AES_BLOCK];
	memcpy(block, s->counter, sizeof(block));
	if (for_response) {
		block[0] = 0x80u;
	}
	const osk_result_t r = osk_aes_ecb_encrypt_block(s->s_enc, block, icv);
	osk_wipe(block, sizeof(block));
	return r;
}

/* ------------------------------------------------------------- Einpacken */

osk_result_t osk_scp03_wrap(osk_scp03_t *s, const uint8_t *in, size_t in_len, uint8_t *out,
			    size_t cap, size_t *len_out)
{
	if (s == NULL || in == NULL || out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (!s->established) {
		return OSK_ERR_DENIED;
	}
	/* Kopf 4 Byte, dann entweder nichts oder Lc und die Nutzlast. */
	if (in_len != 4u && in_len < 5u) {
		return OSK_ERR_ARG;
	}

	const uint8_t *data = NULL;
	size_t data_len = 0;
	if (in_len > 4u) {
		data_len = in[4];
		if (in_len != 5u + data_len) {
			/* Lc passt nicht zur Laenge: entweder ist ein Le
			 * mitgekommen oder der Aufrufer hat sich verrechnet.
			 * Beides darf nicht stillschweigend durchgehen. */
			return OSK_ERR_ARG;
		}
		data = &in[5];
	}

	/* Nutzlast auffuellen: ISO 9797-1 Verfahren 2, also 0x80 und dann
	 * Nullen bis zur Blockgrenze. Das gilt immer, auch wenn die Laenge
	 * schon passt - sonst waere nicht erkennbar, wo der Klartext endet. */
	size_t padded = 0;
	if (data_len > 0) {
		padded = ((data_len + 1u + OSK_AES_BLOCK - 1u) / OSK_AES_BLOCK) * OSK_AES_BLOCK;
	}

	const size_t lcc = padded + SCP03_MAC_LEN;
	if (lcc > 0xFFu) {
		/* Mehr geht in ein Lc-Byte nicht. Laenger wird in dieser
		 * Fassung nicht gebraucht; der SE050 kann zwar erweiterte
		 * Laengen, aber dafuer waere auch der Kopf anders. */
		return OSK_ERR_FULL;
	}
	const size_t total = 5u + lcc + 1u; /* + Le */
	if (cap < total) {
		return OSK_ERR_FULL;
	}

	uint8_t icv[OSK_AES_BLOCK];
	uint8_t plain[1024];
	osk_result_t r = OSK_OK;

	out[0] = CLA_SECURE;
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
	out[4] = (uint8_t)lcc;

	if (data_len > 0) {
		if (padded > sizeof(plain)) {
			return OSK_ERR_FULL;
		}
		memcpy(plain, data, data_len);
		plain[data_len] = 0x80u;
		if (padded > data_len + 1u) {
			memset(&plain[data_len + 1u], 0, padded - data_len - 1u);
		}

		r = make_icv(s, false, icv);
		if (r == OSK_OK) {
			r = osk_aes_cbc_encrypt(s->s_enc, icv, plain, &out[5], padded);
		}
		osk_wipe(plain, padded);
		osk_wipe(icv, sizeof(icv));
		if (r != OSK_OK) {
			osk_wipe(out, total);
			return r;
		}
	}

	/* MAC ueber Kopf und bereits verschluesselte Nutzlast
	 * (Amendment D 6.2.6: erst chiffrieren, dann signieren). */
	uint8_t mac[OSK_AES_BLOCK];
	r = cmac_over_command(s, out, 5u + padded, mac);
	if (r != OSK_OK) {
		osk_wipe(out, total);
		return r;
	}
	memcpy(&out[5 + padded], mac, SCP03_MAC_LEN);
	memcpy(s->chain, mac, SCP03_CHAIN_LEN);
	osk_wipe(mac, sizeof(mac));

	/* Le: der SE050 erwartet es bei allen Befehlen, die etwas
	 * zurueckgeben, und stoert sich sonst nicht daran
	 * (AN12413 4.1.2). */
	out[total - 1u] = 0x00u;

	/* Zaehler weiter - auch wenn nichts zu verschluesseln war
	 * (Amendment D 6.2.6). */
	counter_inc(s->counter);

	*len_out = total;
	return OSK_OK;
}

/* ------------------------------------------------------------ Auspacken */

osk_result_t osk_scp03_unwrap(osk_scp03_t *s, const uint8_t *in, size_t in_len, uint8_t *out,
			      size_t cap, size_t *len_out, uint16_t *sw_out)
{
	if (s == NULL || in == NULL || len_out == NULL || sw_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (!s->established) {
		return OSK_ERR_DENIED;
	}
	if (in_len < 2u) {
		return OSK_ERR_FORMAT;
	}

	*len_out = 0;
	const uint16_t sw = (uint16_t)(((uint16_t)in[in_len - 2u] << 8) | in[in_len - 1u]);
	*sw_out = sw;

	/* Fehlerstatus: nach Amendment D 6.2.5 kommen dann genau die zwei
	 * Statusbytes, ohne MAC und ohne Verschluesselung. Warnungen (62xx,
	 * 63xx) und 9000 gelten dabei nicht als Fehler. */
	const bool warn = ((sw & 0xFF00u) == 0x6200u) || ((sw & 0xFF00u) == 0x6300u);
	if (sw != 0x9000u && !warn) {
		if (in_len != 2u) {
			/* Fehlerstatus MIT Nutzlast gibt es nicht. Wer das
			 * schickt, haelt sich nicht an das Protokoll. */
			return OSK_ERR_FORMAT;
		}
		return OSK_OK;
	}

	if (in_len < 2u + SCP03_MAC_LEN) {
		return OSK_ERR_FORMAT;
	}
	const size_t body_len = in_len - 2u - SCP03_MAC_LEN;
	const uint8_t *body = in;
	const uint8_t *rmac = &in[body_len];

	/* R-MAC nach Amendment D 6.2.5: Verkettungswert, Nutzlast, Status.
	 * Der Verkettungswert ist derselbe wie beim letzten Befehl - R-MAC
	 * hat einen eigenen Schluessel, darum darf er ihn mitbenutzen. */
	uint8_t buf[SCP03_CHAIN_LEN + 1024u + 2u];
	if (body_len > sizeof(buf) - SCP03_CHAIN_LEN - 2u) {
		return OSK_ERR_FULL;
	}
	memcpy(buf, s->chain, SCP03_CHAIN_LEN);
	if (body_len > 0) {
		memcpy(&buf[SCP03_CHAIN_LEN], body, body_len);
	}
	buf[SCP03_CHAIN_LEN + body_len] = (uint8_t)(sw >> 8);
	buf[SCP03_CHAIN_LEN + body_len + 1u] = (uint8_t)(sw & 0xFFu);

	uint8_t mac[OSK_AES_BLOCK];
	osk_result_t r = osk_aes_cmac(s->s_rmac, buf, SCP03_CHAIN_LEN + body_len + 2u, mac);
	osk_wipe(buf, SCP03_CHAIN_LEN + body_len + 2u);
	if (r != OSK_OK) {
		return r;
	}

	/* In gleichbleibender Zeit vergleichen. */
	const int bad = osk_memeq_ct(mac, rmac, SCP03_MAC_LEN);
	osk_wipe(mac, sizeof(mac));
	if (bad != 0) {
		return OSK_ERR_DENIED;
	}

	if (body_len == 0) {
		return OSK_OK;
	}
	if ((body_len % OSK_AES_BLOCK) != 0u) {
		return OSK_ERR_FORMAT;
	}

	/* Entschluesseln. Der Startwert gehoert zum Zaehler DES BEFEHLS, also
	 * zu dem Wert vor dem Weiterzaehlen in wrap(). */
	uint8_t icv[OSK_AES_BLOCK];
	uint8_t plain[1024];
	if (body_len > sizeof(plain)) {
		return OSK_ERR_FULL;
	}

	uint8_t saved[OSK_AES_BLOCK];
	memcpy(saved, s->counter, sizeof(saved));
	/* wrap() hat schon hochgezaehlt - fuer den Startwert der Antwort
	 * zaehlt der Wert des Befehls, also einer zurueck. */
	for (size_t i = OSK_AES_BLOCK; i-- > 0;) {
		if (s->counter[i]-- != 0u) {
			break;
		}
	}
	r = make_icv(s, true, icv);
	memcpy(s->counter, saved, sizeof(saved));
	osk_wipe(saved, sizeof(saved));

	if (r == OSK_OK) {
		r = osk_aes_cbc_decrypt(s->s_enc, icv, body, plain, body_len);
	}
	osk_wipe(icv, sizeof(icv));
	if (r != OSK_OK) {
		osk_wipe(plain, sizeof(plain));
		return r;
	}

	/* Auffuellung nach ISO 9797-1 Verfahren 2 abtrennen: von hinten bis
	 * zum 0x80. Alles danach muss Null sein. */
	size_t n = body_len;
	while (n > 0 && plain[n - 1u] == 0x00u) {
		n--;
	}
	if (n == 0 || plain[n - 1u] != 0x80u) {
		osk_wipe(plain, sizeof(plain));
		return OSK_ERR_FORMAT;
	}
	n--; /* das 0x80 gehoert nicht zum Klartext */

	if (n > cap) {
		osk_wipe(plain, sizeof(plain));
		return OSK_ERR_FULL;
	}
	if (n > 0) {
		if (out == NULL) {
			osk_wipe(plain, sizeof(plain));
			return OSK_ERR_ARG;
		}
		memcpy(out, plain, n);
	}
	osk_wipe(plain, sizeof(plain));

	*len_out = n;
	return OSK_OK;
}

/* --------------------------------------------- Schluessel ersetzen */

osk_result_t osk_scp03_key_check_value(const uint8_t key[OSK_AES_KEY_LEN],
				       uint8_t kcv_out[SCP03_KCV_LEN])
{
	if (key == NULL || kcv_out == NULL) {
		return OSK_ERR_ARG;
	}

	/* Amendment D 7.2.2: einen Block aus 16 Byte 0x01 verschluesseln,
	 * die drei vordersten Bytes sind der Pruefwert. */
	uint8_t block[OSK_AES_BLOCK];
	memset(block, 0x01u, sizeof(block));

	uint8_t out[OSK_AES_BLOCK];
	const osk_result_t r = osk_aes_ecb_encrypt_block(key, block, out);
	if (r != OSK_OK) {
		return r;
	}
	memcpy(kcv_out, out, SCP03_KCV_LEN);
	osk_wipe(out, sizeof(out));
	return OSK_OK;
}

/* Einen einzelnen Schluessel in das PUT KEY-Datenfeld schreiben.
 * Aufbau nach Amendment D Tabelle 7-10:
 *   0x88 (AES) | 0x11 (Laenge der Schluesseldaten) | 0x10 (Schluessellaenge)
 *   | 16 Byte Chiffrat | 0x03 | 3 Byte Pruefwert  =  23 Byte. */
#define PUT_KEY_ENTRY_LEN (1u + 1u + 1u + OSK_AES_KEY_LEN + 1u + SCP03_KCV_LEN)

static osk_result_t put_key_entry(const uint8_t dek[OSK_AES_KEY_LEN],
				  const uint8_t key[OSK_AES_KEY_LEN], uint8_t *out)
{
	/* Der neue Schluessel wird mit Key-DEK des ALTEN Satzes
	 * verschluesselt, AES-CBC mit Startwert Null, ohne Auffuellen - ein
	 * 16-Byte-Schluessel ist schon genau ein Block
	 * (Amendment D 6.2.8). */
	static const uint8_t zero_iv[OSK_AES_BLOCK] = {0};

	out[0] = 0x88u;			   /* Schluesselart: AES */
	out[1] = 0x11u;			   /* 1 + 16 Byte Schluesseldaten */
	out[2] = (uint8_t)OSK_AES_KEY_LEN; /* 0x10 */

	osk_result_t r = osk_aes_cbc_encrypt(dek, zero_iv, key, &out[3], OSK_AES_KEY_LEN);
	if (r != OSK_OK) {
		return r;
	}

	out[3 + OSK_AES_KEY_LEN] = SCP03_KCV_LEN;
	r = osk_scp03_key_check_value(key, &out[3 + OSK_AES_KEY_LEN + 1u]);
	if (r != OSK_OK) {
		osk_wipe(out, PUT_KEY_ENTRY_LEN);
		return r;
	}
	return OSK_OK;
}

osk_result_t osk_scp03_build_put_key_data(const osk_scp03_static_keys_t *current,
					  const osk_scp03_static_keys_t *replacement,
					  uint8_t new_key_version, uint8_t *out, size_t cap,
					  size_t *len_out)
{
	if (current == NULL || replacement == NULL || out == NULL || len_out == NULL) {
		return OSK_ERR_ARG;
	}
	/* GPCS 11.8.2.1: die Schluesselfassung ist 0x01 bis 0x7F. 0x00 waere
	 * "neuen Satz anlegen" - das wollen wir hier nie, wir ERSETZEN. */
	if (new_key_version == 0x00u || new_key_version > 0x7Fu) {
		return OSK_ERR_ARG;
	}

	const size_t total = 1u + 3u * PUT_KEY_ENTRY_LEN;
	if (cap < total) {
		return OSK_ERR_FULL;
	}

	out[0] = new_key_version;

	/* Reihenfolge ENC, MAC, DEK: P2 nennt die Kennung des ERSTEN
	 * Schluessels, die folgenden zaehlen von dort hoch
	 * (GPCS 11.8.2.2). Die Reihenfolge muss also zu der Kennung passen,
	 * die se050.c in P2 setzt. */
	const uint8_t *keys[3] = {
	    replacement->enc,
	    replacement->mac,
	    replacement->dek,
	};
	for (size_t i = 0; i < 3u; i++) {
		const osk_result_t r =
		    put_key_entry(current->dek, keys[i], &out[1u + i * PUT_KEY_ENTRY_LEN]);
		if (r != OSK_OK) {
			osk_wipe(out, total);
			return r;
		}
	}

	*len_out = total;
	return OSK_OK;
}
