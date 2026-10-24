/* Umsetzung von wallet_btc.h. Selbst geschrieben gegen BIP143 und das
 * Transaktionsformat. */

#include <string.h>
#include "wallet_btc.h"
#include "wallet_hash.h"
#include "wallet_codec.h"
#include "osk_crypto.h"

/* --- Zahlen lesen und schreiben ----------------------------------------- */

bool osk_btc_read_u32(const uint8_t *p, size_t len, size_t *pos, uint32_t *out)
{
	if (*pos + 4u > len)
		return false;
	*out = (uint32_t)p[*pos] | ((uint32_t)p[*pos + 1u] << 8) | ((uint32_t)p[*pos + 2u] << 16) |
	       ((uint32_t)p[*pos + 3u] << 24);
	*pos += 4u;
	return true;
}

bool osk_btc_read_u64(const uint8_t *p, size_t len, size_t *pos, uint64_t *out)
{
	uint64_t value = 0;

	if (*pos + 8u > len)
		return false;
	for (unsigned i = 0; i < 8; i++)
		value |= (uint64_t)p[*pos + i] << (8u * i);
	*pos += 8u;
	*out = value;
	return true;
}

bool osk_btc_read_varint(const uint8_t *p, size_t len, size_t *pos, uint64_t *out)
{
	uint8_t first;

	if (*pos >= len)
		return false;
	first = p[*pos];
	*pos += 1u;
	if (first < 0xFDu) {
		*out = first;
		return true;
	}
	if (first == 0xFDu) {
		if (*pos + 2u > len)
			return false;
		*out = (uint64_t)p[*pos] | ((uint64_t)p[*pos + 1u] << 8);
		*pos += 2u;
		/* Kuerzeste Form verlangt: 0xFD mit Wert unter 0xFD waere ein
		 * zweiter Weg, dieselbe Zahl zu schreiben. */
		return *out >= 0xFDu;
	}
	if (first == 0xFEu) {
		uint32_t value = 0;

		if (!osk_btc_read_u32(p, len, pos, &value))
			return false;
		*out = value;
		return value > 0xFFFFu;
	}
	if (!osk_btc_read_u64(p, len, pos, out))
		return false;
	return *out > 0xFFFFFFFFu;
}

size_t osk_btc_write_varint(uint64_t value, uint8_t *out)
{
	if (value < 0xFDu) {
		out[0] = (uint8_t)value;
		return 1;
	}
	if (value <= 0xFFFFu) {
		out[0] = 0xFDu;
		out[1] = (uint8_t)(value & 0xFFu);
		out[2] = (uint8_t)((value >> 8) & 0xFFu);
		return 3;
	}
	if (value <= 0xFFFFFFFFu) {
		out[0] = 0xFEu;
		for (unsigned i = 0; i < 4; i++)
			out[1u + i] = (uint8_t)((value >> (8u * i)) & 0xFFu);
		return 5;
	}
	out[0] = 0xFFu;
	for (unsigned i = 0; i < 8; i++)
		out[1u + i] = (uint8_t)((value >> (8u * i)) & 0xFFu);
	return 9;
}

/* Ein kleiner Schreiber mit Laengenpruefung - jedes Anhaengen prueft, ob es
 * noch passt, und merkt sich einen Ueberlauf statt ihn zu verschlucken. */
typedef struct {
	uint8_t *buf;
	size_t cap;
	size_t len;
	bool overflow;
} writer_t;

static void w_bytes(writer_t *w, const uint8_t *p, size_t len)
{
	if (w->overflow || w->len + len > w->cap) {
		w->overflow = true;
		return;
	}
	if (len > 0)
		memcpy(w->buf + w->len, p, len);
	w->len += len;
}

static void w_u32(writer_t *w, uint32_t v)
{
	uint8_t raw[4];

	for (unsigned i = 0; i < 4; i++)
		raw[i] = (uint8_t)((v >> (8u * i)) & 0xFFu);
	w_bytes(w, raw, sizeof(raw));
}

static void w_u64(writer_t *w, uint64_t v)
{
	uint8_t raw[8];

	for (unsigned i = 0; i < 8; i++)
		raw[i] = (uint8_t)((v >> (8u * i)) & 0xFFu);
	w_bytes(w, raw, sizeof(raw));
}

static void w_varint(writer_t *w, uint64_t v)
{
	uint8_t raw[9];
	size_t len = osk_btc_write_varint(v, raw);

	w_bytes(w, raw, len);
}

/* --- Transaktion lesen und schreiben ------------------------------------ */

osk_result_t osk_btc_parse_tx(const uint8_t *raw, size_t len, osk_btc_tx_t *out)
{
	size_t pos = 0;
	uint64_t count = 0;

	if (raw == NULL || out == NULL)
		return OSK_ERR_ARG;
	memset(out, 0, sizeof(*out));
	if (!osk_btc_read_u32(raw, len, &pos, &out->version))
		return OSK_ERR_FORMAT;
	if (!osk_btc_read_varint(raw, len, &pos, &count))
		return OSK_ERR_FORMAT;
	/* Eine unsignierte Transaktion hat keine Zeugen-Markierung. Steht da
	 * 0x00, ist es eine mit Zeugen - die gehoert nicht in PSBT-Feld 0x00. */
	if (count == 0)
		return OSK_ERR_FORMAT;
	if (count > OSK_BTC_MAX_IN)
		return OSK_ERR_FULL;
	out->in_count = (size_t)count;
	for (size_t i = 0; i < out->in_count; i++) {
		uint64_t script_len = 0;

		if (pos + 32u > len)
			return OSK_ERR_FORMAT;
		memcpy(out->in[i].txid, raw + pos, 32);
		pos += 32u;
		if (!osk_btc_read_u32(raw, len, &pos, &out->in[i].vout))
			return OSK_ERR_FORMAT;
		if (!osk_btc_read_varint(raw, len, &pos, &script_len))
			return OSK_ERR_FORMAT;
		/* Unsigniert heisst: leeres Eingangsskript. Alles andere waere
		 * eine schon unterschriebene Transaktion. */
		if (script_len != 0)
			return OSK_ERR_FORMAT;
		if (!osk_btc_read_u32(raw, len, &pos, &out->in[i].sequence))
			return OSK_ERR_FORMAT;
	}
	if (!osk_btc_read_varint(raw, len, &pos, &count))
		return OSK_ERR_FORMAT;
	if (count > OSK_BTC_MAX_OUT)
		return OSK_ERR_FULL;
	out->out_count = (size_t)count;
	for (size_t i = 0; i < out->out_count; i++) {
		uint64_t script_len = 0;

		if (!osk_btc_read_u64(raw, len, &pos, &out->out[i].value))
			return OSK_ERR_FORMAT;
		/* Mehr als 21 Millionen Bitcoin gibt es nicht. Ein Betrag
		 * darueber ist entweder Unsinn oder ein Angriff auf die
		 * Gebuehrenrechnung. */
		if (out->out[i].value > 2100000000000000ULL)
			return OSK_ERR_FORMAT;
		if (!osk_btc_read_varint(raw, len, &pos, &script_len))
			return OSK_ERR_FORMAT;
		if (script_len > OSK_BTC_SCRIPT_MAX)
			return OSK_ERR_FULL;
		if (pos + (size_t)script_len > len)
			return OSK_ERR_FORMAT;
		memcpy(out->out[i].script, raw + pos, (size_t)script_len);
		out->out[i].script_len = (size_t)script_len;
		pos += (size_t)script_len;
	}
	if (!osk_btc_read_u32(raw, len, &pos, &out->locktime))
		return OSK_ERR_FORMAT;
	if (pos != len)
		return OSK_ERR_FORMAT; /* Rest am Ende */
	return OSK_OK;
}

osk_result_t osk_btc_serialize_tx(const osk_btc_tx_t *tx, uint8_t *out, size_t cap, size_t *len_out)
{
	writer_t w;

	if (tx == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	w.buf = out;
	w.cap = cap;
	w.len = 0;
	w.overflow = false;
	w_u32(&w, tx->version);
	w_varint(&w, tx->in_count);
	for (size_t i = 0; i < tx->in_count; i++) {
		w_bytes(&w, tx->in[i].txid, 32);
		w_u32(&w, tx->in[i].vout);
		w_varint(&w, 0);
		w_u32(&w, tx->in[i].sequence);
	}
	w_varint(&w, tx->out_count);
	for (size_t i = 0; i < tx->out_count; i++) {
		w_u64(&w, tx->out[i].value);
		w_varint(&w, tx->out[i].script_len);
		w_bytes(&w, tx->out[i].script, tx->out[i].script_len);
	}
	w_u32(&w, tx->locktime);
	if (w.overflow)
		return OSK_ERR_ARG;
	*len_out = w.len;
	return OSK_OK;
}

/* --- Skripte ------------------------------------------------------------- */

osk_btc_script_kind_t osk_btc_script_kind(const uint8_t *script, size_t len)
{
	if (script == NULL || len == 0)
		return OSK_BTC_UNKNOWN;
	if (len == 25u && script[0] == 0x76u && script[1] == 0xA9u && script[2] == 0x14u &&
	    script[23] == 0x88u && script[24] == 0xACu)
		return OSK_BTC_P2PKH;
	if (len == 23u && script[0] == 0xA9u && script[1] == 0x14u && script[22] == 0x87u)
		return OSK_BTC_P2SH;
	if (len == 22u && script[0] == 0x00u && script[1] == 0x14u)
		return OSK_BTC_P2WPKH;
	if (len == 34u && script[0] == 0x00u && script[1] == 0x20u)
		return OSK_BTC_P2WSH;
	if (len == 34u && script[0] == 0x51u && script[1] == 0x20u)
		return OSK_BTC_P2TR;
	if (script[0] == 0x6Au)
		return OSK_BTC_OPRETURN;
	return OSK_BTC_UNKNOWN;
}

osk_result_t osk_btc_script_p2pkh(const uint8_t hash160[20], uint8_t *out, size_t cap,
				  size_t *len_out)
{
	if (hash160 == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (cap < 25u)
		return OSK_ERR_ARG;
	out[0] = 0x76u; /* OP_DUP */
	out[1] = 0xA9u; /* OP_HASH160 */
	out[2] = 0x14u;
	memcpy(out + 3, hash160, 20);
	out[23] = 0x88u; /* OP_EQUALVERIFY */
	out[24] = 0xACu; /* OP_CHECKSIG */
	*len_out = 25u;
	return OSK_OK;
}

osk_result_t osk_btc_script_p2wpkh(const uint8_t hash160[20], uint8_t *out, size_t cap,
				   size_t *len_out)
{
	if (hash160 == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (cap < 22u)
		return OSK_ERR_ARG;
	out[0] = 0x00u;
	out[1] = 0x14u;
	memcpy(out + 2, hash160, 20);
	*len_out = 22u;
	return OSK_OK;
}

osk_result_t osk_btc_address_from_script(const uint8_t *script, size_t len, const char *hrp,
					 uint8_t p2pkh_version, uint8_t p2sh_version, char *out,
					 size_t cap, size_t *len_out)
{
	osk_btc_script_kind_t kind = osk_btc_script_kind(script, len);
	uint8_t payload[21];

	if (script == NULL || hrp == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	switch (kind) {
	case OSK_BTC_P2PKH:
		payload[0] = p2pkh_version;
		memcpy(payload + 1, script + 3, 20);
		return osk_base58check_encode(payload, sizeof(payload), out, cap, len_out);
	case OSK_BTC_P2SH:
		payload[0] = p2sh_version;
		memcpy(payload + 1, script + 2, 20);
		return osk_base58check_encode(payload, sizeof(payload), out, cap, len_out);
	case OSK_BTC_P2WPKH:
		return osk_bech32_encode_address(hrp, 0, script + 2, 20, out, cap, len_out);
	case OSK_BTC_P2WSH:
		return osk_bech32_encode_address(hrp, 0, script + 2, 32, out, cap, len_out);
	case OSK_BTC_P2TR:
		return osk_bech32_encode_address(hrp, 1, script + 2, 32, out, cap, len_out);
	default:
		/* OP_RETURN und alles Unbekannte hat keine Adresse. Der
		 * Aufrufer muss das anzeigen koennen oder ablehnen. */
		return OSK_ERR_UNSUPPORTED;
	}
}

/* --- Hash zum Unterschreiben -------------------------------------------- */

osk_result_t osk_btc_sighash_legacy(const osk_btc_tx_t *tx, size_t index,
				    const uint8_t *script_code, size_t script_len,
				    uint32_t hash_type, uint8_t out[32])
{
	uint8_t buf[4 + 9 + OSK_BTC_MAX_IN * (32 + 4 + 9 + OSK_BTC_SCRIPT_MAX + 4) + 9 +
		    OSK_BTC_MAX_OUT * (8 + 9 + OSK_BTC_SCRIPT_MAX) + 4 + 4];
	writer_t w;

	if (tx == NULL || script_code == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (index >= tx->in_count || script_len > OSK_BTC_SCRIPT_MAX)
		return OSK_ERR_ARG;
	if (hash_type != OSK_BTC_SIGHASH_ALL)
		return OSK_ERR_UNSUPPORTED;

	w.buf = buf;
	w.cap = sizeof(buf);
	w.len = 0;
	w.overflow = false;
	w_u32(&w, tx->version);
	w_varint(&w, tx->in_count);
	for (size_t i = 0; i < tx->in_count; i++) {
		w_bytes(&w, tx->in[i].txid, 32);
		w_u32(&w, tx->in[i].vout);
		if (i == index) {
			w_varint(&w, script_len);
			w_bytes(&w, script_code, script_len);
		} else {
			w_varint(&w, 0);
		}
		w_u32(&w, tx->in[i].sequence);
	}
	w_varint(&w, tx->out_count);
	for (size_t i = 0; i < tx->out_count; i++) {
		w_u64(&w, tx->out[i].value);
		w_varint(&w, tx->out[i].script_len);
		w_bytes(&w, tx->out[i].script, tx->out[i].script_len);
	}
	w_u32(&w, tx->locktime);
	w_u32(&w, hash_type);
	if (w.overflow)
		return OSK_ERR_INTERNAL;
	osk_hash256(buf, w.len, out);
	osk_wipe(buf, sizeof(buf));
	return OSK_OK;
}

osk_result_t osk_btc_sighash_segwit(const osk_btc_tx_t *tx, size_t index,
				    const uint8_t *script_code, size_t script_len, uint64_t value,
				    uint32_t hash_type, uint8_t out[32])
{
	uint8_t scratch[OSK_BTC_MAX_OUT * (8 + 9 + OSK_BTC_SCRIPT_MAX)];
	uint8_t prevouts[32], sequences[32], outputs[32];
	uint8_t pre[4 + 32 + 32 + 32 + 4 + 9 + OSK_BTC_SCRIPT_MAX + 8 + 4 + 32 + 4 + 4];
	writer_t w;

	if (tx == NULL || script_code == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (index >= tx->in_count || script_len > OSK_BTC_SCRIPT_MAX)
		return OSK_ERR_ARG;
	if (hash_type != OSK_BTC_SIGHASH_ALL)
		return OSK_ERR_UNSUPPORTED;

	/* hashPrevouts */
	w.buf = scratch;
	w.cap = sizeof(scratch);
	w.len = 0;
	w.overflow = false;
	for (size_t i = 0; i < tx->in_count; i++) {
		w_bytes(&w, tx->in[i].txid, 32);
		w_u32(&w, tx->in[i].vout);
	}
	if (w.overflow)
		return OSK_ERR_INTERNAL;
	osk_hash256(scratch, w.len, prevouts);

	/* hashSequence */
	w.len = 0;
	for (size_t i = 0; i < tx->in_count; i++)
		w_u32(&w, tx->in[i].sequence);
	if (w.overflow)
		return OSK_ERR_INTERNAL;
	osk_hash256(scratch, w.len, sequences);

	/* hashOutputs */
	w.len = 0;
	for (size_t i = 0; i < tx->out_count; i++) {
		w_u64(&w, tx->out[i].value);
		w_varint(&w, tx->out[i].script_len);
		w_bytes(&w, tx->out[i].script, tx->out[i].script_len);
	}
	if (w.overflow)
		return OSK_ERR_INTERNAL;
	osk_hash256(scratch, w.len, outputs);

	w.buf = pre;
	w.cap = sizeof(pre);
	w.len = 0;
	w.overflow = false;
	w_u32(&w, tx->version);
	w_bytes(&w, prevouts, 32);
	w_bytes(&w, sequences, 32);
	w_bytes(&w, tx->in[index].txid, 32);
	w_u32(&w, tx->in[index].vout);
	w_varint(&w, script_len);
	w_bytes(&w, script_code, script_len);
	w_u64(&w, value);
	w_u32(&w, tx->in[index].sequence);
	w_bytes(&w, outputs, 32);
	w_u32(&w, tx->locktime);
	w_u32(&w, hash_type);
	if (w.overflow)
		return OSK_ERR_INTERNAL;
	osk_hash256(pre, w.len, out);

	osk_wipe(scratch, sizeof(scratch));
	osk_wipe(pre, sizeof(pre));
	return OSK_OK;
}
