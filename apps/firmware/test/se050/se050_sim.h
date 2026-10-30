/* Nachbau der Gegenseite, nur fuer die Tests.
 *
 * Der Nachbau beantwortet T=1'-Rahmen so, wie der Treiber es von einem
 * SE050 erwartet: Schnittstelle zuruecksetzen, Applet auswaehlen, SCP03
 * aufbauen, danach verschluesselte Befehle. Damit laeuft osk_se050_open()
 * bis zum stehenden Kanal durch, ohne Hardware.
 *
 * WICHTIG, damit niemand mehr hineinliest, als drinsteht: der Nachbau
 * folgt DEMSELBEN Verstaendnis der Beschreibung wie der Treiber. Wenn
 * beide dieselbe Stelle falsch verstehen, passen sie trotzdem zusammen.
 * Diese Tests zeigen also, dass die Schichten des Treibers zueinander
 * passen - nicht, dass sie zum echten Baustein passen. Steht in
 * docs/se050.md unter "Was geprueft ist und was nicht". */
#ifndef SE050_SIM_H
#define SE050_SIM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "fake_bus.h"
#include "se050_scp03.h"

/* Zaehler und RSA-Schluessel, die der Nachbau wirklich fuehrt.
 *
 * Warum ueberhaupt Zustand, wo der Nachbau sonst nur Antworten abspult:
 * ein monotoner Zaehler IST Zustand. Ein Test, der nur eine festgelegte
 * Antwort zurueckbekommt, prueft nicht, dass nichts zurueckgeht - und
 * genau das ist die Leistung des Zaehlers. Der Zustand liegt im Nachbau,
 * also auf der Chipseite, und ueberlebt osk_se050_close(): damit laesst
 * sich ein Neustart der Firmware nachstellen. */
#define SIM_MAX_COUNTERS 4u
#define SIM_MAX_RSA 2u

typedef struct {
	uint32_t id;
	uint8_t len; /* Laenge in Byte, 1 bis 8 */
	uint64_t value;
	bool used;
} sim_counter_t;

typedef struct {
	uint32_t id;
	uint16_t bits;
	bool crt; /* mit P2_DEFAULT angelegt, nicht mit P2_RAW */
	bool used;
} sim_rsa_t;

typedef struct {
	/* Feste Schluessel - dieselben, die der Testhaken dem Treiber gibt. */
	osk_scp03_static_keys_t keys;

	/* Sitzungszustand der Gegenseite. Der Nachbau haelt eine Sitzung -
	 * mehr brauchen die Tests nicht, und der echte Baustein haelt zwei. */
	osk_scp03_t scp;
	bool applet_selected;
	bool channel;

	/* Kennung der offenen Sitzung und das Anmeldeobjekt, gegen das sie
	 * aufgebaut wurde. Letzteres pruefen die Tests. */
	uint8_t session_id[8];
	bool session_created;
	uint32_t session_obj;

	/* Schluessel, die der Nachbau fuer das naechste CreateSession
	 * erwartet. Vorgabe sind die festen Schluessel; der PIN-Test setzt
	 * hier den aus der PIN abgeleiteten Satz ein. */
	osk_scp03_static_keys_t next_auth_keys;

	/* Zufallszahl, die der Nachbau liefert. Fest, damit ein Fehlerbild
	 * wiederholbar ist. */
	uint8_t card_challenge[8];

	/* Zaehler fuer den Antwort-Startwert: der Wert, mit dem der gerade
	 * beantwortete Befehl verschluesselt war. */
	uint8_t cmd_counter[16];

	/* Merkmalbild, das der Nachbau beim Applet-Auswaehlen meldet.
	 * Steuerbar, damit sich ein Teil im FIPS-Modus nachstellen laesst. */
	uint16_t applet_config;

	/* Mitschrift fuer die Tests. */
	uint8_t last_ins;
	uint8_t last_p1;
	uint8_t last_p2;
	size_t commands;

	/* Die Klartext-Nutzlast des letzten Befehls. */
	uint8_t last_payload[600];
	size_t last_payload_len;

	/* Was auf den naechsten Befehl geantwortet wird. */
	uint8_t next_body[600];
	size_t next_body_len;
	uint16_t next_sw;

	/* Verfahrensbyte (TAG_2) des letzten RSA-Befehls - daran prueft der
	 * Test, dass die Auffuellung nicht geraten wurde. */
	uint8_t last_rsa_algo;

	/* Hinweg-Kette: der Treiber zerlegt einen Befehl, der laenger ist
	 * als das angekuendigte IFSC, in mehrere I-Bloecke. Der Nachbau
	 * sammelt sie, sonst waere ein RSA-Chiffrat nicht zustellbar. */
	uint8_t asm_buf[1104];
	size_t asm_len;

	/* Objekte, die der Nachbau selbst fuehrt. Nur wenn objects_live
	 * gesetzt ist; sonst bleibt es beim abgespulten next_body, und die
	 * aelteren Tests merken von all dem nichts. */
	bool objects_live;
	sim_counter_t counters[SIM_MAX_COUNTERS];
	sim_rsa_t rsa[SIM_MAX_RSA];

	/* Die zwei Platform-SCP-Saetze, 11 und 12 - vom Nachbau wirklich
	 * gefuehrt, nicht nur mitgeschrieben. [0] ist Satz 11, [1] ist
	 * Satz 12.
	 *
	 * WARUM DAS SEIN MUSS: ein Nachbau, der PUT KEY nur mitschreibt und
	 * 0x9000 antwortet, prueft die EIGENE Seite des Vertrags. Ein
	 * Erstlauf waere dann gruen, auch wenn er den neuen Schluessel mit
	 * dem falschen Key-DEK verpackt - und am Geraet wuerde nichts
	 * getauscht. Dieser Nachbau packt deshalb AUS, was hereinkommt: mit
	 * dem Key-DEK des Satzes, der GERADE GILT, und prueft den
	 * Pruefwert gegen den Schluessel, der dabei herauskommt. Passt es
	 * nicht, lehnt er ab - so wie der Chip es tun muesste.
	 *
	 * Danach gilt der neue Satz auch fuer den Sitzungsaufbau. Ein
	 * zweiter Anlauf mit dem Werkssatz scheitert damit AM NACHBAU und
	 * nicht erst am Zettel des Erstlaufs. */
	bool scp_sets_live;
	osk_scp03_static_keys_t scp_set[2];
	bool scp_replaced[2];
	/* SetPlatformSCPRequest mit SCP_REQUIRED ist angekommen. */
	bool scp_required;
	/* Wie oft PUT KEY kam und wie oft der Nachbau abgelehnt hat. Der
	 * Test zaehlt mit: "genau einmal je Satz" ist eine Zahl, kein
	 * Gefuehl. */
	size_t put_key_calls;
	size_t put_key_rejected;
	/* Ein Baustein, der 0x9000 antwortet und NICHTS tauscht. Genau die
	 * Falle vom 29.10.2026, nur umgedreht: hier luegt die Gegenseite,
	 * und der Erstlauf muss es merken, statt zufrieden zu sein. */
	bool put_key_lies;
} se050_sim_t;

/* Nachbau an den Testbus haengen. */
void sim_attach(se050_sim_t *sim, fake_bus_t *fb);

/* Antwort fuer den naechsten Befehl festlegen. body darf NULL sein. */
void sim_set_reply(se050_sim_t *sim, const uint8_t *body, size_t len, uint16_t sw);

/* Die Schluessel festlegen, die der Nachbau beim naechsten Sitzungsaufbau
 * als richtig ansieht - so laesst sich eine falsche PIN pruefen. */
void sim_set_auth_keys(se050_sim_t *sim, const osk_scp03_static_keys_t *keys);

/* Nach einem osk_se050_close() wieder anhaengen, ohne den Zustand des
 * Nachbaus zu loeschen - so laesst sich ein Neustart nachstellen. */
void sim_reattach(se050_sim_t *sim, fake_bus_t *fb);

/* Den Nachbau Objekte wirklich fuehren lassen: Zaehler zaehlen, RSA-
 * Schluessel liegen vor. Ein Lesen auf eine unbekannte Kennung wird dann
 * abgelehnt statt beantwortet. */
void sim_objects_live(se050_sim_t *sim, bool on);

/* Den Nachbau die zwei Platform-SCP-Saetze wirklich fuehren lassen.
 * `main_set` ist Satz 11, `rescue` Satz 12; im Erstlauf-Test sind das die
 * Werkssaetze. Danach wird PUT KEY ausgepackt und geprueft statt
 * mitgeschrieben, und der Sitzungsaufbau laeuft gegen Satz 11.
 *
 * ANNAHME DIESES NACHBAUS, am echten Chip offen: dass die Sitzung auf
 * Satz 11 laeuft und PUT KEY ueber DIESEN Kanal angenommen wird. Siehe
 * docs/erstlauf.md, Abschnitt "Was am echten Chip offen ist". */
void sim_scp_sets_live(se050_sim_t *sim, const osk_scp03_static_keys_t *main_set,
		       const osk_scp03_static_keys_t *rescue);

/* Die festen Schluessel, die Nachbau und Testhaken gemeinsam benutzen. */
const osk_scp03_static_keys_t *sim_keys(void);

#endif
