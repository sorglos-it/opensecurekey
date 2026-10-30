/* PSBT nach BIP174, selbst geschrieben. Siehe wallet_psbt.h fuer die
 * Arbeitsweise und die Grenzen. */

#include <string.h>
#include "wallet_psbt.h"
#include "wallet_seed.h"
#include "wallet_hash.h"
#include "wallet_codec.h"
#include "wallet_signer.h"
#include "osk_crypto.h"
#include "osk_ui.h"

static const uint8_t PSBT_MAGIC[5] = {0x70u, 0x73u, 0x62u, 0x74u, 0xFFu};

/* Schluesseltypen, die wir kennen. Alles andere wird woertlich
 * weitergegeben, nicht geloescht. */
#define GLOBAL_UNSIGNED_TX 0x00u
#define IN_NON_WITNESS_UTXO 0x00u
#define IN_WITNESS_UTXO 0x01u
#define IN_PARTIAL_SIG 0x02u
#define IN_SIGHASH_TYPE 0x03u
#define IN_REDEEM_SCRIPT 0x04u
#define IN_WITNESS_SCRIPT 0x05u
#define IN_BIP32_DERIVATION 0x06u
#define IN_FINAL_SCRIPTSIG 0x07u
#define IN_FINAL_WITNESS 0x08u
#define OUT_BIP32_DERIVATION 0x02u

/* --- Lesen --------------------------------------------------------------- */

typedef struct {
	const uint8_t *p;
	size_t len;
	size_t pos;
} reader_t;

static bool r_varint(reader_t *r, uint64_t *out)
{
	return osk_btc_read_varint(r->p, r->len, &r->pos, out);
}

static bool r_bytes(reader_t *r, size_t len, const uint8_t **out)
{
	if (r->pos + len > r->len)
		return false;
	*out = r->p + r->pos;
	r->pos += len;
	return true;
}

static uint32_t load_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static osk_result_t read_deriv(const uint8_t *key, size_t key_len, const uint8_t *value,
			       size_t value_len, osk_psbt_deriv_t *deriv)
{
	if (key_len != 1u + OSK_EC_PUB_COMPRESSED)
		return OSK_ERR_FORMAT;
	if (key[1] != 0x02u && key[1] != 0x03u)
		return OSK_ERR_FORMAT;
	if (value_len < 4u || ((value_len - 4u) % 4u) != 0)
		return OSK_ERR_FORMAT;
	if ((value_len - 4u) / 4u > OSK_PSBT_MAX_PATH)
		return OSK_ERR_FULL;
	memcpy(deriv->pub, key + 1, OSK_EC_PUB_COMPRESSED);
	/* Der Fingerabdruck steht hier hoechstwertiges Byte zuerst, die
	 * Pfadschritte niederwertig zuerst. Das ist eine Stolperstelle von
	 * BIP174 und steht darum ausdruecklich hier. */
	deriv->fingerprint = ((uint32_t)value[0] << 24) | ((uint32_t)value[1] << 16) |
			     ((uint32_t)value[2] << 8) | (uint32_t)value[3];
	deriv->path_len = (value_len - 4u) / 4u;
	for (size_t i = 0; i < deriv->path_len; i++)
		deriv->path[i] = load_le32(value + 4u + 4u * i);
	return OSK_OK;
}

/* Einen Abschnitt lesen: alle Paare bis zum Trennbyte 0x00. `handler`
 * bekommt jedes Paar. */
typedef osk_result_t (*pair_fn)(void *ctx, const uint8_t *key, size_t key_len, const uint8_t *value,
				size_t value_len);

static osk_result_t read_map(reader_t *r, const uint8_t **start_out, size_t *len_out, pair_fn fn,
			     void *ctx)
{
	size_t begin = r->pos;

	for (;;) {
		uint64_t key_len = 0, value_len = 0;
		const uint8_t *key = NULL, *value = NULL;
		osk_result_t res;

		if (r->pos >= r->len)
			return OSK_ERR_FORMAT; /* Abschnitt ohne Ende */
		if (r->p[r->pos] == 0x00u) {
			r->pos += 1u;
			break;
		}
		if (!r_varint(r, &key_len) || key_len == 0 || key_len > 1024u)
			return OSK_ERR_FORMAT;
		if (!r_bytes(r, (size_t)key_len, &key))
			return OSK_ERR_FORMAT;
		if (!r_varint(r, &value_len) || value_len > 100000u)
			return OSK_ERR_FORMAT;
		if (!r_bytes(r, (size_t)value_len, &value))
			return OSK_ERR_FORMAT;
		res = fn(ctx, key, (size_t)key_len, value, (size_t)value_len);
		if (res != OSK_OK)
			return res;
	}
	*start_out = r->p + begin;
	*len_out = r->pos - begin - 1u; /* ohne das Trennbyte */
	return OSK_OK;
}

struct global_ctx {
	const uint8_t *tx;
	size_t tx_len;
};

static osk_result_t global_pair(void *ctx, const uint8_t *key, size_t key_len, const uint8_t *value,
				size_t value_len)
{
	struct global_ctx *g = (struct global_ctx *)ctx;

	if (key[0] != GLOBAL_UNSIGNED_TX)
		return OSK_OK; /* xpub, Version, Unbekanntes: nur durchreichen */
	if (key_len != 1u)
		return OSK_ERR_FORMAT;
	if (g->tx != NULL)
		return OSK_ERR_FORMAT; /* zweimal dieselbe Transaktion */
	g->tx = value;
	g->tx_len = value_len;
	return OSK_OK;
}

static osk_result_t input_pair(void *ctx, const uint8_t *key, size_t key_len, const uint8_t *value,
			       size_t value_len)
{
	osk_psbt_input_t *in = (osk_psbt_input_t *)ctx;

	switch (key[0]) {
	case IN_NON_WITNESS_UTXO:
		if (key_len != 1u)
			return OSK_ERR_FORMAT;
		/* Die vollstaendige Vorgaenger-Transaktion. Betrag und Skript
		 * holt osk_psbt_parse daraus heraus, nachdem es die Kennung
		 * geprueft hat - hier wird nur gemerkt, wo sie liegt. */
		in->utxo_script = value;
		in->utxo_script_len = value_len;
		in->has_witness_utxo = false;
		break;
	case IN_WITNESS_UTXO: {
		size_t pos = 0;
		uint64_t script_len = 0;

		if (key_len != 1u)
			return OSK_ERR_FORMAT;
		if (!osk_btc_read_u64(value, value_len, &pos, &in->value))
			return OSK_ERR_FORMAT;
		if (in->value > 2100000000000000ULL)
			return OSK_ERR_FORMAT;
		if (!osk_btc_read_varint(value, value_len, &pos, &script_len))
			return OSK_ERR_FORMAT;
		if (pos + (size_t)script_len != value_len)
			return OSK_ERR_FORMAT;
		if (script_len > OSK_BTC_SCRIPT_MAX)
			return OSK_ERR_FULL;
		in->utxo_script = value + pos;
		in->utxo_script_len = (size_t)script_len;
		in->has_witness_utxo = true;
		break;
	}
	case IN_PARTIAL_SIG:
		/* Der Schluessel ist Typ plus oeffentlicher Schluessel. Eine
		 * andere Laenge ist ein kaputtes PSBT - BIP174 fuehrt genau
		 * das als ungueltigen Fall auf. */
		if (key_len != 1u + OSK_EC_PUB_COMPRESSED &&
		    key_len != 1u + OSK_EC_PUB_UNCOMPRESSED)
			return OSK_ERR_FORMAT;
		in->already_signed = true;
		break;
	case IN_FINAL_SCRIPTSIG:
	case IN_FINAL_WITNESS:
		if (key_len != 1u)
			return OSK_ERR_FORMAT;
		in->already_signed = true;
		break;
	case IN_SIGHASH_TYPE:
		if (key_len != 1u || value_len != 4u)
			return OSK_ERR_FORMAT;
		in->sighash_type = load_le32(value);
		break;
	case IN_REDEEM_SCRIPT:
		if (key_len != 1u)
			return OSK_ERR_FORMAT;
		if (value_len > OSK_BTC_SCRIPT_MAX)
			return OSK_ERR_FULL;
		in->redeem_script = value;
		in->redeem_script_len = value_len;
		break;
	case IN_WITNESS_SCRIPT:
		if (key_len != 1u)
			return OSK_ERR_FORMAT;
		if (value_len > OSK_BTC_SCRIPT_MAX)
			return OSK_ERR_FULL;
		in->witness_script = value;
		in->witness_script_len = value_len;
		break;
	case IN_BIP32_DERIVATION: {
		osk_result_t res;

		if (in->deriv_count >= OSK_PSBT_MAX_DERIV)
			return OSK_ERR_FULL;
		res = read_deriv(key, key_len, value, value_len, &in->deriv[in->deriv_count]);
		if (res != OSK_OK)
			return res;
		in->deriv_count++;
		break;
	}
	default:
		break;
	}
	return OSK_OK;
}

static osk_result_t output_pair(void *ctx, const uint8_t *key, size_t key_len, const uint8_t *value,
				size_t value_len)
{
	osk_psbt_output_t *out = (osk_psbt_output_t *)ctx;

	/* Die beiden Skriptfelder eines Ausgangs haben einen Schluessel von
	 * genau einem Byte. Auch wenn wir sie nicht benutzen, wird eine
	 * falsche Laenge abgelehnt - sonst geht ein kaputtes PSBT durch und
	 * kommt unterschrieben zurueck. */
	if ((key[0] == 0x00u || key[0] == 0x01u) && key_len != 1u)
		return OSK_ERR_FORMAT;
	if (key[0] != OUT_BIP32_DERIVATION)
		return OSK_OK;
	if (out->deriv_count >= OSK_PSBT_MAX_DERIV)
		return OSK_ERR_FULL;
	{
		osk_result_t res =
		    read_deriv(key, key_len, value, value_len, &out->deriv[out->deriv_count]);

		if (res != OSK_OK)
			return res;
		out->deriv_count++;
	}
	return OSK_OK;
}

/* Eine fertige, also unterschriebene Vorgaenger-Transaktion durchgehen und
 * dabei zwei Dinge auf einmal tun: ihre Kennung bilden und Betrag samt
 * Skript des gesuchten Ausgangs herausholen.
 *
 * Warum zusammen: die Kennung einer Transaktion ist der doppelte SHA-256
 * ueber die Form OHNE Zeugen. Eine SegWit-Transaktion wird aber MIT Zeugen
 * geliefert (Markierung 0x00 0x01 hinter der Version). Wer einfach die
 * ganzen Bytes hasht, bekommt die Zeugenkennung und damit nie eine
 * Uebereinstimmung - der Vektor "PSBT with one P2PKH input" aus BIP174
 * faellt genau darauf. Darum wird hier woertlich die Form ohne Zeugen
 * gehasht, Stueck fuer Stueck, ohne sie noch einmal zusammenzukopieren. */
static osk_result_t prev_tx_scan(const uint8_t *raw, size_t len, uint32_t index,
				 uint64_t *value_out, const uint8_t **script_out,
				 size_t *script_len_out, uint8_t txid_out[32])
{
	size_t pos = 0;
	uint32_t version = 0;
	uint64_t in_count = 0, out_count = 0;
	bool segwit = false;
	osk_sha256_t sha;
	uint8_t once[OSK_SHA256_LEN];
	bool found = false;

	osk_sha256_init(&sha);
	if (!osk_btc_read_u32(raw, len, &pos, &version))
		return OSK_ERR_FORMAT;
	osk_sha256_update(&sha, raw, 4);
	if (!osk_btc_read_varint(raw, len, &pos, &in_count))
		return OSK_ERR_FORMAT;
	if (in_count == 0) {
		/* Markierung und Flagge: 0x00 0x01. Beides gehoert NICHT in
		 * die Kennung. */
		if (pos >= len || raw[pos] != 0x01u)
			return OSK_ERR_FORMAT;
		pos += 1u;
		segwit = true;
		if (!osk_btc_read_varint(raw, len, &pos, &in_count) || in_count == 0)
			return OSK_ERR_FORMAT;
		/* Die Zahl der Eingaenge steht jetzt woanders; den
		 * Laengenbyte-Block von vorn neu schreiben. */
		{
			uint8_t vi[9];
			size_t vi_len = osk_btc_write_varint(in_count, vi);

			osk_sha256_update(&sha, vi, vi_len);
		}
	} else {
		uint8_t vi[9];
		size_t vi_len = osk_btc_write_varint(in_count, vi);

		osk_sha256_update(&sha, vi, vi_len);
	}

	for (uint64_t i = 0; i < in_count; i++) {
		size_t begin = pos;
		uint64_t script_len = 0;
		uint32_t vout = 0;

		if (pos + 32u > len)
			return OSK_ERR_FORMAT;
		pos += 32u;
		if (!osk_btc_read_u32(raw, len, &pos, &vout))
			return OSK_ERR_FORMAT;
		if (!osk_btc_read_varint(raw, len, &pos, &script_len))
			return OSK_ERR_FORMAT;
		if (pos + (size_t)script_len > len)
			return OSK_ERR_FORMAT;
		pos += (size_t)script_len;
		if (pos + 4u > len)
			return OSK_ERR_FORMAT;
		pos += 4u;
		osk_sha256_update(&sha, raw + begin, pos - begin);
	}

	if (!osk_btc_read_varint(raw, len, &pos, &out_count))
		return OSK_ERR_FORMAT;
	{
		uint8_t vi[9];
		size_t vi_len = osk_btc_write_varint(out_count, vi);

		osk_sha256_update(&sha, vi, vi_len);
	}
	if ((uint64_t)index >= out_count)
		return OSK_ERR_FORMAT;
	for (uint64_t i = 0; i < out_count; i++) {
		size_t begin = pos;
		uint64_t value = 0, script_len = 0;

		if (!osk_btc_read_u64(raw, len, &pos, &value))
			return OSK_ERR_FORMAT;
		if (!osk_btc_read_varint(raw, len, &pos, &script_len))
			return OSK_ERR_FORMAT;
		if (pos + (size_t)script_len > len)
			return OSK_ERR_FORMAT;
		if (i == (uint64_t)index) {
			if (script_len > OSK_BTC_SCRIPT_MAX)
				return OSK_ERR_FULL;
			if (value > 2100000000000000ULL)
				return OSK_ERR_FORMAT;
			*value_out = value;
			*script_out = raw + pos;
			*script_len_out = (size_t)script_len;
			found = true;
		}
		pos += (size_t)script_len;
		osk_sha256_update(&sha, raw + begin, pos - begin);
	}

	if (segwit) {
		/* Zeugen ueberspringen: je Eingang eine Liste von Stuecken. */
		for (uint64_t i = 0; i < in_count; i++) {
			uint64_t items = 0;

			if (!osk_btc_read_varint(raw, len, &pos, &items))
				return OSK_ERR_FORMAT;
			for (uint64_t k = 0; k < items; k++) {
				uint64_t item_len = 0;

				if (!osk_btc_read_varint(raw, len, &pos, &item_len))
					return OSK_ERR_FORMAT;
				if (pos + (size_t)item_len > len)
					return OSK_ERR_FORMAT;
				pos += (size_t)item_len;
			}
		}
	}
	if (pos + 4u > len)
		return OSK_ERR_FORMAT;
	osk_sha256_update(&sha, raw + pos, 4);
	pos += 4u;
	if (pos != len)
		return OSK_ERR_FORMAT; /* Rest am Ende */
	if (!found)
		return OSK_ERR_FORMAT;

	osk_sha256_final(&sha, once);
	osk_sha256(once, sizeof(once), txid_out);
	osk_wipe(once, sizeof(once));
	return OSK_OK;
}

osk_result_t osk_psbt_parse(const uint8_t *raw, size_t len, osk_psbt_t *out)
{
	reader_t r;
	struct global_ctx globals;
	osk_result_t res;

	if (raw == NULL || out == NULL)
		return OSK_ERR_ARG;
	memset(out, 0, sizeof(*out));
	if (len < sizeof(PSBT_MAGIC) || memcmp(raw, PSBT_MAGIC, sizeof(PSBT_MAGIC)) != 0)
		return OSK_ERR_FORMAT;

	out->raw = raw;
	out->raw_len = len;
	r.p = raw;
	r.len = len;
	r.pos = sizeof(PSBT_MAGIC);

	memset(&globals, 0, sizeof(globals));
	res = read_map(&r, &out->global_start, &out->global_len, global_pair, &globals);
	if (res != OSK_OK)
		return res;
	if (globals.tx == NULL)
		return OSK_ERR_FORMAT; /* ohne Transaktion ist es kein PSBT */
	res = osk_btc_parse_tx(globals.tx, globals.tx_len, &out->tx);
	if (res != OSK_OK)
		return res;

	out->in_count = out->tx.in_count;
	out->out_count = out->tx.out_count;
	for (size_t i = 0; i < out->in_count; i++) {
		res = read_map(&r, &out->in[i].start, &out->in[i].len, input_pair, &out->in[i]);
		if (res != OSK_OK)
			return res;
		if (!out->in[i].has_witness_utxo && out->in[i].utxo_script != NULL) {
			/* Nicht-Zeugen-UTXO: erst die Kennung pruefen, dann den
			 * Betrag glauben. Ohne diese Pruefung koennte ein
			 * falscher Betrag die Gebuehr beliebig klein rechnen -
			 * und die Gebuehr ist, was der Nutzer kontrolliert. */
			uint8_t id[32];
			const uint8_t *prev = out->in[i].utxo_script;
			size_t prev_len = out->in[i].utxo_script_len;
			uint64_t value = 0;
			const uint8_t *script = NULL;
			size_t script_len = 0;

			res = prev_tx_scan(prev, prev_len, out->tx.in[i].vout, &value, &script,
					   &script_len, id);
			if (res != OSK_OK)
				return res;
			if (!osk_memeq(id, out->tx.in[i].txid, 32))
				return OSK_ERR_FORMAT;
			out->in[i].value = value;
			out->in[i].utxo_script = script;
			out->in[i].utxo_script_len = script_len;
		}
	}
	for (size_t i = 0; i < out->out_count; i++) {
		res = read_map(&r, &out->out[i].start, &out->out[i].len, output_pair, &out->out[i]);
		if (res != OSK_OK)
			return res;
	}
	if (r.pos != len)
		return OSK_ERR_FORMAT; /* Rest am Ende */
	return OSK_OK;
}

/* --- Anzeige ------------------------------------------------------------- */

static bool output_is_ours(const osk_psbt_output_t *out, uint32_t master_fp)
{
	for (size_t i = 0; i < out->deriv_count; i++) {
		if (out->deriv[i].fingerprint == master_fp)
			return true;
	}
	return false;
}

osk_result_t osk_psbt_view(const osk_psbt_t *psbt, uint32_t master_fp, const char *hrp,
			   uint8_t p2pkh_version, uint8_t p2sh_version, osk_wallet_view_t *view_out)
{
	uint64_t in_total = 0, out_total = 0, sent = 0;
	size_t recipients = 0;
	bool values_known = true;
	char address[OSK_BTC_ADDRESS_MAX];
	bool have_address = false;

	if (psbt == NULL || view_out == NULL || hrp == NULL)
		return OSK_ERR_ARG;
	osk_wallet_view_init(view_out);

	for (size_t i = 0; i < psbt->in_count; i++) {
		if (psbt->in[i].utxo_script == NULL || psbt->in[i].value == 0) {
			values_known = false;
			view_out->missing_key = "wallet.eingang-ohne-betrag";
			continue;
		}
		in_total += psbt->in[i].value;
	}
	for (size_t i = 0; i < psbt->out_count; i++) {
		out_total += psbt->tx.out[i].value;
		if (output_is_ours(&psbt->out[i], master_fp))
			continue; /* Wechselgeld - kein Empfaenger */
		recipients++;
		sent += psbt->tx.out[i].value;
		if (!have_address) {
			size_t len = 0;
			osk_result_t res = osk_btc_address_from_script(
			    psbt->tx.out[i].script, psbt->tx.out[i].script_len, hrp, p2pkh_version,
			    p2sh_version, address, sizeof(address), &len);

			if (res == OSK_OK) {
				have_address = true;
			} else {
				values_known = false;
				view_out->missing_key = "wallet.ausgang-ohne-adresse";
			}
		}
	}

	if (have_address) {
		osk_result_t res =
		    osk_wallet_set_address(view_out, address);

		if (res != OSK_OK)
			values_known = false;
	}
	if (recipients > 1u)
		view_out->more_recipients = (uint32_t)(recipients - 1u);

	if (values_known && in_total >= out_total) {
		if (osk_wallet_format_amount(sent, 8, "BTC", view_out->amount,
					     sizeof(view_out->amount)) != OSK_OK ||
		    osk_wallet_format_amount(in_total - out_total, 8, "BTC", view_out->fee,
					     sizeof(view_out->fee)) != OSK_OK)
			values_known = false;
	} else {
		values_known = false;
		if (in_total < out_total)
			view_out->missing_key = "wallet.gebuehr-negativ";
	}
	view_out->readable = values_known && have_address;
	return OSK_OK;
}

/* --- Unterschreiben ------------------------------------------------------ */

/* Welchen Hash braucht diese Eingabe, und mit welchem Skript?
 * Liefert OSK_ERR_UNSUPPORTED fuer alles, was diese Fassung nicht
 * unterschreibt - Taproot zum Beispiel. */
static osk_result_t sighash_for(const osk_psbt_t *psbt, size_t index,
				const uint8_t pub[OSK_EC_PUB_COMPRESSED], uint8_t out[32])
{
	const osk_psbt_input_t *in = &psbt->in[index];
	uint8_t key_hash[OSK_RIPEMD160_LEN];
	uint8_t script_code[OSK_BTC_SCRIPT_MAX];
	size_t script_len = 0;
	osk_btc_script_kind_t kind;
	const uint8_t *spk;
	size_t spk_len;
	uint32_t hash_type = (in->sighash_type == 0) ? OSK_BTC_SIGHASH_ALL : in->sighash_type;

	if (hash_type != OSK_BTC_SIGHASH_ALL)
		return OSK_ERR_UNSUPPORTED;
	if (in->utxo_script == NULL)
		return OSK_ERR_FORMAT;

	osk_hash160(pub, OSK_EC_PUB_COMPRESSED, key_hash);
	spk = in->utxo_script;
	spk_len = in->utxo_script_len;
	kind = osk_btc_script_kind(spk, spk_len);

	if (kind == OSK_BTC_P2TR)
		return OSK_ERR_UNSUPPORTED;

	if (kind == OSK_BTC_P2SH) {
		/* Das eigentliche Skript steht im Redeem-Skript, und sein
		 * Hash muss zum Skript des Eingangs passen. */
		uint8_t redeem_hash[OSK_RIPEMD160_LEN];

		if (in->redeem_script == NULL)
			return OSK_ERR_FORMAT;
		osk_hash160(in->redeem_script, in->redeem_script_len, redeem_hash);
		if (!osk_memeq(redeem_hash, spk + 2, 20))
			return OSK_ERR_FORMAT;
		spk = in->redeem_script;
		spk_len = in->redeem_script_len;
		kind = osk_btc_script_kind(spk, spk_len);
		if (kind == OSK_BTC_P2TR)
			return OSK_ERR_UNSUPPORTED;
	}

	switch (kind) {
	case OSK_BTC_P2PKH:
		/* Der Schluessel muss der sein, auf den das Skript zeigt. */
		if (!osk_memeq(key_hash, spk + 3, 20))
			return OSK_ERR_FORMAT;
		if (in->has_witness_utxo)
			return OSK_ERR_FORMAT; /* P2PKH hat keine Zeugen */
		memcpy(script_code, spk, spk_len);
		script_len = spk_len;
		return osk_btc_sighash_legacy(&psbt->tx, index, script_code, script_len, hash_type,
					      out);
	case OSK_BTC_P2WPKH: {
		osk_result_t res;

		if (!osk_memeq(key_hash, spk + 2, 20))
			return OSK_ERR_FORMAT;
		/* BIP143: unterschrieben wird mit dem P2PKH-Skript zum
		 * selben Schluesselhash, nicht mit dem Zeugenprogramm. */
		res = osk_btc_script_p2pkh(spk + 2, script_code, sizeof(script_code), &script_len);
		if (res != OSK_OK)
			return res;
		if (in->value == 0)
			return OSK_ERR_FORMAT;
		return osk_btc_sighash_segwit(&psbt->tx, index, script_code, script_len, in->value,
					      hash_type, out);
	}
	case OSK_BTC_P2WSH: {
		uint8_t script_hash[OSK_SHA256_LEN];

		if (in->witness_script == NULL)
			return OSK_ERR_FORMAT;
		osk_sha256(in->witness_script, in->witness_script_len, script_hash);
		if (!osk_memeq(script_hash, spk + 2, 32))
			return OSK_ERR_FORMAT;
		if (in->value == 0)
			return OSK_ERR_FORMAT;
		return osk_btc_sighash_segwit(&psbt->tx, index, in->witness_script,
					      in->witness_script_len, in->value, hash_type, out);
	}
	default:
		return OSK_ERR_UNSUPPORTED;
	}
}

/* Pfad als Text aus den Zahlen bauen - osk_bip32_derive_path nimmt Text,
 * und so liegt die Pfadlogik an einer Stelle. */
static osk_result_t path_text(const osk_psbt_deriv_t *deriv, char *out, size_t cap)
{
	size_t used = 0;

	if (cap < 2u)
		return OSK_ERR_ARG;
	out[used++] = 'm';
	for (size_t i = 0; i < deriv->path_len; i++) {
		uint32_t value = deriv->path[i] & ~OSK_BIP32_HARDENED;
		char digits[12];
		size_t count = 0;

		if (used + 13u >= cap)
			return OSK_ERR_ARG;
		out[used++] = '/';
		do {
			digits[count++] = (char)('0' + (char)(value % 10u));
			value /= 10u;
		} while (value > 0 && count < sizeof(digits));
		while (count > 0)
			out[used++] = digits[--count];
		if ((deriv->path[i] & OSK_BIP32_HARDENED) != 0)
			out[used++] = '\'';
	}
	out[used] = '\0';
	return OSK_OK;
}

struct emit {
	uint8_t *buf;
	size_t cap;
	size_t len;
	bool overflow;
};

static void emit_bytes(struct emit *e, const uint8_t *p, size_t len)
{
	if (e->overflow || e->len + len > e->cap) {
		e->overflow = true;
		return;
	}
	if (len > 0)
		memcpy(e->buf + e->len, p, len);
	e->len += len;
}

static void emit_u8(struct emit *e, uint8_t v)
{
	emit_bytes(e, &v, 1);
}

static void emit_varint(struct emit *e, uint64_t v)
{
	uint8_t raw[9];
	size_t len = osk_btc_write_varint(v, raw);

	emit_bytes(e, raw, len);
}

osk_result_t osk_psbt_sign(const osk_psbt_t *psbt, osk_item_id_t seed_id, const char *asked_by,
			   const char *hrp, uint8_t p2pkh_version, uint8_t p2sh_version,
			   uint8_t *out, size_t cap, size_t *len_out,
			   size_t *signed_out)
{
	osk_wallet_view_t view;
	osk_bip32_priv_t master;
	osk_bip32_pub_t master_pub;
	uint32_t master_fp;
	struct emit e;
	uint8_t sig_der[80];
	size_t signed_count = 0;
	osk_result_t res;
	/* Je Eingabe hoechstens eine eigene Unterschrift. */
	struct {
		bool present;
		uint8_t pub[OSK_EC_PUB_COMPRESSED];
		uint8_t der[80];
		size_t der_len;
	} sigs[OSK_PSBT_MAX_IN];

	if (psbt == NULL || out == NULL || len_out == NULL || signed_out == NULL || hrp == NULL)
		return OSK_ERR_ARG;
	memset(sigs, 0, sizeof(sigs));
	*signed_out = 0;

	/* Erst den Wurzelschluessel holen - ohne ihn wissen wir nicht, welche
	 * Ausgaenge Wechselgeld sind, und ohne das ist der Betrag falsch. */
	res = osk_wallet_seed_derive(seed_id, asked_by, OSK_BIP32_SECP256K1, "m", &master);
	if (res != OSK_OK)
		return res;
	res = osk_bip32_neuter(&master, &master_pub);
	if (res != OSK_OK) {
		osk_bip32_wipe(&master);
		return res;
	}
	master_fp = osk_bip32_fingerprint(master_pub.pub);

	/* Taproot: erkennen und ablehnen, BEVOR gefragt wird.
	 *
	 * Taproot (BIP341) ist ein eigenes Hashverfahren und ein eigener
	 * Signaturtyp. Eine halbe Umsetzung waere gefaehrlicher als keine:
	 * sie wuerde unterschreiben, und niemand wuesste, was. Darum wird
	 * hier abgebrochen - und der Nutzer erfaehrt den Grund, statt eine
	 * Ablehnung ohne Erklaerung zu sehen.
	 *
	 * Das steht vor der Frage und nicht danach: den Nutzer etwas
	 * freigeben zu lassen, das anschliessend scheitert, waere eine
	 * Freigabe, die nichts bedeutet. */
	for (size_t i = 0; i < psbt->in_count; i++) {
		const osk_psbt_input_t *in = &psbt->in[i];

		if (in->utxo_script == NULL)
			continue;
		if (osk_btc_script_kind(in->utxo_script, in->utxo_script_len) == OSK_BTC_P2TR) {
			osk_ui_status("wallet.taproot-nicht-unterstuetzt");
			osk_bip32_wipe(&master);
			return OSK_ERR_UNSUPPORTED;
		}
	}

	res = osk_psbt_view(psbt, master_fp, hrp, p2pkh_version, p2sh_version, &view);
	if (res != OSK_OK) {
		osk_bip32_wipe(&master);
		return res;
	}
	/* Anzeigen und fragen - vor jeder Unterschrift, immer. */
	res = osk_wallet_confirm(&view, asked_by);
	if (res != OSK_OK) {
		osk_bip32_wipe(&master);
		return res;
	}

	for (size_t i = 0; i < psbt->in_count; i++) {
		for (size_t d = 0; d < psbt->in[i].deriv_count; d++) {
			const osk_psbt_deriv_t *deriv = &psbt->in[i].deriv[d];
			char path[OSK_BIP32_TEXT_MAX];
			osk_bip32_priv_t key;
			osk_bip32_pub_t pub;
			uint8_t hash[32];
			uint8_t r[32], s[32];
			size_t der_len = 0;
			osk_ec_point_t point;

			if (deriv->fingerprint != master_fp)
				continue;
			if (path_text(deriv, path, sizeof(path)) != OSK_OK)
				continue;
			if (osk_bip32_derive_path(&master, path, &key) != OSK_OK)
				continue;
			if (osk_bip32_neuter(&key, &pub) != OSK_OK) {
				osk_bip32_wipe(&key);
				continue;
			}
			/* Der Pfad muss wirklich zu dem Schluessel fuehren, den
			 * das PSBT nennt. Sonst unterschreibt das Geraet mit
			 * einem anderen Schluessel als angekuendigt. */
			if (memcmp(pub.pub, deriv->pub, sizeof(pub.pub)) != 0) {
				osk_bip32_wipe(&key);
				continue;
			}
			res = sighash_for(psbt, i, pub.pub, hash);
			if (res != OSK_OK) {
				osk_bip32_wipe(&key);
				osk_bip32_wipe(&master);
				return res; /* nicht halb unterschreiben */
			}
			if (osk_ec_point_decode(pub.pub, OSK_EC_PUB_COMPRESSED, &point) != OSK_OK) {
				osk_bip32_wipe(&key);
				osk_bip32_wipe(&master);
				return OSK_ERR_INTERNAL;
			}
			res =
			    osk_signer_sign_hash(OSK_CURVE_SECP256K1, key.key, hash, &point, r, s);
			osk_bip32_wipe(&key);
			if (res != OSK_OK) {
				osk_bip32_wipe(&master);
				return res;
			}
			res = osk_signer_raw_to_der(r, s, sig_der, sizeof(sig_der), &der_len);
			osk_wipe(r, sizeof(r));
			osk_wipe(s, sizeof(s));
			if (res != OSK_OK) {
				osk_bip32_wipe(&master);
				return res;
			}
			if (der_len + 1u > sizeof(sigs[i].der)) {
				osk_bip32_wipe(&master);
				return OSK_ERR_INTERNAL;
			}
			memcpy(sigs[i].der, sig_der, der_len);
			sigs[i].der[der_len] = (uint8_t)OSK_BTC_SIGHASH_ALL;
			sigs[i].der_len = der_len + 1u;
			memcpy(sigs[i].pub, pub.pub, sizeof(sigs[i].pub));
			sigs[i].present = true;
			signed_count++;
			break; /* eine eigene Unterschrift je Eingabe reicht */
		}
	}
	osk_bip32_wipe(&master);

	/* Jetzt das Ergebnis schreiben: alles woertlich, nur die eigenen
	 * Unterschriften kommen dazu. */
	e.buf = out;
	e.cap = cap;
	e.len = 0;
	e.overflow = false;
	emit_bytes(&e, PSBT_MAGIC, sizeof(PSBT_MAGIC));
	emit_bytes(&e, psbt->global_start, psbt->global_len);
	emit_u8(&e, 0x00u);
	for (size_t i = 0; i < psbt->in_count; i++) {
		emit_bytes(&e, psbt->in[i].start, psbt->in[i].len);
		if (sigs[i].present) {
			uint8_t key[1 + OSK_EC_PUB_COMPRESSED];

			key[0] = IN_PARTIAL_SIG;
			memcpy(key + 1, sigs[i].pub, OSK_EC_PUB_COMPRESSED);
			emit_varint(&e, sizeof(key));
			emit_bytes(&e, key, sizeof(key));
			emit_varint(&e, sigs[i].der_len);
			emit_bytes(&e, sigs[i].der, sigs[i].der_len);
		}
		emit_u8(&e, 0x00u);
	}
	for (size_t i = 0; i < psbt->out_count; i++) {
		emit_bytes(&e, psbt->out[i].start, psbt->out[i].len);
		emit_u8(&e, 0x00u);
	}
	osk_wipe(sigs, sizeof(sigs));
	if (e.overflow)
		return OSK_ERR_ARG;
	*len_out = e.len;
	*signed_out = signed_count;
	return OSK_OK;
}
