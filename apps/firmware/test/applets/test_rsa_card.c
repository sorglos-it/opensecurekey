/* Die RSA-Kartenschicht fuer sich: Umrechnungen, Grenzen, Ablehnungen.
 *
 * Das Wichtigste hier ist NICHT, dass eine Unterschrift herauskommt - die
 * rechnet spaeter der Chip. Geprueft wird, dass die drei Angaben stimmen,
 * die der Vertrag absichtlich verlangt: die Auffuellung wird genannt, das
 * Hashverfahren wird nicht aus der Laenge geraten, und entschluesselt
 * werden kann auch.
 *
 * Alles laeuft am PC gegen Attrappen. Am Geraet ist nichts davon
 * gelaufen - Stufe 2 von vier (geschrieben / uebersetzt / im Abbild / am
 * Geraet gelaufen). */
#include "test_util.h"
#include "osk_rsa_card.h"
#include "osk_apdu.h"
#include "osk_card.h"
#include "mocks.h"

/* Die drei DigestInfo-Vorsaetze woertlich aus RFC 8017 Abschnitt 9.2,
 * Anmerkung 1. Sie stehen hier ZWEITES Mal - absichtlich: die Pruefung
 * soll gegen den RFC pruefen und nicht gegen die Tabelle im Code. Waere
 * sie dieselbe Tabelle, wuerde ein Tippfehler darin unentdeckt bleiben. */
#define DI_SHA256 "3031300d060960864801650304020105000420"
#define DI_SHA384 "3041300d060960864801650304020205000430"
#define DI_SHA512 "3051300d060960864801650304020305000440"

static size_t make_di(const char *prefix_hex, size_t hash_len, uint8_t *out, size_t cap)
{
	size_t n = t_unhex(prefix_hex, out, cap);

	if (n == 0u || n + hash_len > cap)
		return 0u;
	for (size_t i = 0; i < hash_len; i++)
		out[n + i] = (uint8_t)(0xa0u + i);
	return n + hash_len;
}

/* Baut einen vollstaendigen EMSA-PKCS1-v1_5-Block:
 * 00 01 FF..FF 00 || DigestInfo, insgesamt `mod_len` Byte. */
static size_t make_block(const char *prefix_hex, size_t hash_len, size_t mod_len, uint8_t *out,
			 size_t cap)
{
	uint8_t di[OSK_RSA_DI_MAX];
	const size_t di_len = make_di(prefix_hex, hash_len, di, sizeof(di));
	size_t pad;

	if (di_len == 0u || mod_len > cap || mod_len < di_len + 11u)
		return 0u;
	pad = mod_len - di_len - 3u;
	out[0] = 0x00u;
	out[1] = 0x01u;
	for (size_t i = 0; i < pad; i++)
		out[2u + i] = 0xffu;
	out[2u + pad] = 0x00u;
	memcpy(out + 3u + pad, di, di_len);
	return mod_len;
}

static void test_bits(void)
{
	osk_rsa_bits_t e;

	T_TRUE("2048 bekannt", osk_rsa_bits_enum(2048u, &e));
	T_EQ("2048 ist die erste", (int)e, (int)OSK_RSA_2048);
	T_TRUE("3072 bekannt", osk_rsa_bits_enum(3072u, &e));
	T_EQ("3072", (int)e, (int)OSK_RSA_3072);
	T_TRUE("4096 bekannt", osk_rsa_bits_enum(4096u, &e));
	T_EQ("4096", (int)e, (int)OSK_RSA_4096);

	/* Laengen, die es nicht gibt - 1024 ist die, die man am ehesten
	 * geschenkt bekommen moechte, und sie ist zu kurz. */
	T_TRUE("1024 nicht", !osk_rsa_bits_enum(1024u, &e));
	T_TRUE("0 nicht", !osk_rsa_bits_enum(0u, &e));
	T_TRUE("2047 nicht", !osk_rsa_bits_enum(2047u, &e));

	T_EQ("2048 sind 256 Byte", osk_rsa_mod_bytes(2048u), 256u);
	T_EQ("4096 sind 512 Byte", osk_rsa_mod_bytes(4096u), 512u);
	T_EQ("unbekannte Laenge: 0", osk_rsa_mod_bytes(1024u), 0u);
	T_TRUE("512 Byte ist die Obergrenze", osk_rsa_mod_bytes(4096u) == OSK_RSA_MOD_MAX);
}

/* Die Warnung von Strang A: der SE051A2 kann RSA ueberhaupt nicht und
 * antwortet auf derselben Adresse wie der C2. Meldet die Abfrage 0, muss
 * das gemeldet werden statt beim ersten Schluessel zu scheitern. */
static void test_availability(void)
{
	mock_reset_all();
	osk_rsa_forget();

	if (!OSK_RSA_BUILT) {
		/* Die Fassung ohne RSA. Hier ist RSA auch dann nicht da, wenn
		 * der Chip es koennte - die Option wirkt an genau einer
		 * Stelle, und das ist diese. */
		T_TRUE("ohne CONFIG_OSK_RSA kein RSA", !osk_rsa_available());
		T_TRUE("und keine Laenge", !osk_rsa_bits_ok(2048u));
		T_TRUE("auch nicht 4096", !osk_rsa_bits_ok(4096u));
		/* Die Umrechnungen gelten weiter: sie haengen nicht am Chip
		 * und nicht an der Option, nur an der Zahl. */
		T_EQ("2048 sind weiter 256 Byte", osk_rsa_mod_bytes(2048u), 256u);
		T_TRUE("und 2048 ist weiter eine bekannte Laenge",
		       osk_rsa_bits_enum(2048u, NULL));
		return;
	}

	T_TRUE("mit RSA-Chip: da", osk_rsa_available());
	T_TRUE("2048 geht", osk_rsa_bits_ok(2048u));
	T_TRUE("4096 geht", osk_rsa_bits_ok(4096u));
	T_TRUE("1024 geht nicht", !osk_rsa_bits_ok(1024u));

	/* Der falsche Baustein. */
	mock_se050_set_rsa_mask(0u);
	osk_rsa_forget();
	T_TRUE("SE051A2: kein RSA", !osk_rsa_available());
	T_TRUE("und keine Laenge", !osk_rsa_bits_ok(2048u));
	/* Zweite Frage, gleiche Antwort - und sie darf den Chip nicht
	 * nochmal fragen muessen, damit ein Kommando nicht an einem
	 * langsamen Bus haengt. */
	T_TRUE("gemerkt", !osk_rsa_available());

	/* Ein Chip, der nur 2048 kann. */
	mock_se050_set_rsa_mask(1u << OSK_RSA_2048);
	osk_rsa_forget();
	T_TRUE("RSA da", osk_rsa_available());
	T_TRUE("2048 ja", osk_rsa_bits_ok(2048u));
	T_TRUE("4096 nein", !osk_rsa_bits_ok(4096u));

	mock_reset_all();
	osk_rsa_forget();
}

static void test_digestinfo(void)
{
	uint8_t di[OSK_RSA_DI_MAX];
	size_t len;
	osk_hash_t alg;
	const uint8_t *hash = NULL;
	size_t hash_len = 0;

	len = make_di(DI_SHA256, 32u, di, sizeof(di));
	T_EQ("SHA-256-DigestInfo ist 51 Byte", len, 51u);
	T_TRUE("erkannt", osk_rsa_digestinfo_parse(di, len, &alg, &hash, &hash_len));
	T_EQ("Verfahren SHA-256", (int)alg, (int)OSK_HASH_SHA256);
	T_EQ("Hash 32 Byte", hash_len, 32u);
	T_TRUE("Hash zeigt hinter den Vorsatz", hash == di + 19u);

	len = make_di(DI_SHA384, 48u, di, sizeof(di));
	T_EQ("SHA-384-DigestInfo ist 67 Byte", len, 67u);
	T_TRUE("erkannt", osk_rsa_digestinfo_parse(di, len, &alg, &hash, &hash_len));
	T_EQ("Verfahren SHA-384", (int)alg, (int)OSK_HASH_SHA384);
	T_EQ("Hash 48 Byte", hash_len, 48u);

	len = make_di(DI_SHA512, 64u, di, sizeof(di));
	T_EQ("SHA-512-DigestInfo ist 83 Byte", len, 83u);
	T_TRUE("erkannt", osk_rsa_digestinfo_parse(di, len, &alg, &hash, &hash_len));
	T_EQ("Verfahren SHA-512", (int)alg, (int)OSK_HASH_SHA512);
	T_EQ("Laengstes DigestInfo passt in die Grenze", len, OSK_RSA_DI_MAX);

	/* Ein Byte zu lang ist keines. */
	len = make_di(DI_SHA256, 33u, di, sizeof(di));
	T_TRUE("ein Byte zu lang", !osk_rsa_digestinfo_parse(di, len, &alg, &hash, &hash_len));
	/* Ein Byte zu kurz auch nicht. */
	len = make_di(DI_SHA256, 31u, di, sizeof(di));
	T_TRUE("ein Byte zu kurz", !osk_rsa_digestinfo_parse(di, len, &alg, &hash, &hash_len));

	/* DER WICHTIGSTE FALL: richtige Laenge, falsche Kennung. SHA-512/256
	 * hat denselben 32-Byte-Hash wie SHA-256 und eine andere
	 * Objektkennung (letztes Byte des OID 06 statt 01). Wer das
	 * Verfahren aus der Laenge ableitet, unterschreibt hier das Falsche
	 * und merkt es nie. */
	len = make_di("3031300d060960864801650304020605000420", 32u, di, sizeof(di));
	T_EQ("SHA-512/256 ist genauso lang", len, 51u);
	T_TRUE("und wird NICHT fuer SHA-256 gehalten",
	       !osk_rsa_digestinfo_parse(di, len, &alg, &hash, &hash_len));

	/* SHA-1: 15 Byte Vorsatz, 20 Byte Hash. Der Chip koennte es, der
	 * Vertrag nennt es nicht (osk_hash_t hat kein SHA-1) - also wird es
	 * abgelehnt und nicht auf SHA-256 verbogen. */
	len = make_di("3021300906052b0e03021a05000414", 20u, di, sizeof(di));
	T_EQ("SHA-1-DigestInfo ist 35 Byte", len, 35u);
	T_TRUE("SHA-1 abgelehnt", !osk_rsa_digestinfo_parse(di, len, &alg, &hash, &hash_len));

	T_TRUE("leer", !osk_rsa_digestinfo_parse(di, 0u, &alg, &hash, &hash_len));
	T_TRUE("Nullzeiger", !osk_rsa_digestinfo_parse(NULL, 51u, &alg, &hash, &hash_len));
	/* Abgeschnitten mitten im Vorsatz. */
	len = make_di(DI_SHA256, 32u, di, sizeof(di));
	for (size_t cut = 1u; cut < 19u; cut++)
		T_TRUE("abgeschnitten",
		       !osk_rsa_digestinfo_parse(di, cut, &alg, &hash, &hash_len));
}

static void test_unwrap(void)
{
	static uint8_t blk[OSK_RSA_MOD_MAX];
	osk_hash_t alg;
	const uint8_t *hash = NULL;
	size_t hash_len = 0;
	size_t n;

	/* Der gewoehnliche Fall: 2048 Bit, SHA-256. */
	n = make_block(DI_SHA256, 32u, 256u, blk, sizeof(blk));
	T_EQ("Block ist so lang wie der Modulus", n, 256u);
	T_TRUE("ausgepackt", osk_rsa_pkcs1_unwrap(blk, n, 256u, &alg, &hash, &hash_len));
	T_EQ("SHA-256", (int)alg, (int)OSK_HASH_SHA256);
	T_EQ("32 Byte Hash", hash_len, 32u);

	/* Ohne das fuehrende Nullbyte - manche Kartenprogramme lassen es
	 * weg, weil es keine Information traegt. */
	T_TRUE("ohne fuehrende Null",
	       osk_rsa_pkcs1_unwrap(blk + 1u, n - 1u, 256u, &alg, &hash, &hash_len));

	/* 4096 Bit, SHA-512 - der groesste Fall, den es gibt. */
	n = make_block(DI_SHA512, 64u, 512u, blk, sizeof(blk));
	T_EQ("512 Byte", n, 512u);
	T_TRUE("ausgepackt", osk_rsa_pkcs1_unwrap(blk, n, 512u, &alg, &hash, &hash_len));
	T_EQ("SHA-512", (int)alg, (int)OSK_HASH_SHA512);

	/* Falsche Gesamtlaenge: der Block muss zum Modulus passen. */
	n = make_block(DI_SHA256, 32u, 256u, blk, sizeof(blk));
	T_TRUE("zu kurz fuer den Modulus",
	       !osk_rsa_pkcs1_unwrap(blk, n, 512u, &alg, &hash, &hash_len));
	T_TRUE("zu lang fuer den Modulus",
	       !osk_rsa_pkcs1_unwrap(blk, n, 128u, &alg, &hash, &hash_len));

	/* Fuehrendes Byte nicht 00. */
	blk[0] = 0x01u;
	T_TRUE("fuehrendes Byte falsch",
	       !osk_rsa_pkcs1_unwrap(blk, n, 256u, &alg, &hash, &hash_len));
	blk[0] = 0x00u;

	/* Blocktyp nicht 01. Typ 02 ist der fuer Verschluesselung - wer ihn
	 * hier annimmt, unterschreibt einen Block, der nie fuer eine
	 * Unterschrift gedacht war. */
	blk[1] = 0x02u;
	T_TRUE("Blocktyp 02 abgelehnt",
	       !osk_rsa_pkcs1_unwrap(blk, n, 256u, &alg, &hash, &hash_len));
	blk[1] = 0x01u;
	T_TRUE("wieder gut", osk_rsa_pkcs1_unwrap(blk, n, 256u, &alg, &hash, &hash_len));

	/* Trennbyte fehlt: nur 0xFF bis zum DigestInfo. */
	{
		const size_t sep = n - 51u - 1u;

		T_EQ("Trennbyte gefunden", blk[sep], 0x00u);
		blk[sep] = 0xffu;
		T_TRUE("ohne Trennbyte abgelehnt",
		       !osk_rsa_pkcs1_unwrap(blk, n, 256u, &alg, &hash, &hash_len));
		blk[sep] = 0x00u;
	}

	/* Zu wenig Auffuellung. RFC 8017 verlangt mindestens acht Byte; mit
	 * weniger ist Platz fuer ein zweites, kuerzeres DigestInfo im selben
	 * Block, und genau dort setzen Faelschungen an. Gebaut wird der Fall
	 * von Hand: 00 01 FF*7 00 || DigestInfo, davor Nullen aufgefuellt
	 * waere kein gueltiger Block - also wird der Block kurz gehalten und
	 * die Modulus-Laenge mitgezogen. */
	{
		uint8_t small[70];
		uint8_t di[OSK_RSA_DI_MAX];
		const size_t di_len = make_di(DI_SHA256, 32u, di, sizeof(di));
		size_t at = 0;

		small[at++] = 0x00u;
		small[at++] = 0x01u;
		for (unsigned i = 0; i < 7u; i++)
			small[at++] = 0xffu;
		small[at++] = 0x00u;
		memcpy(small + at, di, di_len);
		at += di_len;
		T_EQ("Block mit 7 Auffuellbytes", at, 61u);
		T_TRUE("sieben Auffuellbytes abgelehnt",
		       !osk_rsa_pkcs1_unwrap(small, at, at, &alg, &hash, &hash_len));
		/* Mit acht geht es - der Unterschied ist genau ein Byte. */
		small[2u + 7u] = 0xffu;
		memmove(small + 11u, di, di_len);
		small[10u] = 0x00u;
		T_TRUE("acht Auffuellbytes angenommen",
		       osk_rsa_pkcs1_unwrap(small, 11u + di_len, 11u + di_len, &alg, &hash,
					    &hash_len));
	}

	T_TRUE("Nullzeiger", !osk_rsa_pkcs1_unwrap(NULL, 256u, 256u, &alg, &hash, &hash_len));
	T_TRUE("Modulus 0", !osk_rsa_pkcs1_unwrap(blk, n, 0u, &alg, &hash, &hash_len));
	T_TRUE("Modulus zu gross",
	       !osk_rsa_pkcs1_unwrap(blk, n, OSK_RSA_MOD_MAX + 1u, &alg, &hash, &hash_len));
}

/* Der oeffentliche Teil: die Form, die PIV und OpenPGP beide verlangen,
 * und der Exponent in der Laenge, die der CHIP meldet. */
static void test_public(void)
{
	static uint8_t buf[1024];
	osk_resp_t r;
	const osk_key_id_t id = 0x50490099u;
	osk_key_policy_t policy;
	const uint8_t *inner = NULL;
	const uint8_t *mod = NULL;
	const uint8_t *exp = NULL;
	size_t inner_len = 0;
	size_t mod_len = 0;
	size_t exp_len = 0;

	mock_reset_all();
	osk_rsa_forget();

	policy.exportable = false;
	policy.needs_touch = true;

	if (!OSK_RSA_BUILT) {
		/* HIER IST DIE GRENZE DER OPTION, und sie ist genau so
		 * gewollt: osk_rsa_modulus_bits() und osk_rsa_put_public()
		 * fragen den CHIP und nicht die Option. Sie antworten
		 * deshalb auch in der Fassung ohne RSA.
		 *
		 * Der Riegel liegt eine Ebene hoeher - keine Anwendung
		 * kommt hier an, ohne vorher osk_rsa_available() gefragt zu
		 * haben; test_piv.c und test_openpgp.c pruefen das in beiden
		 * Fassungen. Waere der Riegel auch hier eingebaut, laege er
		 * an zwei Stellen, und eine davon wuerde irgendwann
		 * nachgeben.
		 *
		 * Diese Erwartung stand bis zum ersten Lauf umgekehrt da
		 * ("die Karte sieht ihn nicht") - der Lauf hat die Erwartung
		 * widerlegt, nicht den Code. */
		T_EQ("Schluessel entsteht im Chip",
		     osk_se050_rsa_key_create(id, OSK_RSA_2048, &policy), OSK_OK);
		T_TRUE("die Karte sagt: RSA ist nicht da", !osk_rsa_available());
		T_TRUE("und keine Laenge ist freigegeben", !osk_rsa_bits_ok(2048u));
		T_EQ("die Chipabfrage antwortet aber weiter",
		     osk_rsa_modulus_bits(id), 2048u);
		osk_resp_init(&r, buf, sizeof(buf));
		T_EQ("und der oeffentliche Teil kommt heraus",
		     osk_rsa_put_public(id, &r), OSK_SW_OK);
		T_TRUE("7F49 da", osk_tlv_find(buf, r.len, 0x7f49u, &inner, &inner_len));
		T_TRUE("Modulus", osk_tlv_find(inner, inner_len, 0x81u, &mod, &mod_len));
		T_TRUE("Exponent", osk_tlv_find(inner, inner_len, 0x82u, &exp, &exp_len));
		T_EQ("Exponent weiter drei Byte", exp_len, 3u);
		mock_reset_all();
		osk_rsa_forget();
		return;
	}

	osk_resp_init(&r, buf, sizeof(buf));
	T_EQ("ohne Schluessel: nicht gefunden", osk_rsa_put_public(id, &r), OSK_SW_REF_NOT_FOUND);
	T_EQ("und keine Bitlaenge", osk_rsa_modulus_bits(id), 0u);

	T_EQ("Schluessel anlegen", osk_se050_rsa_key_create(id, OSK_RSA_2048, &policy), OSK_OK);
	T_EQ("Bitlaenge kommt vom Chip", osk_rsa_modulus_bits(id), 2048u);

	osk_resp_init(&r, buf, sizeof(buf));
	T_EQ("oeffentlicher Teil", osk_rsa_put_public(id, &r), OSK_SW_OK);
	T_TRUE("kein Ueberlauf", !r.overflow);
	T_TRUE("7F49 da", osk_tlv_find(buf, r.len, 0x7f49u, &inner, &inner_len));
	T_TRUE("Modulus in 81", osk_tlv_find(inner, inner_len, 0x81u, &mod, &mod_len));
	T_TRUE("Exponent in 82", osk_tlv_find(inner, inner_len, 0x82u, &exp, &exp_len));
	T_EQ("Modulus 256 Byte", mod_len, 256u);

	/* DER BEFUND VON STRANG A, hier als Pruefung: der Exponent kommt
	 * DREI Byte lang heraus. Code, der vier annimmt, baut irgendwann ein
	 * falsches Zertifikat - und faellt hier auf. */
	T_EQ("Exponent ist drei Byte, nicht vier", exp_len, 3u);
	T_HEX("Exponent ist 65537", exp, exp_len, "010001");

	/* Ein Schluessel mit einer Laenge, die der Chip nicht kann. */
	mock_se050_set_rsa_mask(1u << OSK_RSA_2048);
	osk_rsa_forget();
	T_EQ("4096 lehnt der Chip ab",
	     osk_se050_rsa_key_create(0x50490098u, OSK_RSA_4096, &policy), OSK_ERR_UNSUPPORTED);

	mock_reset_all();
	osk_rsa_forget();
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	mock_reset_all();

	test_bits();
	test_availability();
	test_digestinfo();
	test_unwrap();
	test_public();
	return t_report("test_rsa_card");
}
