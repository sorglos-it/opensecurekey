/* Die APDU-Schicht: Pfad lesen, oeffentlichen Schluessel holen,
 * Transaktion in Stuecken unterschreiben, und vor allem: was bei
 * kaputten Befehlen passiert. */

#include "test_util.h"
#include "wallet_apdu.h"
#include "wallet_eth.h"
#include "wallet_seed.h"
#include "mocks.h"

static const char *const MNEMONIC =
    "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon"
    " about";
static const char *const LEGACY_RLP =
    "ec098504a817c800825208943535353535353535353535353535353535353535880de0b6b3a7640000"
    "80018080";

static osk_apdu_state_t state;

static void prepare(void)
{
	osk_item_id_t id = 0;

	mock_store_reset();
	mock_se050_reset();
	mock_ui_reset();
	mock_ui_answer(OSK_OK);
	osk_apdu_init(&state);
	T_EQ("Seed in den Tresor", osk_wallet_seed_store(0, "APDU", MNEMONIC, NULL, &id), OSK_OK);
	state.seed_id = id;
}

static void test_path(void)
{
	uint8_t raw[64];
	char path[128];
	size_t used = 0, len;

	/* m/44'/60'/0'/0/0 */
	len = t_unhex("058000002c8000003c800000000000000000000000", raw, sizeof(raw));
	T_EQ("Pfad gelesen", osk_apdu_read_path(raw, len, path, sizeof(path), &used), OSK_OK);
	T_EQ("21 Byte verbraucht", used, 21);
	T_TRUE("Text stimmt", strcmp(path, "m/44'/60'/0'/0/0") == 0);
	if (strcmp(path, "m/44'/60'/0'/0/0") != 0)
		printf("  bekam %s\n", path);

	len = t_unhex("00", raw, sizeof(raw));
	T_EQ("leerer Pfad", osk_apdu_read_path(raw, len, path, sizeof(path), &used), OSK_OK);
	T_TRUE("nur m", strcmp(path, "m") == 0);

	/* Zu tief und abgeschnitten. */
	len = t_unhex("0a", raw, sizeof(raw));
	T_EQ("zu tief", osk_apdu_read_path(raw, len, path, sizeof(path), &used), OSK_ERR_FULL);
	len = t_unhex("058000002c", raw, sizeof(raw));
	T_EQ("abgeschnitten", osk_apdu_read_path(raw, len, path, sizeof(path), &used),
	     OSK_ERR_FORMAT);
	T_EQ("leer", osk_apdu_read_path(raw, 0, path, sizeof(path), &used), OSK_ERR_FORMAT);
}

static void test_header(void)
{
	uint8_t out[256];
	size_t out_len = 0;
	uint16_t sw = 0;
	uint8_t apdu[8];

	prepare();
	/* Fremde Klasse. */
	apdu[0] = 0x00u;
	apdu[1] = 0x02u;
	apdu[2] = 0;
	apdu[3] = 0;
	T_EQ("bearbeitet", osk_apdu_handle(&state, apdu, 4, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("Klasse unbekannt", sw, OSK_SW_CLA_UNKNOWN);

	/* Unbekannter Befehl. */
	apdu[0] = OSK_APDU_CLA;
	apdu[1] = 0x77u;
	T_EQ("bearbeitet", osk_apdu_handle(&state, apdu, 4, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("Befehl unbekannt", sw, OSK_SW_INS_UNKNOWN);

	/* Zu kurz. */
	T_EQ("bearbeitet", osk_apdu_handle(&state, apdu, 2, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("Laenge falsch", sw, OSK_SW_WRONG_LENGTH);

	/* Laengenangabe passt nicht zur APDU. */
	apdu[1] = OSK_APDU_INS_GET_PUBKEY;
	apdu[4] = 0x20u;
	T_EQ("bearbeitet", osk_apdu_handle(&state, apdu, 6, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("Laenge falsch", sw, OSK_SW_WRONG_LENGTH);

	/* Fassung abfragen. */
	apdu[1] = OSK_APDU_INS_GET_CONFIG;
	T_EQ("bearbeitet", osk_apdu_handle(&state, apdu, 4, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("ok", sw, OSK_SW_OK);
	T_EQ("vier Byte", out_len, 4);
	T_EQ("blind ist aus", out[0], 0);
}

static void test_pubkey(void)
{
	uint8_t apdu[32], out[256];
	size_t out_len = 0;
	uint16_t sw = 0;

	prepare();
	apdu[0] = OSK_APDU_CLA;
	apdu[1] = OSK_APDU_INS_GET_PUBKEY;
	apdu[2] = 0x00u;
	apdu[3] = 0x00u;
	apdu[4] = 21u;
	T_EQ("Pfad gelesen",
	     t_unhex("058000002c8000003c800000000000000000000000", apdu + 5, sizeof(apdu) - 5), 21);
	T_EQ("bearbeitet", osk_apdu_handle(&state, apdu, 26, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("ok", sw, OSK_SW_OK);
	T_EQ("Laenge", out_len, 1u + 65u + 1u + 40u);
	T_EQ("Punkt ist 65 Byte", out[0], 65);
	T_EQ("ungepackt", out[1], 0x04u);
	T_EQ("Adresse 40 Zeichen", out[66], 40);
	/* Die bekannte Adresse zu diesem Seed, ohne "0x". */
	T_TRUE("Adresse stimmt",
	       memcmp(out + 67, "9858EfFD232B4033E47d90003D41EC34EcaEda94", 40) == 0);
	if (memcmp(out + 67, "9858EfFD232B4033E47d90003D41EC34EcaEda94", 40) != 0)
		printf("  bekam %.40s\n", (const char *)(out + 67));

	/* Mit Kettenzahl sind es 32 Byte mehr. */
	apdu[3] = 0x01u;
	T_EQ("bearbeitet", osk_apdu_handle(&state, apdu, 26, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("ok", sw, OSK_SW_OK);
	T_EQ("mit Kettenzahl", out_len, 1u + 65u + 1u + 40u + 32u);

	/* Zu kleiner Antwortpuffer. */
	apdu[3] = 0x00u;
	T_EQ("bearbeitet", osk_apdu_handle(&state, apdu, 26, out, 20, &out_len, &sw), OSK_OK);
	T_EQ("Laenge falsch", sw, OSK_SW_WRONG_LENGTH);
}

static void test_sign_tx(void)
{
	uint8_t apdu[128], out[256], rlp[128];
	size_t out_len = 0, rlp_len;
	uint16_t sw = 0;

	prepare();
	rlp_len = t_unhex(LEGACY_RLP, rlp, sizeof(rlp));

	/* In einem Stueck: Pfad und ganze Transaktion. */
	apdu[0] = OSK_APDU_CLA;
	apdu[1] = OSK_APDU_INS_SIGN_TX;
	apdu[2] = 0x00u;
	apdu[3] = 0x00u;
	apdu[4] = (uint8_t)(21u + rlp_len);
	T_EQ("Pfad", t_unhex("058000002c8000003c800000000000000000000000", apdu + 5, 21), 21);
	memcpy(apdu + 26, rlp, rlp_len);
	T_EQ("bearbeitet",
	     osk_apdu_handle(&state, apdu, 5u + 21u + rlp_len, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("ok", sw, OSK_SW_OK);
	T_EQ("65 Byte Antwort", out_len, 65);
	T_TRUE("v ist 37 oder 38", out[0] == 37u || out[0] == 38u);
	T_EQ("einmal gefragt", mock_ui_calls(), 1);

	/* Jetzt in zwei Stuecken: erst Pfad und der halbe Rumpf mit
	 * P2 = 0x01 ("es kommt noch mehr"), dann der Rest. */
	prepare();
	apdu[3] = 0x01u;
	apdu[4] = (uint8_t)(21u + 10u);
	memcpy(apdu + 26, rlp, 10);
	T_EQ("erstes Stueck",
	     osk_apdu_handle(&state, apdu, 5u + 21u + 10u, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("ok, aber noch nichts", sw, OSK_SW_OK);
	T_EQ("keine Antwortdaten", out_len, 0);
	T_EQ("noch nicht gefragt", mock_ui_calls(), 0);

	apdu[2] = 0x80u;
	apdu[3] = 0x00u;
	apdu[4] = (uint8_t)(rlp_len - 10u);
	memcpy(apdu + 5, rlp + 10, rlp_len - 10u);
	T_EQ("zweites Stueck",
	     osk_apdu_handle(&state, apdu, 5u + rlp_len - 10u, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("ok", sw, OSK_SW_OK);
	T_EQ("65 Byte", out_len, 65);
	T_EQ("jetzt gefragt", mock_ui_calls(), 1);

	/* Ein Folgestueck ohne Anfang wird verworfen. */
	prepare();
	apdu[2] = 0x80u;
	T_EQ("bearbeitet",
	     osk_apdu_handle(&state, apdu, 5u + rlp_len - 10u, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("abgelehnt", sw, OSK_SW_WRONG_DATA);

	/* Ein unbekannter Wert in P1 ebenfalls. */
	prepare();
	apdu[2] = 0x55u;
	T_EQ("bearbeitet",
	     osk_apdu_handle(&state, apdu, 5u + rlp_len - 10u, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("abgelehnt", sw, OSK_SW_WRONG_DATA);

	/* Sagt der Nutzer nein, kommt 0x6985 - das versteht jeder Rechner. */
	prepare();
	mock_ui_answer(OSK_ERR_DENIED);
	apdu[2] = 0x00u;
	apdu[3] = 0x00u;
	apdu[4] = (uint8_t)(21u + rlp_len);
	T_EQ("Pfad", t_unhex("058000002c8000003c800000000000000000000000", apdu + 5, 21), 21);
	memcpy(apdu + 26, rlp, rlp_len);
	T_EQ("bearbeitet",
	     osk_apdu_handle(&state, apdu, 5u + 21u + rlp_len, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("abgelehnt vom Nutzer", sw, OSK_SW_DENIED);
}

static void test_sign_message(void)
{
	uint8_t apdu[128], out[256];
	size_t out_len = 0;
	uint16_t sw = 0;
	const char *text = "Hello, Sorglos!";
	size_t text_len = 15;

	prepare();
	apdu[0] = OSK_APDU_CLA;
	apdu[1] = OSK_APDU_INS_SIGN_MESSAGE;
	apdu[2] = 0x00u;
	apdu[3] = 0x00u;
	apdu[4] = (uint8_t)(21u + 4u + text_len);
	T_EQ("Pfad", t_unhex("058000002c8000003c800000000000000000000000", apdu + 5, 21), 21);
	apdu[26] = 0;
	apdu[27] = 0;
	apdu[28] = 0;
	apdu[29] = (uint8_t)text_len;
	memcpy(apdu + 30, text, text_len);
	T_EQ("bearbeitet",
	     osk_apdu_handle(&state, apdu, 5u + 21u + 4u + text_len, out, sizeof(out), &out_len,
			     &sw),
	     OSK_OK);
	T_EQ("ok", sw, OSK_SW_OK);
	T_EQ("65 Byte", out_len, 65);
	T_TRUE("v ist 27 oder 28", out[0] == 27u || out[0] == 28u);
	T_TRUE("Text stand auf dem Schirm", strcmp(mock_ui_subject(), text) == 0);

	/* Eine angekuendigte Laenge ueber der Grenze wird abgelehnt. */
	prepare();
	apdu[26] = 0xffu;
	apdu[27] = 0xffu;
	T_EQ("bearbeitet",
	     osk_apdu_handle(&state, apdu, 5u + 21u + 4u + text_len, out, sizeof(out), &out_len,
			     &sw),
	     OSK_OK);
	T_EQ("zu lang", sw, OSK_SW_WRONG_LENGTH);

	/* Mehr Bytes als angekuendigt: ebenfalls abgelehnt. */
	prepare();
	apdu[26] = 0;
	apdu[27] = 0;
	apdu[28] = 0;
	apdu[29] = 5u;
	T_EQ("bearbeitet",
	     osk_apdu_handle(&state, apdu, 5u + 21u + 4u + text_len, out, sizeof(out), &out_len,
			     &sw),
	     OSK_OK);
	T_EQ("mehr als angekuendigt", sw, OSK_SW_WRONG_LENGTH);
}

static void test_sign_typed(void)
{
	uint8_t apdu[160], out[256];
	size_t out_len = 0;
	uint16_t sw = 0;

	prepare();
	apdu[0] = OSK_APDU_CLA;
	apdu[1] = OSK_APDU_INS_SIGN_TYPED;
	apdu[2] = 0x00u;
	apdu[3] = 0x00u;
	apdu[4] = (uint8_t)(21u + 64u);
	T_EQ("Pfad", t_unhex("058000002c8000003c800000000000000000000000", apdu + 5, 21), 21);
	memset(apdu + 26, 0x11, 32);
	memset(apdu + 58, 0x22, 32);

	/* EIP-712 ueber diesen Befehl wird IMMER abgelehnt, und daran gibt
	 * es nichts zu drehen.
	 *
	 * Warum: der Befehl uebergibt zwei Hashes - den Bereichstrenner und
	 * den Hash der Struktur. Aus zwei Hashes kann das Geraet nicht
	 * zurueckrechnen, was da eigentlich steht. Es koennte also nur
	 * "unterschreibe etwas" anzeigen, und das ist die Blindunterschrift
	 * mit einem hoeflichen Namen.
	 *
	 * Das kostet echte Vertraeglichkeit: MetaMask kann ueber diesen Weg
	 * keine typisierte Nachricht unterschreiben lassen. Das ist der
	 * Preis der Regel, und er ist in docs/wallet-bau.md aufgeschrieben.
	 *
	 * Auch beim zehnten Versuch bleibt es dabei - es gibt keinen
	 * Zustand, der sich dabei aufweicht. */
	for (unsigned i = 0; i < 10u; i++) {
		mock_ui_reset();
		mock_ui_answer(OSK_OK); /* der Nutzer WUERDE ja sagen */
		T_EQ("bearbeitet",
		     osk_apdu_handle(&state, apdu, 5u + 21u + 64u, out, sizeof(out), &out_len,
				     &sw),
		     OSK_OK);
		T_EQ("abgelehnt", sw, OSK_SW_DENIED);
		T_EQ("nichts herausgegeben", (long long)out_len, 0);
		/* Und es wurde nicht einmal gefragt. */
		T_EQ("nicht gefragt", (long long)mock_ui_calls(), 0);
	}

	/* Falsche Laenge: nicht 64 Byte hinter dem Pfad. */
	apdu[4] = (uint8_t)(21u + 60u);
	T_EQ("bearbeitet",
	     osk_apdu_handle(&state, apdu, 5u + 21u + 60u, out, sizeof(out), &out_len, &sw),
	     OSK_OK);
	T_EQ("Laenge falsch", sw, OSK_SW_WRONG_LENGTH);
}

int main(void)
{
	test_path();
	test_header();
	test_pubkey();
	test_sign_tx();
	test_sign_message();
	test_sign_typed();
	return t_report("apdu");
}
