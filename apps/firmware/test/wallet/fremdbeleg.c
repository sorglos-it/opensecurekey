/* Zahlen fuer den FREMDEN Beleg ausgeben.
 *
 * Warum dieses Programm es gibt:
 *
 * Ein Rundlauf gegen sich selbst geht auch mit einer falschen Umsetzung
 * auf. Wenn der eigene Code eine Signatur bildet und der eigene Code sie
 * nachrechnet, ist bewiesen, dass beide dieselbe Vorstellung haben - und
 * nicht, dass die Vorstellung stimmt. Genau an dieser Stelle hat sich
 * dieses Projekt am 29.10.2026 schon einmal verletzt.
 *
 * Fuer die Schluesselableitung und das Hashen gibt es amtliche
 * Testvektoren (BIP32, EIP-155, EIP-712), und die liegen im Pruefsatz.
 * Fuer die SIGNATUR selbst gibt es keine: sie haengt am Zufallswert k.
 * Also wird sie hier herausgeschrieben und von openssl nachgerechnet -
 * einem Programm, das nichts von diesem Projekt weiss.
 *
 * Dieses Programm prueft nichts. Es schreibt nur Zahlen auf, die
 * fremdbeleg.sh an openssl weitergibt. Was hier herauskommt, ist noch
 * kein Beleg; der Beleg entsteht erst, wenn openssl zustimmt. */
#include <stdio.h>
#include <string.h>
#include "wallet_bip32.h"
#include "wallet_ec.h"
#include "wallet_signer.h"
#include "wallet_bip39.h"
#include "osk_crypto.h"

static void print_hex(const char *name, const uint8_t *p, size_t len)
{
	printf("%s=", name);
	for (size_t i = 0; i < len; i++)
		printf("%02x", p[i]);
	printf("\n");
}

int main(void)
{
	/* Der Seed aus BIP39 mit dem bekannten Satz - derselbe, den der
	 * Pruefsatz benutzt. */
	static const char *const MNEMONIC =
	    "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon"
	    " abandon about";
	uint8_t seed[OSK_BIP39_SEED_LEN];
	osk_bip32_priv_t master, key;
	osk_bip32_pub_t pub;
	osk_ec_point_t point;
	uint8_t hash[32];
	uint8_t r[32], s[32];
	uint8_t der[80];
	uint8_t uncompressed[65];
	size_t der_len = 0, unc_len = 0;

	if (osk_bip39_to_seed(MNEMONIC, NULL, seed) != OSK_OK) {
		fprintf(stderr, "Seed ging nicht\n");
		return 1;
	}
	if (osk_bip32_master(seed, sizeof(seed), OSK_BIP32_SECP256K1, &master) != OSK_OK) {
		fprintf(stderr, "Wurzel ging nicht\n");
		return 1;
	}
	/* Der Pfad, den eine Bitcoin-Geldboerse nach BIP84 nimmt. */
	if (osk_bip32_derive_path(&master, "m/84'/0'/0'/0/0", &key) != OSK_OK) {
		fprintf(stderr, "Ableitung ging nicht\n");
		return 1;
	}
	if (osk_bip32_neuter(&key, &pub) != OSK_OK) {
		fprintf(stderr, "Neutern ging nicht\n");
		return 1;
	}
	if (osk_ec_point_decode(pub.pub, OSK_EC_PUB_COMPRESSED, &point) != OSK_OK) {
		fprintf(stderr, "Punkt ging nicht\n");
		return 1;
	}
	/* openssl will den Punkt unkomprimiert sehen. */
	if (osk_ec_point_encode(&point, false, uncompressed, sizeof(uncompressed), &unc_len) !=
	    OSK_OK) {
		fprintf(stderr, "Punkt unkomprimiert ging nicht\n");
		return 1;
	}

	/* Etwas zu unterschreiben: der Hash eines festen Textes. Fest,
	 * damit ein Lauf wiederholbar ist. */
	osk_sha256((const uint8_t *)"openSecureKey Fremdbeleg", 24u, hash);

	if (osk_signer_sign_hash(OSK_CURVE_SECP256K1, key.key, hash, &point, r, s) != OSK_OK) {
		fprintf(stderr, "Unterschrift ging nicht\n");
		return 1;
	}
	if (osk_signer_raw_to_der(r, s, der, sizeof(der), &der_len) != OSK_OK) {
		fprintf(stderr, "DER ging nicht\n");
		return 1;
	}

	/* Der eigene private Schluessel - damit openssl den oeffentlichen
	 * selbst ausrechnen und mit unserem vergleichen kann. */
	print_hex("PRIV", key.key, 32);
	print_hex("PUB_COMPRESSED", pub.pub, OSK_EC_PUB_COMPRESSED);
	print_hex("PUB_UNCOMPRESSED", uncompressed, unc_len);
	print_hex("HASH", hash, sizeof(hash));
	print_hex("SIG_DER", der, der_len);

	osk_bip32_wipe(&master);
	osk_bip32_wipe(&key);
	osk_wipe(seed, sizeof(seed));
	osk_wipe(r, sizeof(r));
	osk_wipe(s, sizeof(s));
	return 0;
}
