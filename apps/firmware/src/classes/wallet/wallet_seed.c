/* Umsetzung von wallet_seed.h. */

#include <string.h>
#include "wallet_seed.h"
#include "wallet_hash.h"
#include "osk_crypto.h"

osk_result_t osk_wallet_seed_store(uint8_t profile, const char *name, const char *mnemonic,
				   const char *passphrase, osk_item_id_t *id_out)
{
	uint8_t seed[OSK_BIP39_SEED_LEN];
	osk_item_meta_t meta;
	osk_result_t res;

	if (name == NULL || mnemonic == NULL || id_out == NULL)
		return OSK_ERR_ARG;
	res = osk_bip39_check(mnemonic);
	if (res != OSK_OK)
		return res;
	res = osk_bip39_to_seed(mnemonic, passphrase, seed);
	if (res != OSK_OK) {
		osk_wipe(seed, sizeof(seed));
		return res;
	}

	memset(&meta, 0, sizeof(meta));
	meta.type = OSK_ITEM_SEED;
	meta.profile = profile;
	meta.name = name;
	/* Ein Seed wird nie auf einen zweiten Stick uebertragen und nie an
	 * einen Rechner herausgegeben. Beides steht hier, damit der Tresor es
	 * durchsetzt und nicht der Aufrufer daran denken muss. */
	meta.mirrored = false;
	meta.paired_host_only = true;
	/* Suchschluessel: der Fingerabdruck des Wurzelschluessels. Damit
	 * findet PSBT den richtigen Seed, ohne einen einzigen zu oeffnen. */
	{
		osk_bip32_priv_t master;
		osk_bip32_pub_t pub;

		if (osk_bip32_master(seed, sizeof(seed), OSK_BIP32_SECP256K1, &master) == OSK_OK &&
		    osk_bip32_neuter(&master, &pub) == OSK_OK) {
			uint32_t fp = osk_bip32_fingerprint(pub.pub);

			meta.tag[0] = (uint8_t)((fp >> 24) & 0xFFu);
			meta.tag[1] = (uint8_t)((fp >> 16) & 0xFFu);
			meta.tag[2] = (uint8_t)((fp >> 8) & 0xFFu);
			meta.tag[3] = (uint8_t)(fp & 0xFFu);
			meta.tag_len = 4;
		}
		osk_bip32_wipe(&master);
	}

	res = osk_store_add(&meta, seed, sizeof(seed), id_out);
	osk_wipe(seed, sizeof(seed));
	return res;
}

osk_result_t osk_wallet_seed_list(uint8_t profile, osk_item_id_t *out, size_t cap, size_t *len_out)
{
	size_t count = 0, found = 0;
	osk_result_t res;

	if (out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	res = osk_store_count(profile, &count);
	if (res != OSK_OK)
		return res;
	for (size_t i = 0; i < count; i++) {
		osk_item_meta_t meta;

		res = osk_store_list(profile, i, &meta);
		if (res != OSK_OK)
			return res;
		if (meta.type != OSK_ITEM_SEED)
			continue;
		if (found >= cap)
			return OSK_ERR_FULL;
		out[found++] = meta.id;
	}
	*len_out = found;
	return OSK_OK;
}

osk_result_t osk_wallet_seed_derive(osk_item_id_t id, const char *asked_by, osk_bip32_curve_t curve,
				    const char *path, osk_bip32_priv_t *out)
{
	uint8_t seed[OSK_BIP39_SEED_LEN];
	size_t len = 0;
	osk_bip32_priv_t master;
	osk_result_t res;

	if (out == NULL || path == NULL)
		return OSK_ERR_ARG;
	memset(out, 0, sizeof(*out));
	res = osk_store_reveal(id, asked_by, seed, sizeof(seed), &len);
	if (res != OSK_OK) {
		osk_wipe(seed, sizeof(seed));
		return res;
	}
	/* Ein Seed, der nicht 16 bis 64 Byte hat, ist keiner - dann ist der
	 * Eintrag von etwas anderem oder beschaedigt. */
	if (len < 16u || len > sizeof(seed)) {
		osk_wipe(seed, sizeof(seed));
		return OSK_ERR_FORMAT;
	}
	res = osk_bip32_master(seed, len, curve, &master);
	osk_wipe(seed, sizeof(seed));
	if (res != OSK_OK)
		return res;
	res = osk_bip32_derive_path(&master, path, out);
	osk_bip32_wipe(&master);
	if (res != OSK_OK)
		osk_bip32_wipe(out);
	return res;
}
