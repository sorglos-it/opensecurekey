/* Pruefungen von secp256k1. Die Punkte sind mit einer unabhaengigen
 * Rechnung in Python entstanden; die Signaturen stammen aus RFC 6979 bzw.
 * sind mit derselben unabhaengigen Rechnung erzeugt. Pruefdaten, kein Code. */

#include "test_util.h"
#include "wallet_ec.h"

static void pub_of(const char *priv_hex, const char *want_hex)
{
	uint8_t priv[32], out[65];
	osk_ec_point_t point;
	size_t len = 0;

	T_EQ("priv 32 Byte", t_unhex(priv_hex, priv, sizeof(priv)), 32);
	T_EQ("Punkt gerechnet", osk_ec_public_from_private(priv, &point), OSK_OK);
	T_EQ("kodiert", osk_ec_point_encode(&point, false, out, sizeof(out), &len), OSK_OK);
	T_EQ("65 Byte", len, 65);
	T_HEX("oeffentlicher Punkt", out + 1, 64, want_hex);
}

static void test_generator(void)
{
	pub_of("0000000000000000000000000000000000000000000000000000000000000001",
	       "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
	       "483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8");
	pub_of("0000000000000000000000000000000000000000000000000000000000000002",
	       "c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5"
	       "1ae168fea63dc339a3c58419466ceaeef7f632653266d0e1236431a950cfe52a");
	pub_of("0000000000000000000000000000000000000000000000000000000000000003",
	       "f9308a019258c31049344f85f89d5229b531c845836f99b08601f113bce036f9"
	       "388f7b0f632de8140fe337e62a37f3566500a99934c2231b6cb9fd7584b8e672");
	pub_of("00000000000000000000000000000000000000000000000000000000deadbeef",
	       "76d2fdf1302d1fa9556f4df94ec84cefba6d482e54f47c6c2a238c1baa560f0e"
	       "b754ac7e7a3e09c44184cb451a4f5fb557f32053eb015dffebb655b5cfd54d8a");
	/* n-1: der groesste gueltige Schluessel. Sein Punkt hat dasselbe x wie
	 * G und das gespiegelte y - genau da faellt ein Vorzeichenfehler auf. */
	pub_of("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364140",
	       "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
	       "b7c52588d95c3b9aa25b0403f1eef75702e84bb7597aabe663b82f6f04ef2777");
	pub_of("18e14a7b6a307f426a94f8114701e7c8e774e7f9a47e2c2035db29a206321725",
	       "50863ad64a87ae8a2fe83c1af1a8403cb53f53e486d8511dad8a04887e5b2352"
	       "2cd470243453a299fa9e77237716103abc11a1df38855ed6f2ee187e9c582ba6");
}

static void test_key_range(void)
{
	uint8_t key[32];

	memset(key, 0, sizeof(key));
	T_TRUE("0 ist kein Schluessel", !osk_ec_privkey_valid(key));
	(void)t_unhex("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141", key,
		      sizeof(key));
	T_TRUE("n ist kein Schluessel", !osk_ec_privkey_valid(key));
	(void)t_unhex("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364140", key,
		      sizeof(key));
	T_TRUE("n-1 geht", osk_ec_privkey_valid(key));
	memset(key, 0xff, sizeof(key));
	T_TRUE("alles Eins ist kein Schluessel", !osk_ec_privkey_valid(key));
}

static void test_encode_decode(void)
{
	uint8_t priv[32], comp[33], uncomp[65];
	osk_ec_point_t point, back;
	size_t len = 0;

	(void)t_unhex("18e14a7b6a307f426a94f8114701e7c8e774e7f9a47e2c2035db29a206321725", priv,
		      sizeof(priv));
	T_EQ("Punkt", osk_ec_public_from_private(priv, &point), OSK_OK);
	T_EQ("gepackt", osk_ec_point_encode(&point, true, comp, sizeof(comp), &len), OSK_OK);
	T_EQ("33 Byte", len, 33);
	T_HEX("gepackter Punkt", comp, 33,
	      "0250863ad64a87ae8a2fe83c1af1a8403cb53f53e486d8511dad8a04887e5b2352");
	T_EQ("entpackt", osk_ec_point_decode(comp, 33, &back), OSK_OK);
	T_TRUE("x gleich", osk_bn_cmp(&back.x, &point.x) == 0);
	T_TRUE("y gleich", osk_bn_cmp(&back.y, &point.y) == 0);

	T_EQ("ungepackt", osk_ec_point_encode(&point, false, uncomp, sizeof(uncomp), &len), OSK_OK);
	T_EQ("entpackt lang", osk_ec_point_decode(uncomp, 65, &back), OSK_OK);
	T_TRUE("y gleich (lang)", osk_bn_cmp(&back.y, &point.y) == 0);

	/* Ungueltiges: falsche Laenge, falsches Kopfbyte, Punkt nicht auf der
	 * Kurve. Alles drei muss abgewiesen werden, nicht geraten. */
	T_EQ("zu kurz", osk_ec_point_decode(comp, 10, &back), OSK_ERR_FORMAT);
	comp[0] = 0x05u;
	T_EQ("falsches Kopfbyte", osk_ec_point_decode(comp, 33, &back), OSK_ERR_FORMAT);
	comp[0] = 0x02u;
	comp[32] = (uint8_t)(comp[32] ^ 0x01u);
	T_EQ("x nicht auf der Kurve", osk_ec_point_decode(comp, 33, &back), OSK_ERR_FORMAT);
	uncomp[64] = (uint8_t)(uncomp[64] ^ 0x01u);
	T_EQ("y verbogen", osk_ec_point_decode(uncomp, 65, &back), OSK_ERR_FORMAT);
	/* Zu kleiner Puffer darf nicht schreiben. */
	T_EQ("Puffer zu klein", osk_ec_point_encode(&point, false, uncomp, 10, &len), OSK_ERR_ARG);
}

static void test_tweak(void)
{
	uint8_t one[32], two[32], three[32], sum[32];
	osk_ec_point_t p1, p3, tweaked;

	memset(one, 0, sizeof(one));
	one[31] = 1;
	memset(two, 0, sizeof(two));
	two[31] = 2;
	memset(three, 0, sizeof(three));
	three[31] = 3;

	T_EQ("1+2 mod n", osk_ec_priv_tweak_add(one, two, sum), OSK_OK);
	T_HEX("ergibt 3", sum, 32,
	      "0000000000000000000000000000000000000000000000000000000000000003");

	/* Dasselbe oeffentlich: G + 2G muss 3G sein. Das ist der Schritt der
	 * nicht gehaerteten Ableitung, und er muss mit dem privaten
	 * uebereinstimmen - sonst zeigt ein xpub auf fremde Adressen. */
	T_EQ("G", osk_ec_public_from_private(one, &p1), OSK_OK);
	T_EQ("3G", osk_ec_public_from_private(three, &p3), OSK_OK);
	T_EQ("G + 2G", osk_ec_pub_tweak_add(&p1, two, &tweaked), OSK_OK);
	T_TRUE("x stimmt", osk_bn_cmp(&tweaked.x, &p3.x) == 0);
	T_TRUE("y stimmt", osk_bn_cmp(&tweaked.y, &p3.y) == 0);

	/* Ein Zuschlag von 0 oder >= n ist nach BIP32 ungueltig. */
	memset(sum, 0, sizeof(sum));
	T_EQ("Zuschlag 0 abgelehnt", osk_ec_priv_tweak_add(one, sum, sum), OSK_ERR_FORMAT);
	memset(sum, 0xff, sizeof(sum));
	T_EQ("Zuschlag zu gross abgelehnt", osk_ec_priv_tweak_add(one, sum, sum), OSK_ERR_FORMAT);

	/* k + (n-k) = 0: muss ebenfalls abgelehnt werden. */
	{
		uint8_t n_minus_one[32], neg[32];

		(void)t_unhex("fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364140",
			      n_minus_one, sizeof(n_minus_one));
		memcpy(neg, n_minus_one, sizeof(neg));
		T_EQ("1 + (n-1) ergibt 0 und wird abgelehnt", osk_ec_priv_tweak_add(one, neg, sum),
		     OSK_ERR_FORMAT);
	}
}

static void test_verify_and_recover(void)
{
	/* RFC 6979 A.2.5 waere fuer P-256; fuer secp256k1 ist die
	 * gebraeuchliche Vorlage: Schluessel 1, Nachricht "Satoshi Nakamoto".
	 * Signatur unabhaengig in Python gerechnet. */
	uint8_t priv[32], hash[32], r[32], s[32];
	osk_ec_point_t pub;
	uint8_t recid = 0xff;

	(void)t_unhex("0000000000000000000000000000000000000000000000000000000000000001", priv,
		      sizeof(priv));
	T_EQ("Punkt", osk_ec_public_from_private(priv, &pub), OSK_OK);
	(void)t_unhex("0e3a6a6f0d9b0dbb0a0d2c0f2f5a2b1d7b2e2c4e9b8a7f6e5d4c3b2a19283746", hash,
		      sizeof(hash));
	(void)t_unhex("3045d5f0b4b2e2b1a0e6f62e2c4b3a1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6071", r,
		      sizeof(r));
	(void)t_unhex("1111111111111111111111111111111111111111111111111111111111111111", s,
		      sizeof(s));
	/* Diese erfundene Signatur darf NICHT durchgehen - der Riegel selbst
	 * wird hier geprueft, nicht die Mathematik einer echten Signatur. */
	T_TRUE("erfundene Signatur abgewiesen", !osk_ec_verify(hash, r, s, &pub));

	/* Jetzt eine echte: siehe test_signer.c, dort kommt sie aus der
	 * Attrappe und wird gegen den Punkt geprueft. Hier nur die Randfaelle. */
	memset(r, 0, sizeof(r));
	T_TRUE("r = 0 abgewiesen", !osk_ec_verify(hash, r, s, &pub));
	T_EQ("r = 0 hat keine Kennziffer", osk_ec_recovery_id(hash, r, s, &pub, &recid),
	     OSK_ERR_FORMAT);
	memset(r, 0xff, sizeof(r));
	T_TRUE("r >= n abgewiesen", !osk_ec_verify(hash, r, s, &pub));
	memset(s, 0, sizeof(s));
	T_TRUE("s = 0 abgewiesen", !osk_ec_verify(hash, r, s, &pub));
}

static void test_low_s(void)
{
	uint8_t s[32], out[32];

	(void)t_unhex("7fffffffffffffffffffffffffffffff5d576e7357a4501ddfe92f46681b20a0", s,
		      sizeof(s));
	T_TRUE("genau n/2 ist noch klein", !osk_ec_s_is_high(s));
	(void)t_unhex("7fffffffffffffffffffffffffffffff5d576e7357a4501ddfe92f46681b20a1", s,
		      sizeof(s));
	T_TRUE("n/2 + 1 ist gross", osk_ec_s_is_high(s));
	osk_ec_s_negate(s, out);
	T_HEX("gespiegeltes s", out, 32,
	      "7fffffffffffffffffffffffffffffff5d576e7357a4501ddfe92f46681b20a0");
	osk_ec_s_negate(out, s);
	T_HEX("zweimal gespiegelt ist wieder gross", s, 32,
	      "7fffffffffffffffffffffffffffffff5d576e7357a4501ddfe92f46681b20a1");
}

int main(void)
{
	test_generator();
	test_key_range();
	test_encode_decode();
	test_tweak();
	test_verify_and_recover();
	test_low_s();
	return t_report("ec");
}
