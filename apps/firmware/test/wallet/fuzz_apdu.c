/* Fuzzlauf ueber die APDU-Schicht der Geldboerse.
 *
 * Warum gerade hier: diese Schnittstelle erreicht ein Angreifer
 * UNMITTELBAR. Jeder Rechner, an dem der Stick steckt, kann APDUs
 * schicken, ohne dass ein Mensch zustimmt - die Freigabe am Taster kommt
 * erst, wenn diese Schicht die Bytes schon gelesen, einen Pfad gebaut und
 * ein PSBT oder eine Transaktion zusammengesetzt hat. Alles, was vor der
 * Freigabe passiert, passiert auf Zuruf eines Fremden.
 *
 * Gebaut nach dem Muster von test/msc/fuzz_scsi.c.
 *
 * Aufruf: fuzz_apdu [Sekunden] [Startwert]
 *
 * Zusicherungen:
 *   1. Keine APDU darf den Prozess umbringen - kein Absturz, kein
 *      Zugriff daneben, kein Ueberlauf (ASan und UBSan laufen mit).
 *   2. Auf jede APDU kommt genau eine Antwort mit einem Statuswort.
 *   3. Es gehen nie mehr Antwortdaten hinaus, als der Puffer hergibt.
 *      Das ist die Zusicherung, die ein Ueberlauf brechen wuerde.
 *   4. **Ohne Freigabe am Geraet entsteht NIE eine Unterschrift.** Der
 *      Taster sagt in diesem Lauf immer nein; danach darf kein einziger
 *      Schluessel in den Chip gegangen sein.
 *
 *      Damit das etwas heisst, muss der Lauf das Unterschreiben
 *      WIRKLICH erreichen. Zufaellige Bytes ergeben nie ein gueltiges
 *      PSBT - ein Lauf aus lauter Zufall wuerde diese Zusicherung
 *      niemals auf die Probe stellen und waere damit keine Pruefung.
 *      Darum wird regelmaessig das echte PSBT aus psbt_vectors.h
 *      ordentlich zerteilt und durchgeschickt, und am Ende wird
 *      gezaehlt, wie oft die Freigabe wirklich verlangt wurde. Ist das
 *      nie passiert, gilt der Lauf als rot.
 *   5. Ein Statuswort ist immer eines der bekannten - nie eine Zahl, die
 *      sich aus einem uninitialisierten Byte ergeben hat.
 *   6. Der Lauf muss echte Befehle durchbekommen und echte Abweisungen
 *      erzeugt haben - sonst hat er nur die erste Wand getroffen.
 *   7. **Wer das Blindsignieren VERLANGT, bekommt nie ein 0x9000.** Ein
 *      gesetztes Eigenschaftsbit still zu ueberlesen waere schlimmer als
 *      gar keine Pruefung: der Rechner haelte es fuer eingeschaltet. Die
 *      Antwort muss OSK_SW_BLIND_REFUSED sein, und der Lauf zaehlt mit,
 *      wie oft wirklich danach gefragt wurde - nie = rot. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "wallet_apdu.h"
#include "wallet_seed.h"
#include "psbt_vectors.h"
#include "mocks.h"

static uint64_t g_rng;

static size_t unhex(const char *hex, uint8_t *out, size_t cap)
{
	size_t n = 0;

	for (size_t i = 0; hex[i] != 0 && hex[i + 1u] != 0; i += 2u) {
		unsigned hi, lo;

		if (n >= cap)
			return 0;
		hi = (unsigned)((hex[i] <= '9') ? hex[i] - '0' : (hex[i] | 32) - 'a' + 10);
		lo = (unsigned)((hex[i + 1u] <= '9') ? hex[i + 1u] - '0'
						     : (hex[i + 1u] | 32) - 'a' + 10);
		out[n++] = (uint8_t)((hi << 4) | lo);
	}
	return n;
}

static osk_item_id_t g_seed_id;
static uint8_t g_psbt[OSK_APDU_ASSEMBLY_MAX];
static size_t g_psbt_len;

static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (uint32_t)(g_rng >> 32);
}

/* Die Befehle, die es wirklich gibt. Rein zufaellige Bytes treffen fast
 * immer "unbekannter Befehl" und pruefen dann nur eine Zeile. */
static const uint8_t KNOWN_INS[] = {
    OSK_APDU_INS_GET_PUBKEY, OSK_APDU_INS_SIGN_TX,	OSK_APDU_INS_GET_CONFIG,
    OSK_APDU_INS_SIGN_MESSAGE, OSK_APDU_INS_SIGN_TYPED, OSK_APDU_INS_SIGN_PSBT};

/* Die Statusworte, die diese Schicht kennt. Alles andere waere eine Zahl,
 * die niemand gesetzt hat. */
static bool known_sw(uint16_t sw)
{
	switch (sw) {
	case OSK_SW_OK:
	case OSK_SW_DENIED:
	case OSK_SW_WRONG_DATA:
	case OSK_SW_WRONG_LENGTH:
	case OSK_SW_INS_UNKNOWN:
	case OSK_SW_CLA_UNKNOWN:
	case OSK_SW_LOCKED:
	case OSK_SW_BUSY:
	case OSK_SW_BLIND_REFUSED:
		return true;
	default:
		return false;
	}
}

/* Einen BIP32-Pfad schreiben, wie die Befehle ihn erwarten: ein Byte
 * Anzahl, dann je vier Byte hoechstwertiges zuerst. Manchmal absichtlich
 * mit einer Anzahl, die nicht zu den Daten passt. */
static size_t write_path(uint8_t *out, size_t cap, bool *sane_out)
{
	const uint32_t steps = rnd() % 12u; /* ueber OSK_BIP32_MAX_DEPTH hinaus */
	size_t need = 1u + (size_t)steps * 4u;

	if (need > cap) {
		*sane_out = false;
		out[0] = (uint8_t)steps;
		return 1u;
	}
	out[0] = (uint8_t)steps;
	for (uint32_t i = 0; i < steps; i++) {
		uint32_t v = rnd();

		/* Oft gehaertet, so wie echte Pfade. */
		if ((rnd() % 2u) == 0u)
			v |= 0x80000000u;
		out[1u + i * 4u] = (uint8_t)(v >> 24);
		out[2u + i * 4u] = (uint8_t)(v >> 16);
		out[3u + i * 4u] = (uint8_t)(v >> 8);
		out[4u + i * 4u] = (uint8_t)v;
	}
	/* Manchmal luegt das Byte Anzahl ueber die Daten dahinter. */
	if ((rnd() % 8u) == 0u) {
		out[0] = (uint8_t)(steps + 1u + (rnd() % 4u));
		*sane_out = false;
	}
	return need;
}

/* Baut eine APDU, die oft fast richtig ist. */
static size_t build(uint8_t *out, size_t cap, bool *plausible_out)
{
	const unsigned kind = rnd() % 10u;

	*plausible_out = false;

	if (kind == 0u) {
		/* Ganz zufaellig, auch mit krummer Laenge: der Fall, in dem
		 * nicht einmal der Rahmen stimmt. */
		const size_t n = (size_t)(rnd() % (cap + 1u));

		for (size_t i = 0; i < n; i++)
			out[i] = (uint8_t)rnd();
		return n;
	}
	if (kind == 3u) {
		/* DIE BITTE UM DIE BLINDUNTERSCHRIFT. Ein Rechner, der das
		 * Eigenschaftsbit setzt, muss einen Fehler bekommen und nicht
		 * ein stilles 0x9000. Aus zufaelligen Bytes entsteht dieser
		 * Fall kaum - also wird er gebaut. */
		out[0] = (uint8_t)OSK_APDU_CLA;
		out[1] = (uint8_t)OSK_APDU_INS_GET_CONFIG;
		if ((rnd() % 2u) == 0u) {
			/* ueber P1 oder P2 */
			out[2] = (uint8_t)(((rnd() % 2u) == 0u) ? OSK_APDU_FLAG_BLIND : 0u);
			out[3] = (uint8_t)((out[2] == 0u) ? OSK_APDU_FLAG_BLIND : 0u);
			return 4u;
		}
		/* oder ueber ein Datenbyte */
		out[2] = 0x00u;
		out[3] = 0x00u;
		out[4] = 0x01u;
		out[5] = (uint8_t)OSK_APDU_FLAG_BLIND;
		return 6u;
	}
	if (kind == 2u) {
		/* DER ABGESCHNITTENE BEFEHL: das Laengenbyte kuendigt viel an,
		 * dahinter kommt fast nichts. Genau so sieht es aus, wenn eine
		 * Uebertragung abbricht - oder wenn jemand den Zerleger dazu
		 * bringen will, hinter das Ende zu lesen.
		 *
		 * Diese Sorte muss es geben: aus zufaelligen Bytes entsteht sie
		 * so gut wie nie, und damit waere die Pruefung auf das
		 * Laengenbyte nicht zu bekommen. */
		const size_t echt = (size_t)(rnd() % 6u); /* 0 bis 5 Byte Daten */

		out[0] = (uint8_t)OSK_APDU_CLA;
		out[1] = KNOWN_INS[rnd() % (sizeof(KNOWN_INS) / sizeof(KNOWN_INS[0]))];
		out[2] = (uint8_t)(rnd() % 2u);
		out[3] = (uint8_t)(rnd() % 2u);
		/* Viel angekuendigt, wenig dabei. */
		out[4] = (uint8_t)(32u + (rnd() % 200u));
		for (size_t i = 0; i < echt; i++) {
			/* Ein Pfad, der viele Schritte ankuendigt: dann will
			 * der Zerleger weit mehr lesen, als da ist. */
			out[5u + i] = (i == 0u) ? (uint8_t)(4u + (rnd() % 8u)) : (uint8_t)rnd();
		}
		return 5u + echt;
	}
	if (kind == 1u) {
		/* Zu kurz, um auch nur den Kopf zu enthalten. */
		const size_t n = (size_t)(rnd() % 5u);

		for (size_t i = 0; i < n; i++)
			out[i] = (uint8_t)rnd();
		return n;
	}

	/* Ein Kopf, der meistens stimmt. */
	out[0] = ((rnd() % 16u) == 0u) ? (uint8_t)rnd() : (uint8_t)OSK_APDU_CLA;
	out[1] = ((rnd() % 8u) == 0u) ? (uint8_t)rnd()
				      : KNOWN_INS[rnd() % (sizeof(KNOWN_INS) / sizeof(KNOWN_INS[0]))];
	out[2] = (uint8_t)(rnd() % 4u); /* P1: oft 0 oder 1, manchmal daneben */
	out[3] = (uint8_t)(rnd() % 3u);

	{
		size_t len = 0;
		bool sane = true;

		switch (out[1]) {
		case OSK_APDU_INS_GET_CONFIG:
			break;
		case OSK_APDU_INS_GET_PUBKEY:
			len = write_path(out + 5, cap - 5u, &sane);
			break;
		case OSK_APDU_INS_SIGN_TYPED: {
			/* Pfad, dann zwei Hashes zu je 32 Byte. */
			len = write_path(out + 5, cap - 5u, &sane);
			{
				const size_t want = ((rnd() % 4u) == 0u) ? (rnd() % 80u) : 64u;

				if (want != 64u)
					sane = false;
				for (size_t i = 0; i < want && 5u + len + i < cap; i++)
					out[5u + len + i] = (uint8_t)rnd();
				len += (want < cap - 5u - len) ? want : 0u;
			}
			break;
		}
		case OSK_APDU_INS_SIGN_TX:
		case OSK_APDU_INS_SIGN_MESSAGE:
		case OSK_APDU_INS_SIGN_PSBT:
		default: {
			/* Pfad und dann irgendwelche Nutzdaten - genau das,
			 * was der Zerleger auseinandernehmen muss. */
			size_t payload;

			len = write_path(out + 5, cap - 5u, &sane);
			payload = (size_t)(rnd() % 64u);
			for (size_t i = 0; i < payload && 5u + len + i < cap; i++) {
				/* Oft Bytes, die in einem PSBT oder in RLP
				 * etwas bedeuten - sonst trifft der Lauf nur
				 * "unverstanden". */
				switch (rnd() % 4u) {
				case 0:
					out[5u + len + i] = (uint8_t)"psbt\xff"[rnd() % 5u];
					break;
				case 1:
					out[5u + len + i] = (uint8_t)(0xf8u + (rnd() % 8u));
					break;
				default:
					out[5u + len + i] = (uint8_t)rnd();
					break;
				}
			}
			len += (payload < cap - 5u - len) ? payload : 0u;
			break;
		}
		}

		/* Das Laengenbyte: meistens richtig, manchmal gelogen. */
		if ((rnd() % 8u) == 0u) {
			out[4] = (uint8_t)rnd();
			sane = false;
		} else {
			out[4] = (uint8_t)len;
		}
		*plausible_out = sane && out[0] == (uint8_t)OSK_APDU_CLA;
		return 5u + len;
	}
}

/* Das echte PSBT so durchschicken, wie ein Rechner es taete: erstes
 * Stueck mit P1=0x00, weitere mit 0x80, P2=0x01 solange noch etwas folgt
 * und 0x00 beim letzten. Dann laeuft die Schicht bis zur Freigabe - und
 * die sagt in diesem Lauf nein.
 *
 * Liefert 1, wenn etwas nicht stimmt. */
static int send_psbt(osk_apdu_state_t *state, uint8_t *out, size_t cap)
{
	size_t sent = 0;
	bool first = true;

	osk_apdu_init(state);
	state->seed_id = g_seed_id;
	state->btc_hrp = "bc";
	state->btc_p2pkh_version = 0x00u;
	state->btc_p2sh_version = 0x05u;
	state->eth_unit = "ETH";

	while (sent < g_psbt_len) {
		uint8_t apdu[5 + 255];
		size_t chunk = g_psbt_len - sent;
		size_t out_len = 0;
		uint16_t sw = 0xFFFFu;
		bool last;

		if (chunk > 200u)
			chunk = 200u;
		last = (sent + chunk >= g_psbt_len);

		apdu[0] = (uint8_t)OSK_APDU_CLA;
		apdu[1] = (uint8_t)OSK_APDU_INS_SIGN_PSBT;
		apdu[2] = (uint8_t)(first ? 0x00u : 0x80u);
		apdu[3] = (uint8_t)(last ? 0x00u : 0x01u);
		apdu[4] = (uint8_t)chunk;
		memcpy(apdu + 5, g_psbt + sent, chunk);

		if (osk_apdu_handle(state, apdu, 5u + chunk, out, cap, &out_len, &sw) != OSK_OK) {
			printf("FEHLER: das echte PSBT wurde nicht bearbeitet\n");
			return 1;
		}
		if (!last && sw != OSK_SW_OK) {
			printf("FEHLER: Zwischenstueck abgewiesen (0x%04x)\n", sw);
			return 1;
		}
		if (last) {
			/* DAS ist der Punkt: der Taster hat nein gesagt, also
			 * muss hier eine Abweisung stehen - keine Unterschrift. */
			if (sw != OSK_SW_DENIED) {
				printf("FEHLER: der Taster sagte nein, das Geraet antwortete "
				       "0x%04x\n",
				       sw);
				return 1;
			}
			if (out_len != 0u) {
				printf("FEHLER: %zu Byte Antwort trotz Ablehnung\n", out_len);
				return 1;
			}
		}
		sent += chunk;
		first = false;
	}
	return 0;
}

int main(int argc, char **argv)
{
	const long secs = (argc > 1) ? strtol(argv[1], NULL, 10) : 5;
	uint8_t apdu[OSK_APDU_ASSEMBLY_MAX + 64];
	uint8_t out[OSK_APDU_ASSEMBLY_MAX + 64];
	osk_apdu_state_t state;
	unsigned long runs = 0, answered = 0, ok_count = 0, refused = 0;
	unsigned long plausible = 0, out_bytes = 0, deep = 0, blind_asked = 0;
	time_t start;

	g_rng = (argc > 2) ? strtoull(argv[2], NULL, 10) : 0x5ec0de5eed1234ULL;
	if (g_rng == 0)
		g_rng = 1;

	/* Der Taster sagt in diesem ganzen Lauf NEIN. Alles, was trotzdem
	 * eine Unterschrift ergibt, ist ein Weg um die Freigabe herum. */
	mock_se050_reset();
	mock_ui_reset();
	mock_ui_answer(OSK_ERR_DENIED);

	/* Das echte PSBT und der Seed dazu - ohne beides kommt der Lauf nie
	 * bis zur Freigabe, und Zusicherung 4 waere leeres Gerede. */
	{
		osk_item_id_t seed_id = 0;

		mock_ui_answer(OSK_OK); /* nur zum Ablegen des Seeds */
		if (osk_wallet_seed_store(0, "fuzz", OWN_MNEMONIC, NULL, &seed_id) != OSK_OK) {
			printf("FEHLER: Seed laesst sich nicht ablegen\n");
			return 1;
		}
		g_seed_id = seed_id;
		mock_ui_answer(OSK_ERR_DENIED); /* ab jetzt sagt der Taster nein */
	}
	g_psbt_len = unhex(OWN_PSBT, g_psbt, sizeof(g_psbt));
	if (g_psbt_len == 0u) {
		printf("FEHLER: das echte PSBT ist leer - der Lauf wuerde den tiefen Weg nie "
		       "erreichen\n");
		return 1;
	}

	start = time(NULL);
	while (time(NULL) - start < secs) {
		bool plaus = false;

		/* Jeder zwanzigste Durchgang schickt das echte PSBT
		 * ordentlich zerteilt durch - bis zur Freigabe. */
		if ((rnd() % 20u) == 0u) {
			deep++;
			if (send_psbt(&state, out, sizeof(out)) != 0)
				return 1;
			runs++;
			continue;
		}
		size_t in_len = build(apdu, sizeof(apdu), &plaus);
		size_t out_len = 12345u; /* erkennbarer Unsinn vorher */
		uint16_t sw = 0xFFFFu;
		osk_result_t res;

		/* Jede APDU auf einem frischen Zustand: ein hinterlassener
		 * halber Zusammenbau soll den naechsten Fall nicht erklaeren. */
		osk_apdu_init(&state);
		state.seed_id = 1u;
		state.btc_hrp = "bc";
		state.btc_p2pkh_version = 0x00u;
		state.btc_p2sh_version = 0x05u;
		state.eth_unit = "ETH";

		/* Die APDU in einen PASSGENAUEN Puffer umkopieren, bevor sie
		 * hineingeht.
		 *
		 * Das ist der Unterschied zwischen einem Fuzzer, der etwas
		 * findet, und einem, der sich selbst beruhigt: liegt die APDU
		 * in einem grossen Puffer, dann landet ein Zerleger, der dem
		 * Laengenbyte blind glaubt, immer noch INNERHALB des Puffers,
		 * und niemand merkt es. In einem passgenauen Stueck Speicher
		 * schlaegt ASan beim ersten Byte zu weit an. */
		{
			uint8_t *exact = malloc(in_len > 0u ? in_len : 1u);

			if (exact == NULL) {
				printf("FEHLER: kein Speicher\n");
				return 1;
			}
			memcpy(exact, apdu, in_len);
			res = osk_apdu_handle(&state, exact, in_len, out, sizeof(out), &out_len,
					      &sw);
			free(exact);
		}
		runs++;
		if (plaus)
			plausible++;

		if (res != OSK_OK) {
			/* Nur der unbrauchbare Aufruf darf so enden, und dann
			 * ohne Antwort. */
			if (res != OSK_ERR_ARG) {
				printf("FEHLER: Rueckgabe %d bei %zu Byte (INS 0x%02x)\n", (int)res,
				       in_len, in_len > 1u ? apdu[1] : 0u);
				return 1;
			}
			continue;
		}

		/* Zusicherung 2 und 5: es gibt eine Antwort, und das
		 * Statuswort ist eines der bekannten. */
		if (!known_sw(sw)) {
			printf("FEHLER: unbekanntes Statuswort 0x%04x (INS 0x%02x, %zu Byte)\n", sw,
			       in_len > 1u ? apdu[1] : 0u, in_len);
			return 1;
		}

		/* Zusicherung 3: nie mehr Daten als der Puffer hergibt. */
		if (out_len > sizeof(out)) {
			printf("FEHLER: %zu Byte Antwort, der Puffer hat %zu (INS 0x%02x)\n",
			       out_len, sizeof(out), in_len > 1u ? apdu[1] : 0u);
			return 1;
		}
		/* Und bei einem Fehler gar keine. */
		if (sw != OSK_SW_OK && out_len != 0u) {
			printf("FEHLER: %zu Byte Antwort trotz Statuswort 0x%04x (INS 0x%02x)\n",
			       out_len, sw, in_len > 1u ? apdu[1] : 0u);
			return 1;
		}

		/* Zusicherung 7: wer das Blindsignieren verlangt, bekommt NIE
		 * ein 0x9000. Ein stilles Ja waere hier der ganze Schaden. */
		if (in_len >= 4u && apdu[0] == (uint8_t)OSK_APDU_CLA &&
		    apdu[1] == (uint8_t)OSK_APDU_INS_GET_CONFIG) {
			/* Nur bei einem Rahmen, der SONST in Ordnung ist.
			 * Luegt das Laengenbyte, wird die APDU vorher als
			 * kaputt abgewiesen - das ist richtig und auch nicht
			 * still: der Rechner bekommt einen Fehler. Diese
			 * Zusicherung gilt dem Fall, in dem nur die Bitte
			 * selbst der Grund zur Abweisung ist. */
			const bool rahmen_ok = (in_len == 4u) || (in_len == 5u + (size_t)apdu[4]);
			const bool asked_blind =
			    rahmen_ok &&
			    ((((apdu[2] | apdu[3]) & OSK_APDU_FLAG_BLIND) != 0u) ||
			     (in_len >= 6u && apdu[4] >= 1u &&
			      (apdu[5] & OSK_APDU_FLAG_BLIND) != 0u));

			if (asked_blind) {
				blind_asked++;
				if (sw == OSK_SW_OK) {
					printf("FEHLER: das Blindsignieren wurde verlangt und "
					       "mit 0x9000 beantwortet\n");
					return 1;
				}
				if (sw != OSK_SW_BLIND_REFUSED) {
					printf("FEHLER: Blindsignieren verlangt, Antwort 0x%04x "
					       "statt 0x%04x\n",
					       sw, OSK_SW_BLIND_REFUSED);
					return 1;
				}
			}
		}

		answered++;
		out_bytes += out_len;
		if (sw == OSK_SW_OK)
			ok_count++;
		else
			refused++;
	}

	/* Zusicherung 4, die eigentliche: der Taster hat immer nein gesagt,
	 * also darf kein Schluessel in den Chip gegangen sein. Genau hier
	 * waere ein Weg um die Freigabe herum zu sehen. */
	if (mock_se050_imports() != 0u) {
		printf("FEHLER: %u Schluessel gingen in den Chip, obwohl nie freigegeben wurde\n",
		       mock_se050_imports());
		return 1;
	}

	printf("fuzz_apdu: %lu APDUs, %lu beantwortet, %lu gut, %lu abgewiesen, "
	       "%lu plausibel gebaut, %lu Byte Antwortdaten, %lu mal bis zur Freigabe, "
	       "%lu mal blind verlangt\n",
	       runs, answered, ok_count, refused, plausible, out_bytes, deep, blind_asked);

	/* Zusicherung 6: ein Lauf, der nur die erste Wand getroffen hat,
	 * hat nichts geprueft. */
	if (runs == 0u) {
		printf("FEHLER: kein Lauf\n");
		return 1;
	}
	if (answered == 0u) {
		printf("FEHLER: keine einzige Antwort entstanden\n");
		return 1;
	}
	if (plausible == 0u) {
		printf("FEHLER: keine plausible APDU gebaut - der Lauf hat den Zerleger nie "
		       "erreicht\n");
		return 1;
	}
	if (deep == 0u) {
		printf("FEHLER: der Lauf kam nie bis zur Freigabe - Zusicherung 4 wurde nicht "
		       "geprueft\n");
		return 1;
	}
	if (blind_asked == 0u) {
		printf("FEHLER: im ganzen Lauf hat nie jemand das Blindsignieren verlangt - "
		       "Zusicherung 7 wurde nicht geprueft\n");
		return 1;
	}
	if (ok_count == 0u || refused == 0u) {
		printf("FEHLER: nur eine Sorte Antwort (%lu gut, %lu abgewiesen) - der Lauf hat "
		       "nur eine Seite geprueft\n",
		       ok_count, refused);
		return 1;
	}
	printf("fuzz_apdu: ok\n");
	return 0;
}
