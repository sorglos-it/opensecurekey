/* BIP32 gegen die amtlichen Testvektoren der Spezifikation (bip-0032,
 * Abschnitt "Test Vectors"), einschliesslich der Vektoren 3 und 4, die
 * fuehrende Nullen pruefen, und der Liste ungueltiger Schluessel aus
 * Vektor 5. Pruefdaten, kein Fremdcode. */

#include "test_util.h"
#include "wallet_bip32.h"
#include "mocks.h"

struct node {
	const char *path;
	const char *xpub;
	const char *xprv;
};

static void check_chain(const char *seed_hex, const struct node *nodes, size_t count)
{
	uint8_t seed[64];
	size_t seed_len;
	osk_bip32_priv_t master;

	seed_len = t_unhex(seed_hex, seed, sizeof(seed));
	T_TRUE("Seed gelesen", seed_len >= 16u);
	T_EQ("Wurzel", osk_bip32_master(seed, seed_len, OSK_BIP32_SECP256K1, &master), OSK_OK);

	for (size_t i = 0; i < count; i++) {
		osk_bip32_priv_t priv;
		osk_bip32_pub_t pub;
		char text[OSK_BIP32_TEXT_MAX];
		size_t len = 0;

		T_EQ("abgeleitet", osk_bip32_derive_path(&master, nodes[i].path, &priv), OSK_OK);
		T_EQ("xprv",
		     osk_bip32_serialize_priv(&priv, OSK_BIP32_XPRV_MAIN, text, sizeof(text), &len),
		     OSK_OK);
		if (strcmp(text, nodes[i].xprv) != 0)
			printf("FEHLER %s xprv\n  bekam    %s\n  erwartet %s\n", nodes[i].path,
			       text, nodes[i].xprv);
		T_TRUE("xprv stimmt", strcmp(text, nodes[i].xprv) == 0);

		T_EQ("oeffentlich", osk_bip32_neuter(&priv, &pub), OSK_OK);
		T_EQ("xpub",
		     osk_bip32_serialize_pub(&pub, OSK_BIP32_XPUB_MAIN, text, sizeof(text), &len),
		     OSK_OK);
		if (strcmp(text, nodes[i].xpub) != 0)
			printf("FEHLER %s xpub\n  bekam    %s\n  erwartet %s\n", nodes[i].path,
			       text, nodes[i].xpub);
		T_TRUE("xpub stimmt", strcmp(text, nodes[i].xpub) == 0);

		/* Und zurueck: der gelesene xpub muss derselbe Knoten sein. */
		{
			osk_bip32_pub_t back;

			T_EQ("xpub gelesen",
			     osk_bip32_parse_pub(nodes[i].xpub, OSK_BIP32_XPUB_MAIN, &back),
			     OSK_OK);
			T_TRUE("Punkt gleich", memcmp(back.pub, pub.pub, sizeof(pub.pub)) == 0);
			T_TRUE("Kette gleich", memcmp(back.chain, pub.chain, 32) == 0);
			T_EQ("Tiefe gleich", back.depth, pub.depth);
			T_EQ("Index gleich", (long long)back.child, (long long)pub.child);
			T_EQ("Elternabdruck gleich", (long long)back.parent_fp,
			     (long long)pub.parent_fp);
		}
		osk_bip32_wipe(&priv);
	}
	osk_bip32_wipe(&master);
}

static const struct node VECTOR1[] = {
    {"m",
     "xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8Yt"
     "GqsefD265TMg7usUDFdp6W1EGMcet8",
     "xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5"
     "kejMRNNU3TGtRBeJgk33yuGBxrMPHi"},
    {"m/0'",
     "xpub68Gmy5EdvgibQVfPdqkBBCHxA5htiqg55crXYuXoQRKfDBFA1WEjWgP6LHhwBZeNK1VTsfTFUHCdr"
     "fp1bgwQ9xv5ski8PX9rL2dZXvgGDnw",
     "xprv9uHRZZhk6KAJC1avXpDAp4MDc3sQKNxDiPvvkX8Br5ngLNv1TxvUxt4cV1rGL5hj6KCesnDYUhd7o"
     "WgT11eZG7XnxHrnYeSvkzY7d2bhkJ7"},
    {"m/0'/1",
     "xpub6ASuArnXKPbfEwhqN6e3mwBcDTgzisQN1wXN9BJcM47sSikHjJf3UFHKkNAWbWMiGj7Wf5uMash7S"
     "yYq527Hqck2AxYysAA7xmALppuCkwQ",
     "xprv9wTYmMFdV23N2TdNG573QoEsfRrWKQgWeibmLntzniatZvR9BmLnvSxqu53Kw1UmYPxLgboyZQaXw"
     "TCg8MSY3H2EU4pWcQDnRnrVA1xe8fs"},
    {"m/0'/1/2'",
     "xpub6D4BDPcP2GT577Vvch3R8wDkScZWzQzMMUm3PWbmWvVJrZwQY4VUNgqFJPMM3No2dFDFGTsxxpG5u"
     "Jh7n7epu4trkrX7x7DogT5Uv6fcLW5",
     "xprv9z4pot5VBttmtdRTWfWQmoH1taj2axGVzFqSb8C9xaxKymcFzXBDptWmT7FwuEzG3ryjH4ktypQSA"
     "ewRiNMjANTtpgP4mLTj34bhnZX7UiM"},
    {"m/0'/1/2'/2",
     "xpub6FHa3pjLCk84BayeJxFW2SP4XRrFd1JYnxeLeU8EqN3vDfZmbqBqaGJAyiLjTAwm6ZLRQUMv1ZACT"
     "j37sR62cfN7fe5JnJ7dh8zL4fiyLHV",
     "xprvA2JDeKCSNNZky6uBCviVfJSKyQ1mDYahRjijr5idH2WwLsEd4Hsb2Tyh8RfQMuPh7f7RtyzTtdrbd"
     "qqsunu5Mm3wDvUAKRHSC34sJ7in334"},
    {"m/0'/1/2'/2/1000000000",
     "xpub6H1LXWLaKsWFhvm6RVpEL9P4KfRZSW7abD2ttkWP3SSQvnyA8FSVqNTEcYFgJS2UaFcxupHiYkro4"
     "9S8yGasTvXEYBVPamhGW6cFJodrTHy",
     "xprvA41z7zogVVwxVSgdKUHDy1SKmdb533PjDz7J6N6mV6uS3ze1ai8FHa8kmHScGpWmj4WggLyQjgPie"
     "1rFSruoUihUZREPSL39UNdE3BBDu76"},
};

static const struct node VECTOR2[] = {
    {"m",
     "xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6oDMSgv6Zz8idoc"
     "4a6mr8BDzTJY47LJhkJ8UB7WEGuduB",
     "xprv9s21ZrQH143K31xYSDQpPDxsXRTUcvj2iNHm5NUtrGiGG5e2DtALGdso3pGz6ssrdK4PFmM8NSpSB"
     "HNqPqm55Qn3LqFtT2emdEXVYsCzC2U"},
    {"m/0",
     "xpub69H7F5d8KSRgmmdJg2KhpAK8SR3DjMwAdkxj3ZuxV27CprR9LgpeyGmXUbC6wb7ERfvrnKZjXoUmm"
     "DznezpbZb7ap6r1D3tgFxHmwMkQTPH",
     "xprv9vHkqa6EV4sPZHYqZznhT2NPtPCjKuDKGY38FBWLvgaDx45zo9WQRUT3dKYnjwih2yJD9mkrocEZX"
     "o1ex8G81dwSM1fwqWpWkeS3v86pgKt"},
    {"m/0/2147483647'",
     "xpub6ASAVgeehLbnwdqV6UKMHVzgqAG8Gr6riv3Fxxpj8ksbH9ebxaEyBLZ85ySDhKiLDBrQSARLq1uNR"
     "ts8RuJiHjaDMBU4Zn9h8LZNnBC5y4a",
     "xprv9wSp6B7kry3Vj9m1zSnLvN3xH8RdsPP1Mh7fAaR7aRLcQMKTR2vidYEeEg2mUCTAwCd6vnxVrcjfy"
     "2kRgVsFawNzmjuHc2YmYRmagcEPdU9"},
    {"m/0/2147483647'/1",
     "xpub6DF8uhdarytz3FWdA8TvFSvvAh8dP3283MY7p2V4SeE2wyWmG5mg5EwVvmdMVCQcoNJxGoWaU9DCW"
     "h89LojfZ537wTfunKau47EL2dhHKon",
     "xprv9zFnWC6h2cLgpmSA46vutJzBcfJ8yaJGg8cX1e5StJh45BBciYTRXSd25UEPVuesF9yog62tGAQtH"
     "jXajPPdbRCHuWS6T8XA2ECKADdw4Ef"},
    {"m/0/2147483647'/1/2147483646'",
     "xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4koxb5JaHWkY4A"
     "LHY2grBGRjaDMzQLcgJvLJuZZvRcEL",
     "xprvA1RpRA33e1JQ7ifknakTFpgNXPmW2YvmhqLQYMmrj4xJXXWYpDPS3xz7iAxn8L39njGVyuoseXzU6"
     "rcxFLJ8HFsTjSyQbLYnMpCqE2VbFWc"},
    {"m/0/2147483647'/1/2147483646'/2",
     "xpub6FnCn6nSzZAw5Tw7cgR9bi15UV96gLZhjDstkXXxvCLsUXBGXPdSnLFbdpq8p9HmGsApME5hQTZ3e"
     "mM2rnY5agb9rXpVGyy3bdW6EEgAtqt",
     "xprvA2nrNbFZABcdryreWet9Ea4LvTJcGsqrMzxHx98MMrotbir7yrKCEXw7nadnHM8Dq38EGfSh6dqA9"
     "QWTyefMLEcBYJUuekgW4BYPJcr9E7j"},
};

/* Vektor 3 und 4 pruefen fuehrende Nullen - genau dort hat es bei anderen
 * Umsetzungen Schluessel verschluckt. */
static const struct node VECTOR3[] = {
    {"m",
     "xpub661MyMwAqRbcEZVB4dScxMAdx6d4nFc9nvyvH3v4gJL378CSRZiYmhRoP7mBy6gSPSCYk6SzXPTf3"
     "ND1cZAceL7SfJ1Z3GC8vBgp2epUt13",
     "xprv9s21ZrQH143K25QhxbucbDDuQ4naNntJRi4KUfWT7xo4EKsHt2QJDu7KXp1A3u7Bi1j8ph3EGsZ9X"
     "vz9dGuVrtHHs7pXeTzjuxBrCmmhgC6"},
    {"m/0'",
     "xpub68NZiKmJWnxxS6aaHmn81bvJeTESw724CRDs6HbuccFQN9Ku14VQrADWgqbhhTHBaohPX4CjNLf9f"
     "q9MYo6oDaPPLPxSb7gwQN3ih19Zm4Y",
     "xprv9uPDJpEQgRQfDcW7BkF7eTya6RPxXeJCqCJGHuCJ4GiRVLzkTXBAJMu2qaMWPrS7AANYqdq6vcBcB"
     "UdJCVVFceUvJFjaPdGZ2y9WACViL4L"},
};

static const struct node VECTOR4[] = {
    {"m",
     "xpub661MyMwAqRbcGczjuMoRm6dXaLDEhW1u34gKenbeYqAix21mdUKJyuyu5F1rzYGVxyL6tmgBUAEPr"
     "Ez92mBXjByMRiJdba9wpnN37RLLAXa",
     "xprv9s21ZrQH143K48vGoLGRPxgo2JNkJ3J3fqkirQC2zVdk5Dgd5w14S7fRDyHH4dWNHUgkvsvNDCkvA"
     "wcSHNAQwhwgNMgZhLtQC63zxwhQmRv"},
    {"m/0'",
     "xpub69AUMk3qDBi3uW1sXgjCmVjJ2G6WQoYSnNHyzkmdCHEhSZ4tBok37xfFEqHd2AddP56Tqp4o56AeP"
     "AgCjYdvpW2PU2jbUPFKsav5ut6Ch1m",
     "xprv9vB7xEWwNp9kh1wQRfCCQMnZUEG21LpbR9NPCNN1dwhiZkjjeGRnaALmPXCX7SgjFTiCTT6bXes17"
     "boXtjq3xLpcDjzEuGLQBM5ohqkao9G"},
    {"m/0'/1'",
     "xpub6BJA1jSqiukeaesWfxe6sNK9CCGaujFFSJLomWHprUL9DePQ4JDkM5d88n49sMGJxrhpjazuXYWdM"
     "f17C9T5XnxkopaeS7jGk1GyyVziaMt",
     "xprv9xJocDuwtYCMNAo3Zw76WENQeAS6WGXQ55RCy7tDJ8oALr4FWkuVoHJeHVAcAqiZLE7Je3vZJHxsp"
     "ZdFHfnBEjHqU5hG1Jaj32dVoS6XLT1"},
};

/* Vektor 5: alles hiervon muss abgelehnt werden. Die xprv-Faelle ebenso -
 * osk_bip32_parse_pub darf sie nicht als xpub durchlassen. */
static const char *const INVALID[] = {
    "xpub661MyMwAqRbcEYS8w7XLSVeEsBXy79zSzH1J8vCdxAZningWLdN3zgtU6LBpB85b3D2yc8sfvZU521AAw"
    "dZafEz7mnzBBsz4wKY5fTtTQBm",
    "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzFGTQQD3dC4H2D5GBj7vWvSQaa"
    "Bv5cxi9gafk7NF3pnBju6dwKvH",
    "xpub661MyMwAqRbcEYS8w7XLSVeEsBXy79zSzH1J8vCdxAZningWLdN3zgtU6Txnt3siSujt9RCVYsx4qHZGc"
    "62TG4McvMGcAUjeuwZdduYEvFn",
    "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzFGpWnsj83BHtEy5Zt8CcDr1Ui"
    "RXuWCmTQLxEK9vbz5gPstX92JQ",
    "xpub661MyMwAqRbcEYS8w7XLSVeEsBXy79zSzH1J8vCdxAZningWLdN3zgtU6N8ZMMXctdiCjxTNq964yKkwr"
    "kBJJwpzZS4HS2fxvyYUA4q2Xe4",
    "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzFAzHGBP2UuGCqWLTAPLcMtD9y"
    "5gkZ6Eq3Rjuahrv17fEQ3Qen6J",
    "xpub661no6RGEX3uJkY4bNnPcw4URcQTrSibUZ4NqJEw5eBkv7ovTwgiT91XX27VbEXGENhYRCf7hyEbWrR3F"
    "ewATdCEebj6znwMfQkhRYHRLpJ",
    "xpub661MyMwAuDcm6CRQ5N4qiHKrJ39Xe1R1NyfouMKTTWcguwVcfrZJaNvhpebzGerh7gucBvzEQWRugZDuD"
    "XjNDRmXzSZe4c7mnTK97pTvGS8",
    "DMwo58pR1QLEFihHiXPVykYB6fJmsTeHvyTp7hRThAtCX8CvYzgPcn8XnmdfHGMQzT7ayAmfo4z3gY5KfbrZW"
    "Z6St24UVf2Qgo6oujFktLHdHY4",
    "xpub661MyMwAqRbcEYS8w7XLSVeEsBXy79zSzH1J8vCdxAZningWLdN3zgtU6Q5JXayek4PRsn35jii4veMim"
    "ro1xefsM58PgBMrvdYre8QyULY",
    "xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejM"
    "RNNU3TGtRBeJgk33yuGBxrMPHL",
};

static void test_invalid_keys(void)
{
	for (size_t i = 0; i < sizeof(INVALID) / sizeof(INVALID[0]); i++) {
		osk_bip32_pub_t pub;
		osk_result_t res = osk_bip32_parse_pub(INVALID[i], OSK_BIP32_XPUB_MAIN, &pub);

		if (res == OSK_OK)
			t_fail("ungueltiger Schluessel abgelehnt", __FILE__, __LINE__, INVALID[i]);
		T_TRUE("abgelehnt", res != OSK_OK);
	}
}

static void test_path_parse(void)
{
	uint32_t steps[OSK_BIP32_MAX_DEPTH];
	size_t count = 99;

	T_EQ("m allein", osk_bip32_path_parse("m", steps, OSK_BIP32_MAX_DEPTH, &count), OSK_OK);
	T_EQ("keine Schritte", count, 0);
	T_EQ("BIP44", osk_bip32_path_parse("m/44'/0'/0'/0/0", steps, OSK_BIP32_MAX_DEPTH, &count),
	     OSK_OK);
	T_EQ("fuenf Schritte", count, 5);
	T_EQ("44 gehaertet", (long long)steps[0], (long long)(44u | OSK_BIP32_HARDENED));
	T_EQ("letzter Schritt 0", (long long)steps[4], 0);
	T_EQ("h gilt wie '", osk_bip32_path_parse("m/44h/0h", steps, OSK_BIP32_MAX_DEPTH, &count),
	     OSK_OK);
	T_EQ("zwei Schritte", count, 2);
	T_EQ("ohne m geht auch", osk_bip32_path_parse("44'/0'", steps, OSK_BIP32_MAX_DEPTH, &count),
	     OSK_OK);
	T_EQ("auch zwei", count, 2);

	T_EQ("doppelter Schraegstrich",
	     osk_bip32_path_parse("m//0", steps, OSK_BIP32_MAX_DEPTH, &count), OSK_ERR_FORMAT);
	T_EQ("Schraegstrich am Ende",
	     osk_bip32_path_parse("m/0/", steps, OSK_BIP32_MAX_DEPTH, &count), OSK_ERR_FORMAT);
	T_EQ("Buchstabe mitten drin",
	     osk_bip32_path_parse("m/0x1", steps, OSK_BIP32_MAX_DEPTH, &count), OSK_ERR_FORMAT);
	T_EQ("minus gibt es nicht",
	     osk_bip32_path_parse("m/-1", steps, OSK_BIP32_MAX_DEPTH, &count), OSK_ERR_FORMAT);
	T_EQ("zu gross", osk_bip32_path_parse("m/2147483648", steps, OSK_BIP32_MAX_DEPTH, &count),
	     OSK_ERR_FORMAT);
	T_EQ("Zahl ueberlaeuft",
	     osk_bip32_path_parse("m/99999999999999", steps, OSK_BIP32_MAX_DEPTH, &count),
	     OSK_ERR_FORMAT);
	T_EQ("zu tief",
	     osk_bip32_path_parse("m/0/0/0/0/0/0/0/0/0", steps, OSK_BIP32_MAX_DEPTH, &count),
	     OSK_ERR_FULL);
	T_EQ("nur m/", osk_bip32_path_parse("m/", steps, OSK_BIP32_MAX_DEPTH, &count),
	     OSK_ERR_FORMAT);
	T_EQ("leer", osk_bip32_path_parse("", steps, OSK_BIP32_MAX_DEPTH, &count), OSK_ERR_FORMAT);
}

/* Der oeffentliche Weg muss denselben Knoten ergeben wie der private -
 * sonst zeigt ein xpub, den der Nutzer einer Geldboerse gibt, auf fremde
 * Adressen. */
static void test_public_derivation(void)
{
	uint8_t seed[16];
	osk_bip32_priv_t master, account, child;
	osk_bip32_pub_t pub_account, pub_child;

	memset(seed, 0x42, sizeof(seed));
	T_EQ("Wurzel", osk_bip32_master(seed, sizeof(seed), OSK_BIP32_SECP256K1, &master), OSK_OK);
	T_EQ("Konto", osk_bip32_derive_path(&master, "m/84'/0'/0'", &account), OSK_OK);
	T_EQ("Konto oeffentlich", osk_bip32_neuter(&account, &pub_account), OSK_OK);

	for (uint32_t index = 0; index < 3; index++) {
		osk_bip32_priv_t step;
		osk_bip32_pub_t from_priv;

		T_EQ("privat abgeleitet", osk_bip32_derive_child(&account, index, &step), OSK_OK);
		T_EQ("davon oeffentlich", osk_bip32_neuter(&step, &from_priv), OSK_OK);
		T_EQ("oeffentlich abgeleitet",
		     osk_bip32_pub_derive_child(&pub_account, index, &pub_child), OSK_OK);
		T_TRUE("gleicher Punkt",
		       memcmp(from_priv.pub, pub_child.pub, sizeof(pub_child.pub)) == 0);
		T_TRUE("gleiche Kette", memcmp(from_priv.chain, pub_child.chain, 32) == 0);
		osk_bip32_wipe(&step);
	}
	/* Gehaertet geht oeffentlich nicht - das ist der Sinn der Haertung. */
	T_EQ("gehaertet oeffentlich abgelehnt",
	     osk_bip32_pub_derive_child(&pub_account, OSK_BIP32_HARDENED, &pub_child),
	     OSK_ERR_UNSUPPORTED);
	osk_bip32_wipe(&child);
	osk_bip32_wipe(&account);
	osk_bip32_wipe(&master);
}

/* SLIP-0010 fuer ed25519: nur gehaertet, und der Schluessel ist der linke
 * Teil ohne jede Nachrechnung. Die Vektoren stammen aus SLIP-0010
 * ("Test vector 1 for ed25519", Seed 000102030405060708090a0b0c0d0e0f). */
static void test_ed25519(void)
{
	uint8_t seed[16];
	osk_bip32_priv_t master, step;

	T_EQ("Seed", t_unhex("000102030405060708090a0b0c0d0e0f", seed, sizeof(seed)), 16);
	T_EQ("Wurzel", osk_bip32_master(seed, sizeof(seed), OSK_BIP32_ED25519, &master), OSK_OK);
	T_HEX("Wurzelschluessel", master.key, 32,
	      "2b4be7f19ee27bbf30c667b642d5f4aa69fd169872f8fc3059c08ebae2eb19e7");
	T_HEX("Wurzelkette", master.chain, 32,
	      "90046a93de5380a72b5e45010748567d5ea02bbf6522f979e05c0d8d8ca9fffb");

	T_EQ("m/0'", osk_bip32_derive_child(&master, 0u | OSK_BIP32_HARDENED, &step), OSK_OK);
	T_HEX("m/0' Schluessel", step.key, 32,
	      "68e0fe46dfb67e368c75379acec591dad19df3cde26e63b93a8e704f1dade7a3");
	T_HEX("m/0' Kette", step.chain, 32,
	      "8b59aa11380b624e81507a27fedda59fea6d0b779a778918a2fd3590e16e9c69");

	T_EQ("nicht gehaertet abgelehnt", osk_bip32_derive_child(&master, 0, &step),
	     OSK_ERR_UNSUPPORTED);
	{
		osk_bip32_pub_t pub;

		T_EQ("xpub gibt es fuer ed25519 nicht hier", osk_bip32_neuter(&master, &pub),
		     OSK_ERR_UNSUPPORTED);
	}
	osk_bip32_wipe(&master);
}

int main(void)
{
	check_chain("000102030405060708090a0b0c0d0e0f", VECTOR1,
		    sizeof(VECTOR1) / sizeof(VECTOR1[0]));
	check_chain("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c99969390"
		    "8d8a8784817e7b7875726f6c696663605d5a5754514e4b484542",
		    VECTOR2, sizeof(VECTOR2) / sizeof(VECTOR2[0]));
	check_chain("4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319a"
		    "c14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be",
		    VECTOR3, sizeof(VECTOR3) / sizeof(VECTOR3[0]));
	check_chain("3ddd5602285899a946114506157c7997e5444528f3003f6134712147db19b678", VECTOR4,
		    sizeof(VECTOR4) / sizeof(VECTOR4[0]));
	test_invalid_keys();
	test_path_parse();
	test_public_derivation();
	test_ed25519();
	return t_report("bip32");
}
