/* BIP32 / SLIP-0010, selbst geschrieben gegen die Spezifikationen. */

#include <string.h>
#include "wallet_bip32.h"
#include "wallet_hash.h"
#include "wallet_codec.h"
#include "osk_crypto.h"

static void store_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)((v >> 24) & 0xFFu);
	p[1] = (uint8_t)((v >> 16) & 0xFFu);
	p[2] = (uint8_t)((v >> 8) & 0xFFu);
	p[3] = (uint8_t)(v & 0xFFu);
}

static uint32_t load_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
	       (uint32_t)p[3];
}

osk_result_t osk_bip32_master(const uint8_t *seed, size_t seed_len, osk_bip32_curve_t curve,
			      osk_bip32_priv_t *out)
{
	static const char SEED_SECP[] = "Bitcoin seed";
	static const char SEED_ED[] = "ed25519 seed";
	uint8_t mac[OSK_SHA512_LEN];
	const char *label;

	if (seed == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (seed_len < 16u || seed_len > 64u)
		return OSK_ERR_ARG;
	if (curve != OSK_BIP32_SECP256K1 && curve != OSK_BIP32_ED25519)
		return OSK_ERR_ARG;

	label = (curve == OSK_BIP32_ED25519) ? SEED_ED : SEED_SECP;
	osk_hmac_sha512((const uint8_t *)label, strlen(label), seed, seed_len, mac);

	/* secp256k1: ist die linke Haelfte 0 oder >= n, wird dieselbe Rechnung
	 * auf das Ergebnis wiederholt (SLIP-0010). Das tritt mit einer
	 * Wahrscheinlichkeit von etwa 2^-127 ein; der Zweig ist trotzdem da,
	 * weil ein Seed aus fremder Quelle ihn treffen kann. */
	while (curve == OSK_BIP32_SECP256K1 && !osk_ec_privkey_valid(mac))
		osk_hmac_sha512((const uint8_t *)label, strlen(label), mac, sizeof(mac), mac);

	memset(out, 0, sizeof(*out));
	out->curve = curve;
	memcpy(out->key, mac, OSK_BIP32_KEY_LEN);
	memcpy(out->chain, mac + 32, OSK_BIP32_CHAIN_LEN);
	osk_wipe(mac, sizeof(mac));
	return OSK_OK;
}

uint32_t osk_bip32_fingerprint(const uint8_t pub[OSK_EC_PUB_COMPRESSED])
{
	uint8_t id[OSK_RIPEMD160_LEN];
	uint32_t value;

	osk_hash160(pub, OSK_EC_PUB_COMPRESSED, id);
	value = load_be32(id);
	osk_wipe(id, sizeof(id));
	return value;
}

static osk_result_t compressed_public(const osk_bip32_priv_t *in,
				      uint8_t out[OSK_EC_PUB_COMPRESSED])
{
	osk_ec_point_t point;
	size_t len = 0;
	osk_result_t res;

	if (in->curve != OSK_BIP32_SECP256K1)
		return OSK_ERR_UNSUPPORTED;
	res = osk_ec_public_from_private(in->key, &point);
	if (res != OSK_OK)
		return res;
	return osk_ec_point_encode(&point, true, out, OSK_EC_PUB_COMPRESSED, &len);
}

osk_result_t osk_bip32_derive_child(const osk_bip32_priv_t *parent, uint32_t index,
				    osk_bip32_priv_t *out)
{
	uint8_t data[1 + OSK_BIP32_KEY_LEN + 4];
	uint8_t mac[OSK_SHA512_LEN];
	uint8_t pub[OSK_EC_PUB_COMPRESSED];
	bool hardened = (index & OSK_BIP32_HARDENED) != 0;
	osk_result_t res;

	if (parent == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (parent->depth >= OSK_BIP32_MAX_DEPTH)
		return OSK_ERR_FULL;
	if (parent->curve == OSK_BIP32_ED25519 && !hardened)
		return OSK_ERR_UNSUPPORTED;

	if (hardened) {
		data[0] = 0x00u;
		memcpy(data + 1, parent->key, OSK_BIP32_KEY_LEN);
	} else {
		res = compressed_public(parent, pub);
		if (res != OSK_OK)
			return res;
		memcpy(data, pub, OSK_EC_PUB_COMPRESSED);
	}
	store_be32(data + 1 + OSK_BIP32_KEY_LEN, index);
	osk_hmac_sha512(parent->chain, OSK_BIP32_CHAIN_LEN, data, sizeof(data), mac);

	memset(out, 0, sizeof(*out));
	out->curve = parent->curve;
	out->depth = (uint8_t)(parent->depth + 1u);
	out->child = index;

	if (parent->curve == OSK_BIP32_ED25519) {
		/* SLIP-0010: der linke Teil IST der Schluessel, nichts wird
		 * addiert und nichts geprueft. */
		memcpy(out->key, mac, OSK_BIP32_KEY_LEN);
		memcpy(out->chain, mac + 32, OSK_BIP32_CHAIN_LEN);
		osk_wipe(data, sizeof(data));
		osk_wipe(mac, sizeof(mac));
		return OSK_OK;
	}

	/* secp256k1: k_child = IL + k_parent mod n. Ist IL >= n oder die
	 * Summe 0, wird nach SLIP-0010 mit 0x01 || IR || index wiederholt. */
	for (;;) {
		res = osk_ec_priv_tweak_add(parent->key, mac, out->key);
		if (res == OSK_OK) {
			memcpy(out->chain, mac + 32, OSK_BIP32_CHAIN_LEN);
			break;
		}
		if (res != OSK_ERR_FORMAT) {
			osk_bip32_wipe(out);
			osk_wipe(data, sizeof(data));
			osk_wipe(mac, sizeof(mac));
			return res;
		}
		data[0] = 0x01u;
		memcpy(data + 1, mac + 32, OSK_BIP32_CHAIN_LEN);
		store_be32(data + 1 + OSK_BIP32_KEY_LEN, index);
		osk_hmac_sha512(parent->chain, OSK_BIP32_CHAIN_LEN, data, sizeof(data), mac);
	}

	res = compressed_public(parent, pub);
	out->parent_fp = (res == OSK_OK) ? osk_bip32_fingerprint(pub) : 0u;
	osk_wipe(data, sizeof(data));
	osk_wipe(mac, sizeof(mac));
	osk_wipe(pub, sizeof(pub));
	return OSK_OK;
}

osk_result_t osk_bip32_pub_derive_child(const osk_bip32_pub_t *parent, uint32_t index,
					osk_bip32_pub_t *out)
{
	uint8_t data[OSK_EC_PUB_COMPRESSED + 4];
	uint8_t mac[OSK_SHA512_LEN];
	osk_ec_point_t point, child;
	size_t len = 0;
	osk_result_t res;

	if (parent == NULL || out == NULL)
		return OSK_ERR_ARG;
	if ((index & OSK_BIP32_HARDENED) != 0)
		return OSK_ERR_UNSUPPORTED;
	if (parent->depth >= OSK_BIP32_MAX_DEPTH)
		return OSK_ERR_FULL;

	res = osk_ec_point_decode(parent->pub, OSK_EC_PUB_COMPRESSED, &point);
	if (res != OSK_OK)
		return res;

	memcpy(data, parent->pub, OSK_EC_PUB_COMPRESSED);
	store_be32(data + OSK_EC_PUB_COMPRESSED, index);
	osk_hmac_sha512(parent->chain, OSK_BIP32_CHAIN_LEN, data, sizeof(data), mac);
	res = osk_ec_pub_tweak_add(&point, mac, &child);
	if (res != OSK_OK) {
		osk_wipe(mac, sizeof(mac));
		return res;
	}
	memset(out, 0, sizeof(*out));
	res = osk_ec_point_encode(&child, true, out->pub, sizeof(out->pub), &len);
	if (res != OSK_OK) {
		osk_wipe(mac, sizeof(mac));
		return res;
	}
	memcpy(out->chain, mac + 32, OSK_BIP32_CHAIN_LEN);
	out->depth = (uint8_t)(parent->depth + 1u);
	out->child = index;
	out->parent_fp = osk_bip32_fingerprint(parent->pub);
	osk_wipe(mac, sizeof(mac));
	return OSK_OK;
}

osk_result_t osk_bip32_path_parse(const char *text, uint32_t *out, size_t cap, size_t *len_out)
{
	size_t count = 0, i = 0;

	if (text == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (text[0] == 'm' || text[0] == 'M') {
		i = 1;
		if (text[1] == '\0') {
			*len_out = 0;
			return OSK_OK;
		}
		if (text[1] != '/')
			return OSK_ERR_FORMAT;
		i = 2;
	}
	if (text[i] == '\0')
		return OSK_ERR_FORMAT;
	for (;;) {
		uint32_t value = 0;
		size_t digits = 0;

		while (text[i] >= '0' && text[i] <= '9') {
			uint32_t digit = (uint32_t)(text[i] - '0');

			if (value > (0x7FFFFFFFu - digit) / 10u)
				return OSK_ERR_FORMAT; /* ueber 2^31-1 gibt es nicht */
			value = value * 10u + digit;
			digits++;
			i++;
		}
		if (digits == 0 || digits > 10u)
			return OSK_ERR_FORMAT;
		if (text[i] == '\'' || text[i] == 'h' || text[i] == 'H') {
			value |= OSK_BIP32_HARDENED;
			i++;
		}
		if (count >= cap || count >= OSK_BIP32_MAX_DEPTH)
			return OSK_ERR_FULL;
		out[count++] = value;
		if (text[i] == '\0')
			break;
		if (text[i] != '/')
			return OSK_ERR_FORMAT;
		i++;
		if (text[i] == '\0')
			return OSK_ERR_FORMAT; /* Pfad endet auf einem Schraegstrich */
	}
	*len_out = count;
	return OSK_OK;
}

osk_result_t osk_bip32_derive_path(const osk_bip32_priv_t *master, const char *path,
				   osk_bip32_priv_t *out)
{
	uint32_t steps[OSK_BIP32_MAX_DEPTH];
	size_t count = 0;
	osk_bip32_priv_t current;
	osk_result_t res;

	if (master == NULL || out == NULL)
		return OSK_ERR_ARG;
	res = osk_bip32_path_parse(path, steps, OSK_BIP32_MAX_DEPTH, &count);
	if (res != OSK_OK)
		return res;
	current = *master;
	for (size_t i = 0; i < count; i++) {
		osk_bip32_priv_t next;

		res = osk_bip32_derive_child(&current, steps[i], &next);
		osk_bip32_wipe(&current);
		if (res != OSK_OK)
			return res;
		current = next;
		osk_wipe(&next, sizeof(next));
	}
	*out = current;
	osk_wipe(&current, sizeof(current));
	return OSK_OK;
}

osk_result_t osk_bip32_neuter(const osk_bip32_priv_t *in, osk_bip32_pub_t *out)
{
	osk_result_t res;

	if (in == NULL || out == NULL)
		return OSK_ERR_ARG;
	memset(out, 0, sizeof(*out));
	res = compressed_public(in, out->pub);
	if (res != OSK_OK)
		return res;
	memcpy(out->chain, in->chain, OSK_BIP32_CHAIN_LEN);
	out->depth = in->depth;
	out->child = in->child;
	out->parent_fp = in->parent_fp;
	return OSK_OK;
}

static osk_result_t serialize(uint32_t version, uint8_t depth, uint32_t parent_fp, uint32_t child,
			      const uint8_t chain[32], const uint8_t payload[33], char *out,
			      size_t cap, size_t *len_out)
{
	uint8_t raw[OSK_BIP32_SERIALIZED];

	store_be32(raw, version);
	raw[4] = depth;
	store_be32(raw + 5, parent_fp);
	store_be32(raw + 9, child);
	memcpy(raw + 13, chain, 32);
	memcpy(raw + 45, payload, 33);
	{
		osk_result_t res = osk_base58check_encode(raw, sizeof(raw), out, cap, len_out);

		osk_wipe(raw, sizeof(raw));
		return res;
	}
}

osk_result_t osk_bip32_serialize_pub(const osk_bip32_pub_t *in, uint32_t version, char *out,
				     size_t cap, size_t *len_out)
{
	if (in == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	return serialize(version, in->depth, in->parent_fp, in->child, in->chain, in->pub, out, cap,
			 len_out);
}

osk_result_t osk_bip32_serialize_priv(const osk_bip32_priv_t *in, uint32_t version, char *out,
				      size_t cap, size_t *len_out)
{
	uint8_t payload[33];
	osk_result_t res;

	if (in == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	payload[0] = 0x00u; /* so trennt BIP32 den privaten vom oeffentlichen Teil */
	memcpy(payload + 1, in->key, OSK_BIP32_KEY_LEN);
	res = serialize(version, in->depth, in->parent_fp, in->child, in->chain, payload, out, cap,
			len_out);
	osk_wipe(payload, sizeof(payload));
	return res;
}

osk_result_t osk_bip32_parse_pub(const char *text, uint32_t expect_version, osk_bip32_pub_t *out)
{
	uint8_t raw[OSK_BASE58_MAX_BIN];
	size_t len = 0;
	osk_result_t res;

	if (text == NULL || out == NULL)
		return OSK_ERR_ARG;
	res = osk_base58check_decode(text, raw, sizeof(raw), &len);
	if (res != OSK_OK)
		return res;
	if (len != OSK_BIP32_SERIALIZED)
		return OSK_ERR_FORMAT;
	if (load_be32(raw) != expect_version)
		return OSK_ERR_FORMAT;
	if (raw[45] != 0x02u && raw[45] != 0x03u)
		return OSK_ERR_FORMAT;
	/* Tiefe 0 ist die Wurzel: sie hat keinen Elternknoten und keinen
	 * Index. Steht dort etwas, ist der Schluessel falsch zusammengesetzt -
	 * BIP32 fuehrt genau diese zwei Faelle in seiner Liste ungueltiger
	 * Schluessel auf. */
	if (raw[4] == 0 && (load_be32(raw + 5) != 0 || load_be32(raw + 9) != 0))
		return OSK_ERR_FORMAT;
	memset(out, 0, sizeof(*out));
	out->depth = raw[4];
	out->parent_fp = load_be32(raw + 5);
	out->child = load_be32(raw + 9);
	memcpy(out->chain, raw + 13, 32);
	memcpy(out->pub, raw + 45, 33);
	/* Liegt der Punkt wirklich auf der Kurve? Ein xpub von aussen darf
	 * nicht ungeprueft weiterwandern. */
	{
		osk_ec_point_t point;

		if (osk_ec_point_decode(out->pub, OSK_EC_PUB_COMPRESSED, &point) != OSK_OK)
			return OSK_ERR_FORMAT;
	}
	return OSK_OK;
}

void osk_bip32_wipe(osk_bip32_priv_t *key)
{
	if (key != NULL)
		osk_wipe(key, sizeof(*key));
}
