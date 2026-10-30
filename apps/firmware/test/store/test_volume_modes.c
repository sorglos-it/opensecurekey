/* Die drei Betriebsarten GEGENEINANDER. Das ist der eigentliche Pruefsatz:
 * eine Art, die nicht haelt, was ihr Name sagt, ist schlimmer als keine.
 *
 *   1  nur dieser Stick   der falsche Stick darf NICHT oeffnen
 *   2  mit Kartenwort     das falsche Wort darf NICHT oeffnen
 *   3  nur gesiegelt      JEDER Stick darf lesen - und es ist nicht geheim
 *
 * Attrappen, kein Geraet. */

#include <string.h>
#include "osk_test.h"
#include "test_util.h"
#include "osk_volume.h"
#include "fake_card.h"
#include "fake_se050.h"
#include "test_volume.h"

#define BLOCKS 256u

static const char WORD_RIGHT[] = "amsel birke chrom dach eiche feder gras hafen igel joch";
static const char WORD_WRONG[] = "amsel birke chrom dach eiche feder gras hafen igel kork";

static uint8_t buf_in[OSK_VOL_BLOCK_LEN];
static uint8_t buf_out[OSK_VOL_BLOCK_LEN];

static void fill(uint8_t seed)
{
	for (uint32_t i = 0u; i < OSK_VOL_BLOCK_LEN; i++) {
		buf_in[i] = (uint8_t)(seed ^ (uint8_t)i);
	}
}

/* Eine Karte in der gewuenschten Art beschreiben, von Stick 1 aus. */
static void written_by_stick1(osk_vol_mode_t mode, const osk_vol_recipient_t *recv,
			      uint32_t recv_count)
{
	fake_se050_reset();
	vt_stick(1u);
	vt_card(BLOCKS);
	vt_ui_reset(false);
	T_OK(vt_init());
	T_OK(osk_volume_format(mode, recv, recv_count, BLOCKS));
	T_OK(osk_volume_write_enable());
	fill(0x5au);
	T_OK(osk_volume_block_write(17u, buf_in));
	T_OK(osk_volume_commit());
	osk_volume_close();
}

/* --- Modus 1: nur dieser Stick ---------------------------------------- */

static void case_mode1(void)
{
	osk_vol_recipient_t recv = {.kind = OSK_VOL_RECV_DEVICE};
	osk_vol_sender_t who;

	osk_test_case("Modus 1: der Stick, der beschrieben hat, liest");
	written_by_stick1(OSK_VOL_MODE_DEVICE, &recv, 1u);
	vt_stick(1u);
	T_OK(vt_init());
	T_OK(osk_volume_open(NULL));
	T_EQ(osk_volume_mode(), OSK_VOL_MODE_DEVICE);
	T_TRUE(osk_vol_mode_secret(OSK_VOL_MODE_DEVICE));
	T_OK(osk_volume_root_unchanged());
	T_OK(osk_volume_block_read(17u, buf_out));
	T_MEM(buf_in, buf_out, OSK_VOL_BLOCK_LEN);
	/* In Modus 1 gibt es keinen Absender - derselbe Stick prueft. */
	T_OK(osk_volume_sender(&who));
	T_EQ(who.state, OSK_VOL_SENDER_SELF);
	osk_volume_close();

	osk_test_case("Modus 1: der FALSCHE Stick kommt nicht hinein");
	/* Nicht, weil er sich weigert, sondern weil er nichts herleiten kann:
	 * sein Grundgeheimnis ist ein anderes. */
	vt_stick(2u);
	T_OK(vt_init());
	T_RES(osk_volume_open(NULL), OSK_ERR_DENIED);
	T_TRUE(!osk_volume_is_open());
	/* Und ohne geoeffneten Datentraeger gibt es auch keinen Block. */
	T_RES(osk_volume_block_read(17u, buf_out), OSK_ERR_ARG);
}

/* --- Modus 2: mit Kartenwort ------------------------------------------ */

static void case_mode2(void)
{
	osk_vol_recipient_t recv[2];
	osk_vol_recipient_t unlock;
	osk_vol_sender_t who;

	memset(recv, 0, sizeof(recv));
	recv[0].kind = OSK_VOL_RECV_DEVICE;
	recv[1].kind = OSK_VOL_RECV_WORD;
	recv[1].word = WORD_RIGHT;
	recv[1].word_len = sizeof(WORD_RIGHT) - 1u;

	osk_test_case("Modus 2: ein fremder Stick mit dem richtigen Wort liest");
	written_by_stick1(OSK_VOL_MODE_WORD, recv, 2u);
	/* Der Absender darf selbst auch noch lesen - darum beide Zeilen. Der
	 * Preis steht im Konzept: die Kette ist so stark wie ihr schwaechstes
	 * Glied, und die Anzeige nennt darum die Zahl der Empfaenger. */
	T_EQ(vt_asked_recipients(), 2u);

	vt_stick(2u);
	T_OK(vt_init());
	memset(&unlock, 0, sizeof(unlock));
	unlock.kind = OSK_VOL_RECV_WORD;
	unlock.word = WORD_RIGHT;
	unlock.word_len = sizeof(WORD_RIGHT) - 1u;
	T_OK(osk_volume_open(&unlock));
	T_EQ(osk_volume_mode(), OSK_VOL_MODE_WORD);
	T_TRUE(osk_vol_mode_secret(OSK_VOL_MODE_WORD));
	T_OK(osk_volume_root_unchanged());
	T_OK(osk_volume_block_read(17u, buf_out));
	T_MEM(buf_in, buf_out, OSK_VOL_BLOCK_LEN);

	osk_test_case("Modus 2: unveraendert - aber der Absender ist UNBEKANNT");
	/* Das ist die ganze Grenze zwischen Echtheit und Urheberschaft, und
	 * sie steht hier als Pruefung, nicht als Kommentar. Die Signatur geht
	 * auf; wer sie gesetzt hat, weiss dieser Stick nicht. */
	T_OK(osk_volume_sender(&who));
	T_EQ(who.state, OSK_VOL_SENDER_UNKNOWN);
	T_TRUE(who.version > 0u);
	osk_volume_close();

	osk_test_case("Modus 2: das FALSCHE Wort kommt nicht hinein");
	vt_stick(2u);
	T_OK(vt_init());
	unlock.word = WORD_WRONG;
	unlock.word_len = sizeof(WORD_WRONG) - 1u;
	T_RES(osk_volume_open(&unlock), OSK_ERR_DENIED);
	T_TRUE(!osk_volume_is_open());

	osk_test_case("Modus 2: gar kein Wort kommt auch nicht hinein");
	T_OK(vt_init());
	T_RES(osk_volume_open(NULL), OSK_ERR_DENIED);
}

/* --- Modus 3: nur gesiegelt, NICHT geheim ----------------------------- */

static void case_mode3(void)
{
	osk_vol_sender_t who;
	uint32_t i;

	osk_test_case("Modus 3: jeder Stick liest - ohne Wort, ohne Chipschluessel");
	written_by_stick1(OSK_VOL_MODE_SEALED, NULL, 0u);
	for (i = 1u; i <= 3u; i++) {
		vt_stick(i);
		T_OK(vt_init());
		T_OK(osk_volume_open(NULL));
		T_EQ(osk_volume_mode(), OSK_VOL_MODE_SEALED);
		T_OK(osk_volume_root_unchanged());
		T_OK(osk_volume_block_read(17u, buf_out));
		T_MEM(buf_in, buf_out, OSK_VOL_BLOCK_LEN);
		osk_volume_close();
	}

	osk_test_case("Modus 3 ist NICHT geheim, und das sagt die Schnittstelle");
	T_TRUE(!osk_vol_mode_secret(OSK_VOL_MODE_SEALED));
	/* Und der Name faengt mit dem Wort an, das der Nutzer lesen soll. */
	T_TRUE(strncmp(osk_vol_mode_text(OSK_VOL_MODE_SEALED), "OFFEN", 5u) == 0);
	T_TRUE(strncmp(osk_vol_mode_text(OSK_VOL_MODE_DEVICE), "OFFEN", 5u) != 0);
	/* Die Anzeige wurde vor dem Beschreiben mit secret=false gefragt. Das
	 * ist der schlimmste Fehler dieses Geraets, abgefangen: wer glaubt
	 * verschluesselt zu haben und hat nur gesiegelt, gibt Geheimnisse weg. */
	T_TRUE(!vt_asked_secret());
	T_EQ(vt_asked_mode(), OSK_VOL_MODE_SEALED);

	osk_test_case("Modus 3: ein veraenderter Block fliegt trotzdem auf");
	/* Nicht geheim heisst nicht ungeschuetzt. Baum und Signatur hat auch
	 * Modus 3; an die Stelle des Stempels tritt ein Hash. */
	{
		osk_vol_layout_t lay;

		T_OK(osk_vol_layout(BLOCKS, &lay));
		fake_card_flip(lay.first_data_page + 17u, 7u);
	}
	vt_stick(2u);
	T_OK(vt_init());
	T_OK(osk_volume_open(NULL));
	T_OK(osk_volume_root_unchanged());
	T_RES(osk_volume_block_read(17u, buf_out), OSK_ERR_FORMAT);
	T_OK(osk_volume_sender(&who));
	T_EQ(who.state, OSK_VOL_SENDER_UNKNOWN);
	osk_volume_close();

	osk_test_case("Modus 3: versiegelt heisst schreibgeschuetzt");
	written_by_stick1(OSK_VOL_MODE_SEALED, NULL, 0u);
	vt_stick(1u);
	T_OK(vt_init());
	T_OK(osk_volume_open(NULL));
	T_OK(osk_volume_write_enable());
	T_OK(osk_volume_seal());
	T_TRUE(osk_volume_sealed());
	/* Ab hier weist der Stick jeden Schreibbefehl ab - sonst wuerde er
	 * unterschreiben, was das Betriebssystem hineingeschrieben hat. */
	T_RES(osk_volume_write_enable(), OSK_ERR_DENIED);
	T_RES(osk_volume_block_write(1u, buf_in), OSK_ERR_DENIED);
	/* Lesen geht weiter, und das Siegel haelt. */
	T_OK(osk_volume_root_unchanged());
	T_OK(osk_volume_block_read(17u, buf_out));
	osk_volume_close();

	osk_test_case("Versiegeln gibt es nur in Modus 3");
	written_by_stick1(OSK_VOL_MODE_DEVICE, &(osk_vol_recipient_t){.kind = OSK_VOL_RECV_DEVICE},
			  1u);
	vt_stick(1u);
	T_OK(vt_init());
	T_OK(osk_volume_open(NULL));
	T_RES(osk_volume_seal(), OSK_ERR_UNSUPPORTED);
	osk_volume_close();
}

/* --- Was die drei Arten NICHT durcheinanderbringen duerfen ------------- */

static void case_no_mixing(void)
{
	osk_vol_recipient_t recv = {.kind = OSK_VOL_RECV_DEVICE};
	osk_vol_recipient_t stick;

	osk_test_case("Modus 3 mit Empfaengerzeile ist ein Widerspruch");
	fake_se050_reset();
	vt_stick(1u);
	vt_card(BLOCKS);
	vt_ui_reset(false);
	T_OK(vt_init());
	/* Wer in Modus 3 einen Empfaenger angibt, meint etwas anderes als
	 * Modus 3. Genau diese Verwechslung darf nicht durchgehen. */
	T_RES(osk_volume_format(OSK_VOL_MODE_SEALED, &recv, 1u, BLOCKS), OSK_ERR_ARG);
	/* Und umgekehrt: Modus 1 und 2 ohne Empfaenger waere ein Datentraeger
	 * ohne Schluesselzeile - also unlesbar. */
	T_RES(osk_volume_format(OSK_VOL_MODE_DEVICE, NULL, 0u, BLOCKS), OSK_ERR_ARG);
	/* Die Anzeige wurde dabei nicht einmal gefragt - abgewiesen wird
	 * vorher. */
	T_EQ(vt_asked_calls(), 0u);

	osk_test_case("Der Empfaenger 'anderer Stick' ist NICHT gebaut und sagt das");
	/* Ob der SE051C2 Curve25519-ECDH kann, ist nicht bestaetigt (offener
	 * Punkt 6 des Konzepts). Eine Attrappe, die hier ein gemeinsames
	 * Geheimnis aus zwei oeffentlichen Teilen zusammenrechnet, wuerde die
	 * EIGENE Seite des Vertrags pruefen - also lieber ein ehrliches
	 * UNSUPPORTED. */
	memset(&stick, 0, sizeof(stick));
	stick.kind = OSK_VOL_RECV_STICK;
	T_RES(osk_volume_format(OSK_VOL_MODE_WORD, &stick, 1u, BLOCKS), OSK_ERR_UNSUPPORTED);
	osk_volume_close();
	fake_card_free();
}

void test_volume_modes(void)
{
	case_mode1();
	case_mode2();
	case_mode3();
	case_no_mixing();
}
