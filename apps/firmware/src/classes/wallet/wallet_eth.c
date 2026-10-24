/* Umsetzung von wallet_eth.h. Selbst geschrieben gegen EIP-155, EIP-1559,
 * EIP-2930, EIP-191, EIP-712 und EIP-55. */

#include <string.h>
#include "wallet_eth.h"
#include "wallet_rlp.h"
#include "wallet_bn.h"
#include "wallet_hash.h"
#include "wallet_codec.h"
#include "wallet_signer.h"
#include "osk_crypto.h"

/* Gas mal Preis, beides 256 Bit - aber Gas passt in 64 Bit, und ein
 * Gaspreis ueber 2^128 Wei ist Unsinn. Gerechnet wird mit 256 Bit und
 * geprueft, dass nichts ueberlaeuft. */
static osk_result_t fee_total(uint64_t gas, const uint8_t price[32], uint8_t out[32])
{
	osk_bn_t a, b, product;
	osk_bn2_t wide;

	osk_bn_from_be(&a, price);
	osk_bn_zero(&b);
	b.v[0] = (uint32_t)(gas & 0xFFFFFFFFu);
	b.v[1] = (uint32_t)((gas >> 32) & 0xFFFFFFFFu);
	osk_bn_mul(&wide, &a, &b);
	/* Passt das Ergebnis nicht in 256 Bit, ist die Angabe Unsinn - und
	 * eine abgeschnittene Gebuehr auf dem Schirm waere eine Luege. */
	for (unsigned i = OSK_BN_LIMBS; i < 2u * OSK_BN_LIMBS; i++) {
		if (wide.v[i] != 0)
			return OSK_ERR_FORMAT;
	}
	memcpy(product.v, wide.v, sizeof(product.v));
	osk_bn_to_be(&product, out);
	return OSK_OK;
}

static osk_result_t read_address(const osk_rlp_item_t *item, uint8_t out[OSK_ETH_ADDRESS_LEN],
				 bool *has_out)
{
	if (item->kind != OSK_RLP_BYTES)
		return OSK_ERR_FORMAT;
	if (item->len == 0) {
		memset(out, 0, OSK_ETH_ADDRESS_LEN);
		*has_out = false;
		return OSK_OK;
	}
	if (item->len != OSK_ETH_ADDRESS_LEN)
		return OSK_ERR_FORMAT;
	memcpy(out, item->data, OSK_ETH_ADDRESS_LEN);
	*has_out = true;
	return OSK_OK;
}

/* Eine Zahl, die auch 0 sein darf. RLP schreibt die Null als leere Folge. */
static osk_result_t read_u256_or_zero(const osk_rlp_item_t *item, uint8_t out[32])
{
	if (item->kind != OSK_RLP_BYTES)
		return OSK_ERR_FORMAT;
	if (item->len == 0) {
		memset(out, 0, 32);
		return OSK_OK;
	}
	return osk_rlp_as_u256(item, out);
}

static osk_result_t read_u64_or_zero(const osk_rlp_item_t *item, uint64_t *out)
{
	if (item->kind != OSK_RLP_BYTES)
		return OSK_ERR_FORMAT;
	if (item->len == 0) {
		*out = 0;
		return OSK_OK;
	}
	return osk_rlp_as_u64(item, out);
}

osk_result_t osk_eth_parse_tx(const uint8_t *raw, size_t len, osk_eth_tx_t *out)
{
	osk_rlp_reader_t outer, list;
	osk_rlp_item_t item;
	size_t offset = 0;
	osk_result_t res;

	if (raw == NULL || out == NULL || len == 0)
		return OSK_ERR_ARG;
	memset(out, 0, sizeof(*out));

	/* Ein Typbyte unter 0x80 steht fuer die neuen Arten; ab 0xC0 beginnt
	 * direkt eine Liste, und das ist die alte Art. Werte dazwischen gibt
	 * es nicht. */
	if (raw[0] == 0x01u) {
		out->kind = OSK_ETH_ACCESS;
		offset = 1;
	} else if (raw[0] == 0x02u) {
		out->kind = OSK_ETH_FEEMARKET;
		offset = 1;
	} else if (raw[0] >= 0xC0u) {
		out->kind = OSK_ETH_LEGACY;
	} else {
		return OSK_ERR_UNSUPPORTED;
	}

	osk_rlp_reader_init(&outer, raw + offset, len - offset);
	res = osk_rlp_next(&outer, &item);
	if (res != OSK_OK)
		return res;
	if (item.kind != OSK_RLP_LIST || !osk_rlp_at_end(&outer))
		return OSK_ERR_FORMAT; /* Rest hinter der Liste */
	osk_rlp_reader_init(&list, item.data, item.len);

#define NEXT()                                    \
	do {                                      \
		res = osk_rlp_next(&list, &item); \
		if (res != OSK_OK)                \
			return OSK_ERR_FORMAT;    \
	} while (0)

	if (out->kind == OSK_ETH_LEGACY) {
		uint8_t scratch[32];

		NEXT();
		res = read_u64_or_zero(&item, &out->nonce);
		if (res != OSK_OK)
			return res;
		NEXT();
		res = read_u256_or_zero(&item, out->max_fee);
		if (res != OSK_OK)
			return res;
		NEXT();
		res = read_u64_or_zero(&item, &out->gas_limit);
		if (res != OSK_OK)
			return res;
		NEXT();
		res = read_address(&item, out->to, &out->has_to);
		if (res != OSK_OK)
			return res;
		NEXT();
		res = read_u256_or_zero(&item, out->value);
		if (res != OSK_OK)
			return res;
		NEXT();
		if (item.kind != OSK_RLP_BYTES)
			return OSK_ERR_FORMAT;
		out->data_len = item.len;
		NEXT();
		res = read_u64_or_zero(&item, &out->chain_id);
		if (res != OSK_OK)
			return res;
		/* Dann zwei Nullen - so sieht die unsignierte Form nach
		 * EIP-155 aus. Fehlen sie, ist es die alte Form ohne
		 * Kettennummer, und die lehnen wir ab: ohne Kettennummer ist
		 * dieselbe Unterschrift auf jeder EVM-Kette gueltig. */
		NEXT();
		res = read_u256_or_zero(&item, scratch);
		if (res != OSK_OK)
			return res;
		for (unsigned i = 0; i < 32; i++) {
			if (scratch[i] != 0)
				return OSK_ERR_FORMAT;
		}
		NEXT();
		res = read_u256_or_zero(&item, scratch);
		if (res != OSK_OK)
			return res;
		for (unsigned i = 0; i < 32; i++) {
			if (scratch[i] != 0)
				return OSK_ERR_FORMAT;
		}
		if (out->chain_id == 0)
			return OSK_ERR_UNSUPPORTED;
	} else {
		NEXT();
		res = read_u64_or_zero(&item, &out->chain_id);
		if (res != OSK_OK)
			return res;
		NEXT();
		res = read_u64_or_zero(&item, &out->nonce);
		if (res != OSK_OK)
			return res;
		if (out->kind == OSK_ETH_FEEMARKET) {
			NEXT();
			res = read_u256_or_zero(&item, out->max_priority);
			if (res != OSK_OK)
				return res;
		}
		NEXT();
		res = read_u256_or_zero(&item, out->max_fee);
		if (res != OSK_OK)
			return res;
		NEXT();
		res = read_u64_or_zero(&item, &out->gas_limit);
		if (res != OSK_OK)
			return res;
		NEXT();
		res = read_address(&item, out->to, &out->has_to);
		if (res != OSK_OK)
			return res;
		NEXT();
		res = read_u256_or_zero(&item, out->value);
		if (res != OSK_OK)
			return res;
		NEXT();
		if (item.kind != OSK_RLP_BYTES)
			return OSK_ERR_FORMAT;
		out->data_len = item.len;
		NEXT();
		if (item.kind != OSK_RLP_LIST)
			return OSK_ERR_FORMAT;
		out->has_access_list = item.len > 0;
		if (out->chain_id == 0)
			return OSK_ERR_UNSUPPORTED;
	}
#undef NEXT

	if (!osk_rlp_at_end(&list))
		return OSK_ERR_FORMAT; /* mehr Werte als die Art kennt */
	return OSK_OK;
}

osk_result_t osk_eth_tx_hash(const uint8_t *raw, size_t len, uint8_t out[32])
{
	if (raw == NULL || out == NULL || len == 0)
		return OSK_ERR_ARG;
	osk_keccak256(raw, len, out);
	return OSK_OK;
}

osk_result_t osk_eth_view(const osk_eth_tx_t *tx, const char *unit, osk_wallet_view_t *view_out)
{
	uint8_t fee[32];
	char address[43];
	osk_result_t res;

	if (tx == NULL || view_out == NULL)
		return OSK_ERR_ARG;
	osk_wallet_view_init(view_out);

	if (!tx->has_to) {
		view_out->missing_key = "wallet.vertrag-wird-angelegt";
		return OSK_OK;
	}
	res = osk_eth_address_text(tx->to, address, sizeof(address));
	if (res != OSK_OK) {
		view_out->missing_key = "wallet.ausgang-ohne-adresse";
		return OSK_OK;
	}
	res = osk_wallet_shorten(address, view_out->recipient, sizeof(view_out->recipient));
	if (res != OSK_OK) {
		view_out->missing_key = "wallet.adresse-zu-lang";
		return OSK_OK;
	}
	if (osk_wallet_format_amount256(tx->value, 18, unit, view_out->amount,
					sizeof(view_out->amount)) != OSK_OK) {
		view_out->missing_key = "wallet.betrag-nicht-darstellbar";
		return OSK_OK;
	}
	res = fee_total(tx->gas_limit, tx->max_fee, fee);
	if (res != OSK_OK || osk_wallet_format_amount256(fee, 18, unit, view_out->fee,
							 sizeof(view_out->fee)) != OSK_OK) {
		view_out->missing_key = "wallet.gebuehr-nicht-darstellbar";
		return OSK_OK;
	}
	/* Die Kettennummer gehoert auf den Schirm: dieselbe Adresse gibt es
	 * auf jeder EVM-Kette, und der Betrag heisst dort etwas anderes. */
	{
		char chain[24];
		size_t used = 0;
		uint64_t value = tx->chain_id;
		char digits[20];
		size_t count = 0;

		do {
			digits[count++] = (char)('0' + (char)(value % 10u));
			value /= 10u;
		} while (value > 0 && count < sizeof(digits));
		chain[used++] = 'c';
		chain[used++] = 'h';
		chain[used++] = 'a';
		chain[used++] = 'i';
		chain[used++] = 'n';
		chain[used++] = ' ';
		while (count > 0 && used + 1u < sizeof(chain))
			chain[used++] = digits[--count];
		chain[used] = '\0';
		memcpy(view_out->extra, chain, used + 1u);
	}

	/* Daten heisst Vertragsaufruf. Ein Betrag und eine Adresse sagen
	 * dann nicht, was geschieht - also ist es nicht lesbar. */
	if (tx->data_len > 0) {
		view_out->missing_key = "wallet.vertragsaufruf";
		view_out->readable = false;
		return OSK_OK;
	}
	view_out->readable = true;
	return OSK_OK;
}

osk_result_t osk_eth_message_hash(const uint8_t *message, size_t len, uint8_t out[32])
{
	static const char PREFIX[] = "\x19"
				     "Ethereum Signed Message:\n";
	uint8_t head[sizeof(PREFIX) + 20];
	size_t head_len = sizeof(PREFIX) - 1u;
	uint64_t value = len;
	char digits[20];
	size_t count = 0;
	uint8_t once[OSK_KECCAK256_LEN];

	if (message == NULL && len > 0)
		return OSK_ERR_ARG;
	if (out == NULL)
		return OSK_ERR_ARG;
	memcpy(head, PREFIX, head_len);
	do {
		digits[count++] = (char)('0' + (char)(value % 10u));
		value /= 10u;
	} while (value > 0 && count < sizeof(digits));
	while (count > 0)
		head[head_len++] = (uint8_t)digits[--count];

	/* Keccak ueber zwei Stuecke - ohne sie zusammenzukopieren geht das
	 * hier nicht, weil osk_keccak256 nur eine Folge nimmt. Darum wird
	 * eine Obergrenze verlangt; mehr als das passt nicht auf den Schirm
	 * und wird vom Aufrufer abgelehnt. */
	if (len > 512u)
		return OSK_ERR_FULL;
	{
		uint8_t buf[sizeof(head) + 512];

		memcpy(buf, head, head_len);
		if (len > 0)
			memcpy(buf + head_len, message, len);
		osk_keccak256(buf, head_len + len, once);
		osk_wipe(buf, sizeof(buf));
	}
	memcpy(out, once, sizeof(once));
	return OSK_OK;
}

osk_result_t osk_eth_typed_hash(const uint8_t domain[32], const uint8_t message[32],
				uint8_t out[32])
{
	uint8_t buf[2 + 32 + 32];

	if (domain == NULL || message == NULL || out == NULL)
		return OSK_ERR_ARG;
	buf[0] = 0x19u;
	buf[1] = 0x01u;
	memcpy(buf + 2, domain, 32);
	memcpy(buf + 34, message, 32);
	osk_keccak256(buf, sizeof(buf), out);
	return OSK_OK;
}

osk_result_t osk_eth_address(const osk_bip32_priv_t *key, uint8_t out[OSK_ETH_ADDRESS_LEN])
{
	osk_ec_point_t point;
	uint8_t raw[OSK_EC_PUB_UNCOMPRESSED];
	uint8_t digest[OSK_KECCAK256_LEN];
	size_t len = 0;
	osk_result_t res;

	if (key == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (key->curve != OSK_BIP32_SECP256K1)
		return OSK_ERR_UNSUPPORTED;
	res = osk_ec_public_from_private(key->key, &point);
	if (res != OSK_OK)
		return res;
	res = osk_ec_point_encode(&point, false, raw, sizeof(raw), &len);
	if (res != OSK_OK)
		return res;
	/* Ohne das Kopfbyte 0x04 - genau das wird gern vergessen. */
	osk_keccak256(raw + 1, 64, digest);
	memcpy(out, digest + 12, OSK_ETH_ADDRESS_LEN);
	osk_wipe(raw, sizeof(raw));
	osk_wipe(digest, sizeof(digest));
	return OSK_OK;
}

/* Gemeinsamer Weg: Schluessel holen, fragen, unterschreiben, Kennziffer
 * bilden, alles loeschen. */
static osk_result_t sign_hash_with(osk_item_id_t seed_id, const char *asked_by, const char *path,
				   const osk_wallet_view_t *view, bool allow_blind,
				   const uint8_t hash[32], uint64_t v_base, bool eip155,
				   uint64_t chain_id, uint8_t r_out[32], uint8_t s_out[32],
				   uint64_t *v_out)
{
	osk_bip32_priv_t key;
	osk_ec_point_t point;
	uint8_t r[32], s[32];
	uint8_t recid = 0;
	osk_result_t res;

	res = osk_wallet_confirm(view, asked_by, allow_blind);
	if (res != OSK_OK)
		return res;
	res = osk_wallet_seed_derive(seed_id, asked_by, OSK_BIP32_SECP256K1, path, &key);
	if (res != OSK_OK)
		return res;
	res = osk_ec_public_from_private(key.key, &point);
	if (res != OSK_OK) {
		osk_bip32_wipe(&key);
		return res;
	}
	res = osk_signer_sign_hash(OSK_CURVE_SECP256K1, key.key, hash, &point, r, s);
	osk_bip32_wipe(&key);
	if (res != OSK_OK)
		return res;
	res = osk_ec_recovery_id(hash, r, s, &point, &recid);
	if (res != OSK_OK) {
		osk_wipe(r, sizeof(r));
		osk_wipe(s, sizeof(s));
		return res;
	}
	memcpy(r_out, r, 32);
	memcpy(s_out, s, 32);
	if (eip155) {
		/* v = Kennziffer + 35 + 2 * Kettennummer. Ueber 2^63 waere das
		 * ein Ueberlauf - eine solche Kettennummer gibt es nicht. */
		if (chain_id > (0xFFFFFFFFFFFFFFFFULL - 36u) / 2u)
			return OSK_ERR_FORMAT;
		*v_out = (uint64_t)recid + 35u + 2u * chain_id;
	} else {
		*v_out = v_base + (uint64_t)recid;
	}
	osk_wipe(r, sizeof(r));
	osk_wipe(s, sizeof(s));
	return OSK_OK;
}

osk_result_t osk_eth_sign_tx(const uint8_t *raw, size_t len, osk_item_id_t seed_id,
			     const char *asked_by, const char *path, const char *unit,
			     bool allow_blind, uint8_t r_out[32], uint8_t s_out[32],
			     uint64_t *v_out)
{
	osk_eth_tx_t tx;
	osk_wallet_view_t view;
	uint8_t hash[32];
	osk_result_t res;

	if (raw == NULL || r_out == NULL || s_out == NULL || v_out == NULL || path == NULL)
		return OSK_ERR_ARG;
	res = osk_eth_parse_tx(raw, len, &tx);
	if (res != OSK_OK)
		return res;
	res = osk_eth_view(&tx, unit, &view);
	if (res != OSK_OK)
		return res;
	res = osk_eth_tx_hash(raw, len, hash);
	if (res != OSK_OK)
		return res;
	return sign_hash_with(seed_id, asked_by, path, &view, allow_blind, hash, 0,
			      tx.kind == OSK_ETH_LEGACY, tx.chain_id, r_out, s_out, v_out);
}

osk_result_t osk_eth_sign_message(const uint8_t *message, size_t len, osk_item_id_t seed_id,
				  const char *asked_by, const char *path, bool allow_blind,
				  uint8_t r_out[32], uint8_t s_out[32], uint64_t *v_out)
{
	osk_wallet_view_t view;
	uint8_t hash[32];
	osk_result_t res;
	bool printable = len > 0 && len < sizeof(view.recipient);

	if (message == NULL || r_out == NULL || s_out == NULL || v_out == NULL)
		return OSK_ERR_ARG;
	for (size_t i = 0; i < len && printable; i++) {
		if (message[i] < 0x20u || message[i] > 0x7Eu)
			printable = false;
	}
	res = osk_eth_message_hash(message, len, hash);
	if (res != OSK_OK)
		return res;

	osk_wallet_view_init(&view);
	if (printable) {
		memcpy(view.recipient, message, len);
		view.recipient[len] = '\0';
		/* Eine Nachricht hat keinen Betrag und keine Gebuehr. Damit
		 * die Regel aus wallet_confirm.h nicht ins Leere greift,
		 * stehen hier Striche - der Nutzer sieht, dass nichts
		 * ueberwiesen wird. */
		memcpy(view.amount, "-", 2);
		memcpy(view.fee, "-", 2);
		view.readable = true;
	} else {
		view.missing_key = "wallet.nachricht-nicht-lesbar";
	}
	return sign_hash_with(seed_id, asked_by, path, &view, allow_blind, hash, 27u, false, 0,
			      r_out, s_out, v_out);
}

osk_result_t osk_eth_sign_typed(const uint8_t domain[32], const uint8_t message[32],
				osk_item_id_t seed_id, const char *asked_by, const char *path,
				bool allow_blind, uint8_t r_out[32], uint8_t s_out[32],
				uint64_t *v_out)
{
	osk_wallet_view_t view;
	uint8_t hash[32];
	osk_result_t res;

	if (domain == NULL || message == NULL || r_out == NULL || s_out == NULL || v_out == NULL)
		return OSK_ERR_ARG;
	res = osk_eth_typed_hash(domain, message, hash);
	if (res != OSK_OK)
		return res;
	osk_wallet_view_init(&view);
	/* Bewusst nicht lesbar: wir haben nur zwei Hashes, nicht den Inhalt. */
	view.missing_key = "wallet.eip712-inhalt-unbekannt";
	view.readable = false;
	return sign_hash_with(seed_id, asked_by, path, &view, allow_blind, hash, 27u, false, 0,
			      r_out, s_out, v_out);
}
