/* Umsetzung von wallet_apdu.h. Selbst geschrieben nach der oeffentlich
 * beschriebenen Form der Befehle; kein uebernommener Code. */

#include <string.h>
#include "wallet_apdu.h"
#include "wallet_eth.h"
#include "wallet_psbt.h"
#include "wallet_seed.h"
#include "wallet_codec.h"
#include "wallet_bip32.h"
#include "osk_crypto.h"

/* Fassung dieser Schicht - steht in der Antwort auf GET_CONFIG. */
#define APDU_VERSION_MAJOR 0u
#define APDU_VERSION_MINOR 0u
#define APDU_VERSION_PATCH 1u

void osk_apdu_init(osk_apdu_state_t *state)
{
	if (state == NULL)
		return;
	memset(state, 0, sizeof(*state));
	state->btc_hrp = "bc";
	state->btc_p2pkh_version = 0x00u;
	state->btc_p2sh_version = 0x05u;
	state->eth_unit = "ETH";
}

static void forget(osk_apdu_state_t *state)
{
	osk_wipe(state->assembly, sizeof(state->assembly));
	state->assembly_len = 0;
	state->pending_ins = 0;
	state->message_expected = 0;
	memset(state->path, 0, sizeof(state->path));
}

osk_result_t osk_apdu_read_path(const uint8_t *in, size_t len, char *out, size_t cap,
				size_t *used_out)
{
	unsigned count;
	size_t used = 0;

	if (in == NULL || out == NULL || used_out == NULL || cap < 2u)
		return OSK_ERR_ARG;
	if (len < 1u)
		return OSK_ERR_FORMAT;
	count = in[0];
	if (count > OSK_BIP32_MAX_DEPTH)
		return OSK_ERR_FULL;
	if (len < 1u + 4u * count)
		return OSK_ERR_FORMAT;

	out[used++] = 'm';
	for (unsigned i = 0; i < count; i++) {
		const uint8_t *p = in + 1u + 4u * i;
		uint32_t step = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
				((uint32_t)p[2] << 8) | (uint32_t)p[3];
		uint32_t value = step & ~OSK_BIP32_HARDENED;
		char digits[12];
		size_t digit_count = 0;

		if (used + 14u >= cap)
			return OSK_ERR_ARG;
		out[used++] = '/';
		do {
			digits[digit_count++] = (char)('0' + (char)(value % 10u));
			value /= 10u;
		} while (value > 0 && digit_count < sizeof(digits));
		while (digit_count > 0)
			out[used++] = digits[--digit_count];
		if ((step & OSK_BIP32_HARDENED) != 0)
			out[used++] = '\'';
	}
	out[used] = '\0';
	*used_out = 1u + 4u * count;
	return OSK_OK;
}

/* --- Einzelne Befehle ---------------------------------------------------- */

static uint16_t sw_for(osk_result_t res)
{
	switch (res) {
	case OSK_OK:
		return OSK_SW_OK;
	case OSK_ERR_DENIED:
		return OSK_SW_DENIED;
	case OSK_ERR_LOCKED:
		return OSK_SW_LOCKED;
	case OSK_ERR_ARG:
	case OSK_ERR_FORMAT:
	case OSK_ERR_UNSUPPORTED:
		return OSK_SW_WRONG_DATA;
	case OSK_ERR_FULL:
		return OSK_SW_WRONG_LENGTH;
	default:
		return OSK_SW_BUSY;
	}
}

static uint16_t cmd_get_pubkey(osk_apdu_state_t *state, const uint8_t *data, size_t len,
			       bool with_chaincode, uint8_t *out, size_t cap, size_t *out_len)
{
	char path[128];
	size_t used = 0, written = 0, point_len = 0;
	osk_bip32_priv_t key;
	osk_ec_point_t point;
	uint8_t raw[OSK_EC_PUB_UNCOMPRESSED];
	uint8_t address[OSK_ETH_ADDRESS_LEN];
	char text[64];
	osk_result_t res;

	res = osk_apdu_read_path(data, len, path, sizeof(path), &used);
	if (res != OSK_OK)
		return sw_for(res);
	res = osk_wallet_seed_derive(state->seed_id, "apdu", OSK_BIP32_SECP256K1, path, &key);
	if (res != OSK_OK)
		return sw_for(res);
	res = osk_ec_public_from_private(key.key, &point);
	if (res == OSK_OK)
		res = osk_ec_point_encode(&point, false, raw, sizeof(raw), &point_len);
	if (res == OSK_OK)
		res = osk_eth_address(&key, address);
	if (res == OSK_OK)
		res = osk_eth_address_text(address, text, sizeof(text));
	if (res != OSK_OK) {
		osk_bip32_wipe(&key);
		return sw_for(res);
	}

	/* Antwort: Laenge des Punktes, Punkt, Laenge der Adresse, Adresse
	 * als Text OHNE "0x", dann auf Wunsch die Kettenzahl. */
	if (cap < 1u + point_len + 1u + 40u + (with_chaincode ? 32u : 0u)) {
		osk_bip32_wipe(&key);
		return OSK_SW_WRONG_LENGTH;
	}
	out[written++] = (uint8_t)point_len;
	memcpy(out + written, raw, point_len);
	written += point_len;
	out[written++] = 40u;
	memcpy(out + written, text + 2, 40);
	written += 40u;
	if (with_chaincode) {
		memcpy(out + written, key.chain, 32);
		written += 32u;
	}
	*out_len = written;
	osk_bip32_wipe(&key);
	osk_wipe(raw, sizeof(raw));
	return OSK_SW_OK;
}

static uint16_t cmd_sign_tx(osk_apdu_state_t *state, uint8_t *out, size_t cap, size_t *out_len)
{
	uint8_t r[32], s[32];
	uint64_t v = 0;
	osk_result_t res;

	if (cap < 65u)
		return OSK_SW_WRONG_LENGTH;
	res = osk_eth_sign_tx(state->assembly, state->assembly_len, state->seed_id, "apdu",
			      state->path, state->eth_unit, state->allow_blind, r, s, &v);
	if (res != OSK_OK)
		return sw_for(res);
	/* Antwort: v, r, s. v wird als ein Byte gesendet; passt es nicht
	 * hinein (sehr grosse Kettennummer), bleibt der Rechner mit der
	 * Kettennummer in der Hand und rechnet selbst - darum werden die
	 * unteren acht Bit gesendet, so wie es die Gegenstellen erwarten. */
	out[0] = (uint8_t)(v & 0xFFu);
	memcpy(out + 1, r, 32);
	memcpy(out + 33, s, 32);
	*out_len = 65u;
	osk_wipe(r, sizeof(r));
	osk_wipe(s, sizeof(s));
	return OSK_SW_OK;
}

static uint16_t cmd_sign_message(osk_apdu_state_t *state, uint8_t *out, size_t cap, size_t *out_len)
{
	uint8_t r[32], s[32];
	uint64_t v = 0;
	osk_result_t res;

	if (cap < 65u)
		return OSK_SW_WRONG_LENGTH;
	res = osk_eth_sign_message(state->assembly, state->assembly_len, state->seed_id, "apdu",
				   state->path, state->allow_blind, r, s, &v);
	if (res != OSK_OK)
		return sw_for(res);
	out[0] = (uint8_t)(v & 0xFFu);
	memcpy(out + 1, r, 32);
	memcpy(out + 33, s, 32);
	*out_len = 65u;
	osk_wipe(r, sizeof(r));
	osk_wipe(s, sizeof(s));
	return OSK_SW_OK;
}

static uint16_t cmd_sign_typed(osk_apdu_state_t *state, const uint8_t *data, size_t len,
			       uint8_t *out, size_t cap, size_t *out_len)
{
	char path[128];
	size_t used = 0;
	uint8_t r[32], s[32];
	uint64_t v = 0;
	osk_result_t res;

	if (cap < 65u)
		return OSK_SW_WRONG_LENGTH;
	res = osk_apdu_read_path(data, len, path, sizeof(path), &used);
	if (res != OSK_OK)
		return sw_for(res);
	if (len != used + 64u)
		return OSK_SW_WRONG_LENGTH;
	res = osk_eth_sign_typed(data + used, data + used + 32u, state->seed_id, "apdu", path,
				 state->allow_blind, r, s, &v);
	if (res != OSK_OK)
		return sw_for(res);
	out[0] = (uint8_t)(v & 0xFFu);
	memcpy(out + 1, r, 32);
	memcpy(out + 33, s, 32);
	*out_len = 65u;
	osk_wipe(r, sizeof(r));
	osk_wipe(s, sizeof(s));
	return OSK_SW_OK;
}

static uint16_t cmd_sign_psbt(osk_apdu_state_t *state, uint8_t *out, size_t cap, size_t *out_len)
{
	osk_psbt_t psbt;
	size_t signed_count = 0;
	osk_result_t res;

	res = osk_psbt_parse(state->assembly, state->assembly_len, &psbt);
	if (res != OSK_OK)
		return sw_for(res);
	res = osk_psbt_sign(&psbt, state->seed_id, "apdu", state->btc_hrp, state->btc_p2pkh_version,
			    state->btc_p2sh_version, state->allow_blind, out, cap, out_len,
			    &signed_count);
	if (res != OSK_OK)
		return sw_for(res);
	if (signed_count == 0)
		/* Nichts gehoerte uns. Das ist kein Fehler des Rechners,
		 * aber er soll es merken. */
		return OSK_SW_DENIED;
	return OSK_SW_OK;
}

/* --- Verteiler ----------------------------------------------------------- */

osk_result_t osk_apdu_handle(osk_apdu_state_t *state, const uint8_t *in, size_t in_len,
			     uint8_t *out, size_t cap, size_t *out_len, uint16_t *sw_out)
{
	uint8_t ins, p1, p2;
	const uint8_t *data;
	size_t data_len;

	if (state == NULL || in == NULL || out == NULL || out_len == NULL || sw_out == NULL)
		return OSK_ERR_ARG;
	*out_len = 0;
	*sw_out = OSK_SW_WRONG_LENGTH;

	if (in_len < 4u)
		return OSK_OK;
	if (in[0] != OSK_APDU_CLA) {
		*sw_out = OSK_SW_CLA_UNKNOWN;
		return OSK_OK;
	}
	ins = in[1];
	p1 = in[2];
	p2 = in[3];
	if (in_len == 4u) {
		data = NULL;
		data_len = 0;
	} else {
		size_t lc = in[4];

		if (in_len < 5u + lc)
			return OSK_OK; /* Laengenangabe passt nicht zur APDU */
		data = in + 5;
		data_len = lc;
	}

	switch (ins) {
	case OSK_APDU_INS_GET_CONFIG:
		if (cap < 4u)
			return OSK_OK;
		/* Erstes Byte sind Eigenschaften; 0 heisst: keine
		 * Besonderheiten, blind nur nach Freigabe am Geraet. */
		out[0] = state->allow_blind ? 0x01u : 0x00u;
		out[1] = APDU_VERSION_MAJOR;
		out[2] = APDU_VERSION_MINOR;
		out[3] = APDU_VERSION_PATCH;
		*out_len = 4u;
		*sw_out = OSK_SW_OK;
		return OSK_OK;

	case OSK_APDU_INS_GET_PUBKEY:
		if (data == NULL) {
			*sw_out = OSK_SW_WRONG_LENGTH;
			return OSK_OK;
		}
		*sw_out = cmd_get_pubkey(state, data, data_len, p2 == 0x01u, out, cap, out_len);
		return OSK_OK;

	case OSK_APDU_INS_SIGN_TX:
	case OSK_APDU_INS_SIGN_MESSAGE:
	case OSK_APDU_INS_SIGN_PSBT: {
		bool first = (p1 == 0x00u);
		size_t offset = 0;

		if (data == NULL) {
			*sw_out = OSK_SW_WRONG_LENGTH;
			return OSK_OK;
		}
		if (!first && p1 != 0x80u) {
			*sw_out = OSK_SW_WRONG_DATA;
			return OSK_OK;
		}
		if (first) {
			forget(state);
			state->pending_ins = ins;
			if (ins != OSK_APDU_INS_SIGN_PSBT) {
				size_t used = 0;
				osk_result_t res = osk_apdu_read_path(data, data_len, state->path,
								      sizeof(state->path), &used);

				if (res != OSK_OK) {
					forget(state);
					*sw_out = sw_for(res);
					return OSK_OK;
				}
				offset = used;
			}
			if (ins == OSK_APDU_INS_SIGN_MESSAGE) {
				/* Danach kommt die Laenge der Nachricht,
				 * vier Byte hoechstwertiges Byte zuerst. */
				if (data_len < offset + 4u) {
					forget(state);
					*sw_out = OSK_SW_WRONG_LENGTH;
					return OSK_OK;
				}
				state->message_expected = ((size_t)data[offset] << 24) |
							  ((size_t)data[offset + 1u] << 16) |
							  ((size_t)data[offset + 2u] << 8) |
							  (size_t)data[offset + 3u];
				offset += 4u;
				if (state->message_expected > OSK_APDU_ASSEMBLY_MAX) {
					forget(state);
					*sw_out = OSK_SW_WRONG_LENGTH;
					return OSK_OK;
				}
			}
		} else if (state->pending_ins != ins) {
			/* Ein Folgestueck ohne Anfang, oder zu einem anderen
			 * Befehl: beides wird verworfen, nicht geraten. */
			forget(state);
			*sw_out = OSK_SW_WRONG_DATA;
			return OSK_OK;
		}

		if (state->assembly_len + (data_len - offset) > OSK_APDU_ASSEMBLY_MAX) {
			forget(state);
			*sw_out = OSK_SW_WRONG_LENGTH;
			return OSK_OK;
		}
		memcpy(state->assembly + state->assembly_len, data + offset, data_len - offset);
		state->assembly_len += data_len - offset;

		/* Wann ist alles da? Zwei Wege, und beide muessen eindeutig
		 * sein - raten waere hier gefaehrlich:
		 *  - P2 = 0x01 heisst ausdruecklich "es kommt noch mehr"
		 *  - bei einer Nachricht ist zusaetzlich die angekuendigte
		 *    Laenge die Grenze; weniger heisst warten, mehr ist ein
		 *    Fehler. */
		if (ins == OSK_APDU_INS_SIGN_MESSAGE) {
			if (state->assembly_len > state->message_expected) {
				forget(state);
				*sw_out = OSK_SW_WRONG_LENGTH;
				return OSK_OK;
			}
			if (state->assembly_len < state->message_expected) {
				*sw_out = OSK_SW_OK;
				return OSK_OK;
			}
		} else if (p2 == 0x01u) {
			*sw_out = OSK_SW_OK;
			return OSK_OK;
		}

		if (ins == OSK_APDU_INS_SIGN_TX)
			*sw_out = cmd_sign_tx(state, out, cap, out_len);
		else if (ins == OSK_APDU_INS_SIGN_MESSAGE)
			*sw_out = cmd_sign_message(state, out, cap, out_len);
		else
			*sw_out = cmd_sign_psbt(state, out, cap, out_len);
		forget(state);
		return OSK_OK;
	}

	case OSK_APDU_INS_SIGN_TYPED:
		if (data == NULL) {
			*sw_out = OSK_SW_WRONG_LENGTH;
			return OSK_OK;
		}
		*sw_out = cmd_sign_typed(state, data, data_len, out, cap, out_len);
		return OSK_OK;

	default:
		(void)p1;
		*sw_out = OSK_SW_INS_UNKNOWN;
		return OSK_OK;
	}
}
