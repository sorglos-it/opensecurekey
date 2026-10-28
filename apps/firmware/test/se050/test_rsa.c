/* RSA gegen den Nachbau: abfragen, anlegen, lesen, signieren,
 * entschluesseln.
 *
 * Grenze wie in se050_sim.h: der Nachbau liest AN12413 so wie der
 * Treiber. Ein gruener Lauf heisst "die Schichten passen zueinander",
 * nicht "sie passen zum echten SE051". Am Geraet ist nichts davon
 * gelaufen. */
#include "tiny_test.h"
#include "fake_bus.h"
#include "se050_sim.h"
#include "osk_se050.h"
#include "se050_cmd.h"

/* Beide Hilfen liegen in test_contract.c - dort stehen auch die
 * Platinenhaken, die der Treiber braucht. */
int boot_sim(fake_bus_t *fb, se050_sim_t *sim);

/* Merkmalbild eines Bausteins, der RSA kann: die Vorgabe des Nachbaus
 * plus CONFIG_RSA_PLAIN und CONFIG_RSA_CRT. */
#define CFG_WITH_RSA (uint16_t)(0x100Fu | CONFIG_RSA_PLAIN | CONFIG_RSA_CRT)

#define RSA_KEY_ID 0x20000001u

/* Einen Baustein mit RSA anmelden. Das Merkmalbild muss VOR dem Anmelden
 * stehen: der Treiber liest es beim Auswaehlen des Applets, und danach
 * steht es fest - genau wie beim echten Teil, bei dem es im Werk
 * festgelegt wird. */
static int boot_rsa(fake_bus_t *fb, se050_sim_t *sim, uint16_t cfg)
{
	fb_init(fb);
	sim_attach(sim, fb);
	sim->applet_config = cfg;
	sim_objects_live(sim, true);
	return boot_sim(fb, sim) != 0;
}

void test_rsa(void)
{
	TT_CASE("RSA: ohne Anmeldung antwortet nichts");
	{
		osk_se050_close();

		uint32_t mask = 0;
		uint8_t buf[8];
		size_t n = 0;
		const osk_key_policy_t pol = {false, false};

		TT_EQ_INT(osk_se050_rsa_sizes(&mask), OSK_ERR_IO, "sizes");
		TT_EQ_INT(osk_se050_rsa_key_create(RSA_KEY_ID, OSK_RSA_2048, &pol), OSK_ERR_IO,
			  "key_create");
		TT_EQ_INT(osk_se050_rsa_public(RSA_KEY_ID, buf, sizeof(buf), &n, buf, sizeof(buf),
					       &n),
			  OSK_ERR_IO, "public");
		TT_EQ_INT(osk_se050_rsa_sign(RSA_KEY_ID, OSK_RSA_PAD_PKCS1_V15, OSK_HASH_SHA256,
					     buf, 32u, buf, sizeof(buf), &n),
			  OSK_ERR_IO, "sign");
		TT_EQ_INT(osk_se050_rsa_decrypt(RSA_KEY_ID, OSK_RSA_PAD_PKCS1_V15, OSK_HASH_SHA256,
						buf, 4u, buf, sizeof(buf), &n),
			  OSK_ERR_IO, "decrypt");
	}

	TT_CASE("RSA: der falsche Baustein meldet 0, nicht einen Fehler spaeter");
	{
		/* Das ist der Fall, um den es beim SE051A2 geht: er kann RSA
		 * ueberhaupt nicht und antwortet auf dieselbe Adresse wie der
		 * C2. Die Firmware muss das hier erfahren - nicht beim ersten
		 * Schluessel. */
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_rsa(&fb, &sim, 0x100Fu), "angemeldet, aber ohne RSA-Merkmal");

		uint32_t mask = 0xFFFFu;
		TT_EQ_INT(osk_se050_rsa_sizes(&mask), OSK_OK, "abgefragt");
		TT_EQ_INT(mask, 0u, "keine Laenge - falscher Baustein");

		/* Und dann wird auch nicht versucht, einen anzulegen. */
		const size_t before = sim.commands;
		const osk_key_policy_t pol = {false, false};
		TT_EQ_INT(osk_se050_rsa_key_create(RSA_KEY_ID, OSK_RSA_2048, &pol),
			  OSK_ERR_UNSUPPORTED, "anlegen lehnt ab");
		TT_EQ_INT(sim.commands, before, "und schickt dabei keinen Befehl");

		osk_se050_close();
	}

	TT_CASE("RSA: welche Laengen der Baustein kann");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_rsa(&fb, &sim, CFG_WITH_RSA), "angemeldet");

		uint32_t mask = 0;
		TT_EQ_INT(osk_se050_rsa_sizes(&mask), OSK_OK, "abgefragt");
		TT_EQ_INT(mask, (1u << OSK_RSA_2048) | (1u << OSK_RSA_3072) | (1u << OSK_RSA_4096),
			  "2048, 3072 und 4096");
		TT_EQ_INT(osk_se050_rsa_sizes(NULL), OSK_ERR_ARG, "NULL");

		/* Ein Baustein, der nur rohe Schluessel kann, kann RSA auch -
		 * das Merkmal ist ein anderes, das Ergebnis dasselbe. */
		osk_se050_close();
		TT_TRUE(boot_rsa(&fb, &sim, (uint16_t)(0x100Fu | CONFIG_RSA_PLAIN)),
			"nur rohe Schluessel");
		TT_EQ_INT(osk_se050_rsa_sizes(&mask), OSK_OK, "abgefragt");
		TT_TRUE(mask != 0u, "RSA ist da");

		osk_se050_close();
	}

	TT_CASE("RSA: Schluessel anlegen nennt Kennung, Laenge und Format");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_rsa(&fb, &sim, CFG_WITH_RSA), "angemeldet");

		const osk_key_policy_t pol = {false, true}; /* needs_touch, kein Export */
		TT_EQ_INT(osk_se050_rsa_key_create(RSA_KEY_ID, OSK_RSA_3072, &pol), OSK_OK,
			  "angelegt");

		TT_EQ_INT(sim.last_ins, INS_WRITE, "INS_WRITE");
		TT_EQ_INT(sim.last_p1, (uint8_t)(P1_KEY_PAIR | P1_RSA), "P1: Schluesselpaar, RSA");
		TT_EQ_INT(sim.last_p2, P2_DEFAULT, "P2_DEFAULT heisst CRT-Format");

		/* TAG_1 Kennung (4 Byte), TAG_2 Laenge in Bit (2 Byte). Die
		 * Laenge steht in BIT, nicht in Byte - zwei Groessen, die
		 * leicht verwechselt werden. 3072 Bit sind 0x0C00. */
		TT_EQ_INT(sim.last_payload_len, 6u + 4u, "zwei Felder");
		TT_EQ_INT(sim.last_payload[0], TAG_1, "TAG_1");
		TT_EQ_INT(sim.last_payload[6], TAG_2, "TAG_2");
		TT_EQ_INT(sim.last_payload[7], 2u, "zwei Byte Laenge");
		TT_EQ_INT((sim.last_payload[8] << 8) | sim.last_payload[9], 3072,
			  "3072 Bit, nicht 384 Byte");

		/* Kein privater Teil ist dabei: ohne Schluesselteile erzeugt
		 * der Baustein das Paar selbst (AN12413 4.7.1.2). */
		for (size_t i = 0; i < sim.last_payload_len; i++) {
			if (sim.last_payload[i] == TAG_3) {
				TT_TRUE(false, "kein TAG_3 - sonst kaeme der Schluessel von aussen");
			}
		}

		/* Exportierbar lehnt der Treiber ab, statt es still zu
		 * uebergehen. */
		const osk_key_policy_t exp_pol = {true, false};
		TT_EQ_INT(osk_se050_rsa_key_create(RSA_KEY_ID, OSK_RSA_2048, &exp_pol),
			  OSK_ERR_UNSUPPORTED, "exportierbar abgelehnt");
		TT_EQ_INT(osk_se050_rsa_key_create(RSA_KEY_ID, (osk_rsa_bits_t)99, &pol),
			  OSK_ERR_ARG, "unbekannte Laenge");

		osk_se050_close();
	}

	TT_CASE("RSA: ohne CRT-Merkmal wird roh angelegt");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_rsa(&fb, &sim, (uint16_t)(0x100Fu | CONFIG_RSA_PLAIN)),
			"nur rohe Schluessel");

		const osk_key_policy_t pol = {false, false};
		TT_EQ_INT(osk_se050_rsa_key_create(RSA_KEY_ID, OSK_RSA_2048, &pol), OSK_OK,
			  "angelegt");
		TT_EQ_INT(sim.last_p2, P2_RAW, "P2_RAW statt P2_DEFAULT");

		osk_se050_close();
	}

	TT_CASE("RSA: oeffentlicher Teil kommt in zwei Stuecken, beide mit Laenge");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_rsa(&fb, &sim, CFG_WITH_RSA), "angemeldet");

		const osk_key_policy_t pol = {false, false};
		TT_EQ_INT(osk_se050_rsa_key_create(RSA_KEY_ID, OSK_RSA_2048, &pol), OSK_OK,
			  "angelegt");

		uint8_t mod[512];
		uint8_t exp[8];
		size_t mod_len = 0;
		size_t exp_len = 0;
		TT_EQ_INT(osk_se050_rsa_public(RSA_KEY_ID, mod, sizeof(mod), &mod_len, exp,
					       sizeof(exp), &exp_len),
			  OSK_OK, "gelesen");
		TT_EQ_INT(mod_len, 256u, "Modulus 2048 Bit = 256 Byte");

		/* Der Punkt dieses Falles: der Exponent ist DREI Byte lang,
		 * nicht vier. Wer 65537 als vier Byte annimmt, baut
		 * irgendwann ein falsches Zertifikat - darum meldet der
		 * Vertrag beide Laengen, und darum prueft der Test sie. */
		TT_EQ_INT(exp_len, 3u, "Exponent drei Byte, nicht vier");
		TT_EQ_INT(exp[0], 0x01u, "0x010001");
		TT_EQ_INT(exp[1], 0x00u, "0x010001");
		TT_EQ_INT(exp[2], 0x01u, "0x010001");

		/* Der zweite Befehl muss den Exponenten verlangt haben -
		 * TAG_4 mit RSA_COMP_PUB_EXP (AN12413 Tabelle 34). */
		TT_EQ_INT(sim.last_payload[6], TAG_4, "TAG_4 nennt den Teil");
		TT_EQ_INT(sim.last_payload[8], RSA_COMP_PUB_EXP, "RSA_COMP_PUB_EXP");

		/* Ein zu kleiner Puffer wird gemeldet, nicht abgeschnitten. */
		uint8_t tiny[4];
		TT_EQ_INT(osk_se050_rsa_public(RSA_KEY_ID, tiny, sizeof(tiny), &mod_len, exp,
					       sizeof(exp), &exp_len),
			  OSK_ERR_FULL, "Modulus passt nicht");
		TT_EQ_INT(osk_se050_rsa_public(RSA_KEY_ID, mod, sizeof(mod), &mod_len, NULL, 0,
					       &exp_len),
			  OSK_ERR_ARG, "kein Exponentenpuffer");

		osk_se050_close();
	}

	TT_CASE("RSA: signieren nennt die Auffuellung, es wird nicht geraten");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_rsa(&fb, &sim, CFG_WITH_RSA), "angemeldet");

		const osk_key_policy_t pol = {false, false};
		TT_EQ_INT(osk_se050_rsa_key_create(RSA_KEY_ID, OSK_RSA_2048, &pol), OSK_OK,
			  "angelegt");

		uint8_t hash[64];
		for (size_t i = 0; i < sizeof(hash); i++) {
			hash[i] = (uint8_t)i;
		}
		uint8_t sig[512];
		size_t sig_len = 0;

		TT_EQ_INT(osk_se050_rsa_sign(RSA_KEY_ID, OSK_RSA_PAD_PKCS1_V15, OSK_HASH_SHA256,
					     hash, 32u, sig, sizeof(sig), &sig_len),
			  OSK_OK, "PKCS#1 v1.5 mit SHA-256");
		TT_EQ_INT(sim.last_ins, INS_CRYPTO, "INS_CRYPTO");
		TT_EQ_INT(sim.last_p1, P1_SIGNATURE, "P1_SIGNATURE");
		TT_EQ_INT(sim.last_p2, P2_SIGN, "P2_SIGN");
		TT_EQ_INT(sim.last_rsa_algo, RSA_SHA_256_PKCS1, "Verfahren 0x28");
		TT_EQ_INT(sig_len, 256u, "Signatur so lang wie der Modulus");

		TT_EQ_INT(osk_se050_rsa_sign(RSA_KEY_ID, OSK_RSA_PAD_PSS, OSK_HASH_SHA384, hash,
					     48u, sig, sizeof(sig), &sig_len),
			  OSK_OK, "PSS mit SHA-384");
		TT_EQ_INT(sim.last_rsa_algo, RSA_SHA384_PKCS1_PSS, "Verfahren 0x2D");

		TT_EQ_INT(osk_se050_rsa_sign(RSA_KEY_ID, OSK_RSA_PAD_PSS, OSK_HASH_SHA512, hash,
					     64u, sig, sizeof(sig), &sig_len),
			  OSK_OK, "PSS mit SHA-512");
		TT_EQ_INT(sim.last_rsa_algo, RSA_SHA512_PKCS1_PSS, "Verfahren 0x2E");

		/* Der wichtigste Fall dieses Prueffeldes: die genannte
		 * Hashart und die uebergebene Laenge muessen zusammenpassen.
		 * Aus der Laenge allein ist die Art nicht ableitbar -
		 * SHA-512/256 und SHA-256 sind beide 32 Byte lang. Passt es
		 * nicht, baut der Baustein ein DigestInfo mit der falschen
		 * Kennung, und die Signatur ist gueltig geformt und trotzdem
		 * falsch. */
		const size_t before = sim.commands;
		TT_EQ_INT(osk_se050_rsa_sign(RSA_KEY_ID, OSK_RSA_PAD_PKCS1_V15, OSK_HASH_SHA256,
					     hash, 48u, sig, sizeof(sig), &sig_len),
			  OSK_ERR_ARG, "SHA-256 genannt, 48 Byte uebergeben");
		TT_EQ_INT(sim.commands, before, "und nichts geschickt");

		/* Auffuellungen, die zum Signieren nicht gehoeren. */
		TT_EQ_INT(osk_se050_rsa_sign(RSA_KEY_ID, OSK_RSA_PAD_OAEP, OSK_HASH_SHA256, hash,
					     32u, sig, sizeof(sig), &sig_len),
			  OSK_ERR_ARG, "OAEP ist zum Entschluesseln");
		/* Rohes Signieren kennt AN12413 Tabelle 31 nicht - ehrlich
		 * abgelehnt, statt es selbst nachzubauen. */
		TT_EQ_INT(osk_se050_rsa_sign(RSA_KEY_ID, OSK_RSA_PAD_NONE, OSK_HASH_SHA256, hash,
					     32u, sig, sizeof(sig), &sig_len),
			  OSK_ERR_UNSUPPORTED, "ohne Auffuellung kann der Baustein nicht");
		TT_EQ_INT(osk_se050_rsa_sign(RSA_KEY_ID, OSK_RSA_PAD_PKCS1_V15, OSK_HASH_SHA256,
					     NULL, 32u, sig, sizeof(sig), &sig_len),
			  OSK_ERR_ARG, "kein Hash");

		osk_se050_close();
	}

	TT_CASE("RSA: entschluesseln - dafuer ist PIV 9D und OpenPGP da");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_rsa(&fb, &sim, CFG_WITH_RSA), "angemeldet");

		const osk_key_policy_t pol = {false, false};
		TT_EQ_INT(osk_se050_rsa_key_create(RSA_KEY_ID, OSK_RSA_2048, &pol), OSK_OK,
			  "angelegt");

		/* Ein Chiffrat ist so lang wie der Modulus: 256 Byte bei
		 * 2048 Bit. Damit ist der Befehl ueber 255 Byte lang und
		 * braucht die erweiterte Laenge - ohne sie bliebe
		 * verschluesselte Post ungelesen. */
		uint8_t ct[256];
		for (size_t i = 0; i < sizeof(ct); i++) {
			ct[i] = (uint8_t)(0xA0u + (i & 0x0Fu));
		}
		uint8_t plain[64];
		size_t plain_len = 0;

		TT_EQ_INT(osk_se050_rsa_decrypt(RSA_KEY_ID, OSK_RSA_PAD_PKCS1_V15, OSK_HASH_SHA256,
						ct, sizeof(ct), plain, sizeof(plain), &plain_len),
			  OSK_OK, "entschluesselt");
		TT_EQ_INT(sim.last_ins, INS_CRYPTO, "INS_CRYPTO");
		TT_EQ_INT(sim.last_p1, P1_RSA, "P1_RSA");
		TT_EQ_INT(sim.last_p2, P2_DECRYPT_ONESHOT, "P2_DECRYPT_ONESHOT");
		TT_EQ_INT(sim.last_rsa_algo, RSA_PKCS1, "RSAES-PKCS1-v1_5");
		TT_EQ_INT(plain_len, 15u, "Klartext kam an");
		TT_EQ_MEM(plain, "Geheime Post!!", 14u, "und ist der richtige");

		/* Das ganze Chiffrat muss angekommen sein, nicht nur die
		 * ersten 255 Byte: der Nachbau lehnt eine andere Laenge ab. */
		TT_TRUE(sim.last_payload_len > 255u, "Befehl ueber 255 Byte lang");

		/* OAEP: BEFUND, nicht Faulheit. AN12413 Tabelle 32 kennt OAEP
		 * nur mit SHA-1, und osk_hash_t kennt kein SHA-1. Also gibt
		 * es kein hash_alg, mit dem der Aufruf richtig waere. */
		TT_EQ_INT(osk_se050_rsa_decrypt(RSA_KEY_ID, OSK_RSA_PAD_OAEP, OSK_HASH_SHA256, ct,
						sizeof(ct), plain, sizeof(plain), &plain_len),
			  OSK_ERR_UNSUPPORTED, "OAEP nur mit SHA-1 - abgelehnt");
		TT_EQ_INT(osk_se050_rsa_decrypt(RSA_KEY_ID, OSK_RSA_PAD_PSS, OSK_HASH_SHA256, ct,
						sizeof(ct), plain, sizeof(plain), &plain_len),
			  OSK_ERR_ARG, "PSS ist zum Signieren");

		/* Rohes RSA gibt es beim Entschluesseln (Tabelle 32,
		 * RSA_NO_PAD) - anders als beim Signieren. */
		TT_EQ_INT(osk_se050_rsa_decrypt(RSA_KEY_ID, OSK_RSA_PAD_NONE, OSK_HASH_SHA256, ct,
						sizeof(ct), plain, sizeof(plain), &plain_len),
			  OSK_OK, "ohne Auffuellung");
		TT_EQ_INT(sim.last_rsa_algo, RSA_NO_PAD, "RSA_NO_PAD");

		osk_se050_close();
	}

	TT_CASE("RSA: 4096 Bit - der laengste Befehl und die laengste Antwort");
	{
		fake_bus_t fb;
		se050_sim_t sim;
		TT_TRUE(boot_rsa(&fb, &sim, CFG_WITH_RSA), "angemeldet");

		const osk_key_policy_t pol = {false, false};
		TT_EQ_INT(osk_se050_rsa_key_create(RSA_KEY_ID, OSK_RSA_4096, &pol), OSK_OK,
			  "angelegt");

		uint8_t mod[512];
		uint8_t exp[8];
		size_t mod_len = 0;
		size_t exp_len = 0;
		TT_EQ_INT(osk_se050_rsa_public(RSA_KEY_ID, mod, sizeof(mod), &mod_len, exp,
					       sizeof(exp), &exp_len),
			  OSK_OK, "oeffentlicher Teil");
		TT_EQ_INT(mod_len, 512u, "Modulus 512 Byte");

		/* 512 Byte hinein: der laengste Befehl, den dieser Treiber
		 * ueberhaupt schickt. */
		uint8_t ct[512];
		for (size_t i = 0; i < sizeof(ct); i++) {
			ct[i] = (uint8_t)(i & 0xFFu);
		}
		uint8_t plain[64];
		size_t plain_len = 0;
		TT_EQ_INT(osk_se050_rsa_decrypt(RSA_KEY_ID, OSK_RSA_PAD_PKCS1_V15, OSK_HASH_SHA256,
						ct, sizeof(ct), plain, sizeof(plain), &plain_len),
			  OSK_OK, "entschluesselt");
		TT_EQ_INT(plain_len, 15u, "Klartext kam an");
		TT_TRUE(fb_clean(&fb), "Testbus ohne Beanstandung");

		/* Laenger als der laengste Modulus kann kein Chiffrat sein. */
		uint8_t too_long[600];
		memset(too_long, 0x5Au, sizeof(too_long));
		TT_EQ_INT(osk_se050_rsa_decrypt(RSA_KEY_ID, OSK_RSA_PAD_PKCS1_V15, OSK_HASH_SHA256,
						too_long, sizeof(too_long), plain, sizeof(plain),
						&plain_len),
			  OSK_ERR_ARG, "600 Byte abgelehnt");

		osk_se050_close();
	}
}
