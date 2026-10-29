/* Pruefungen der Yubico-Pruefsumme gegen veroeffentlichte Pruefwerte.
 *
 * Das ist der Teil, der ueberhaupt beweisbar ist, ohne Geraet: eine
 * Pruefsumme hat veroeffentlichte Werte, und wenn die nicht
 * herauskommen, redet unser Parser nicht dieselbe Sprache wie ykman.
 *
 * Pruefwerte [4] (siehe yk_hid.h):
 *   8792ebfe26cc130030c20011c89f          -> uebertragen 0xc823
 *   8792ebfe26cc130030c20011c89f23c8      -> Restwert 0xf0b8
 *   55aa00ff                              -> 0xcbb5
 *
 * Die ersten zwei legen das Verfahren vollstaendig fest: Startwert,
 * Polynom, Drehrichtung und die Verabredung, dass das Einerkomplement
 * uebertragen wird. Beim dritten sagt die Quelle NICHT, ob 0xcbb5 die
 * rohe oder die uebertragene Form ist. Darum wird er nicht behauptet,
 * sondern gedruckt - und geprueft wird daran nur die Eigenschaft, die
 * ohne diese Angabe gilt. Nach der erwarteten Zahl zu suchen, bis eine
 * der beiden Formen passt, wuerde nur den vermuteten Fehler finden. */
#include "yk_test.h"
#include "yk_crc.h"
#include "yk_frame.h"
#include "yk_hid.h"

static void published_vectors(void)
{
	uint8_t buf[32];
	size_t n;

	/* Erster Wert: die uebertragene Form. */
	n = t_unhex("8792ebfe26cc130030c20011c89f", buf, sizeof(buf));
	T_EQ("Vorlage 1 ist 14 Byte lang", n, 14);
	T_EQ("Vorlage 1: uebertragene Pruefsumme", yk_crc16_wire(buf, n), 0xc823);

	/* Zweiter Wert: dieselben Bytes samt angehaengter Pruefsumme,
	 * niederwertiges Byte zuerst - 0xc823 wird 23 c8. Der Restwert
	 * muss stehen bleiben. */
	n = t_unhex("8792ebfe26cc130030c20011c89f23c8", buf, sizeof(buf));
	T_EQ("Vorlage 2 ist 16 Byte lang", n, 16);
	T_EQ("Vorlage 2: Restwert", yk_crc16(buf, n), YK_CRC_RESIDUE);
	T_TRUE("Vorlage 2: Restwertpruefung sagt ja", yk_crc16_residue_ok(buf, n));

	/* Gegenprobe: ein verdrehtes Byte darf den Restwert NICHT
	 * stehen lassen. Ohne diese Probe wuerde eine Pruefsumme, die
	 * immer 0xf0b8 liefert, als richtig durchgehen. */
	buf[3] ^= 0x01u;
	T_TRUE("ein gekipptes Bit faellt auf", !yk_crc16_residue_ok(buf, n));

	/* Dritter Wert: beide Formen werden gedruckt, keine behauptet. */
	n = t_unhex("55aa00ff", buf, sizeof(buf));
	printf("  55aa00ff: roh 0x%04x, uebertragen 0x%04x (Quelle sagt 0xcbb5, "
	       "ohne zu sagen welche Form)\n",
	       (unsigned)yk_crc16(buf, n), (unsigned)yk_crc16_wire(buf, n));
	/* Was ohne diese Angabe gilt: haengt man die uebertragene Form an,
	 * steht der Restwert. */
	buf[n] = (uint8_t)(yk_crc16_wire(buf, n) & 0xffu);
	buf[n + 1u] = (uint8_t)((yk_crc16_wire(buf, n) >> 8) & 0xffu);
	T_TRUE("55aa00ff: eigener Restwert stimmt", yk_crc16_residue_ok(buf, n + 2u));
}

static void edges(void)
{
	uint8_t buf[4] = {0, 0, 0, 0};

	/* Laenge 0: der Startwert, unveraendert. */
	T_EQ("leerer Puffer gibt den Startwert", yk_crc16(buf, 0), 0xffff);
	/* NULL darf nicht in den Zeiger laufen. */
	T_EQ("NULL gibt den Startwert", yk_crc16(NULL, 7), 0xffff);
	T_TRUE("NULL ist kein gueltiger Restwert", !yk_crc16_residue_ok(NULL, 8));
	/* Unter drei Byte gibt es keine Daten neben der Pruefsumme. */
	T_TRUE("zwei Byte sind kein Puffer mit Pruefsumme",
	       !yk_crc16_residue_ok(buf, 2));

	/* Das Komplement ist wirklich das Komplement. */
	T_EQ("roh und uebertragen sind Komplemente",
	     (uint16_t)(yk_crc16(buf, 4) ^ yk_crc16_wire(buf, 4)), 0xffff);
}

static void frame_crc_is_not_residue(void)
{
	/* Die Falle, die diesem Projekt siebenmal passiert ist: zwei
	 * Groessen mit demselben Namen. Im HID-Rahmen liegt das
	 * Steckplatzbyte ZWISCHEN Nutzlast und Pruefsumme. Wer dort den
	 * Restwert ueber den Rahmen rechnet, bekommt bei einem gueltigen
	 * Rahmen fast immer etwas anderes als 0xf0b8 - und wuerde jeden
	 * gueltigen Rahmen abweisen.
	 *
	 * Hier wird das gemessen, damit niemand die beiden Wege vertauscht. */
	uint8_t frame[YK_FRAME_LEN];
	uint8_t payload[YK_SLOT_DATA_LEN];

	for (size_t i = 0; i < sizeof(payload); i++)
		payload[i] = (uint8_t)(i + 1u);

	T_TRUE("Rahmen gebaut", yk_frame_build(YK_SLOT_CHAL_HMAC1, payload, sizeof(payload),
					       frame));

	/* Der richtige Weg: ueber die 64 Nutzlastbytes rechnen. */
	const uint16_t want = (uint16_t)((uint16_t)frame[YK_FRAME_OFF_CRC] |
					 (uint16_t)((uint16_t)frame[YK_FRAME_OFF_CRC + 1] << 8));

	T_EQ("Pruefsumme deckt die Nutzlast", yk_crc16_wire(frame, YK_SLOT_DATA_LEN), want);

	/* Der falsche Weg, gemessen statt behauptet: der Restwert ueber
	 * Nutzlast + Steckplatz + Pruefsumme ist nicht 0xf0b8. */
	const uint16_t over_frame = yk_crc16(frame, YK_FRAME_OFF_CRC + 2u);

	printf("  Restwert ueber Nutzlast+Steckplatz+Pruefsumme: 0x%04x "
	       "(0x%04x waere der Restwert)\n",
	       (unsigned)over_frame, (unsigned)YK_CRC_RESIDUE);
	T_TRUE("der Restwert gilt im Rahmen NICHT", over_frame != YK_CRC_RESIDUE);
}

void suite_ykcrc(void)
{
	printf("== Pruefsumme\n");
	published_vectors();
	edges();
	frame_crc_is_not_residue();
}
