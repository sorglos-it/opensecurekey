/* Siehe usb_kbd.h. */
#include <string.h>

#include "usb_kbd.h"
#include "osk_mem.h"

/* Die Werkseinstellung der Abbildung [7], in der Reihenfolge aus
 * OSK_KBD_SCAN_ORDER. Gegengerechnet gegen die HID-Tastentabelle:
 * c=0x06, b=0x05, d=0x07, Grossbuchstaben dieselben Codes mit 0x80,
 * 0=0x27, 1=0x1e, !=0x9e (Umschalter und 1), Tabulator 0x2b,
 * Eingabetaste 0x28. */
static const uint8_t default_map[OSK_KBD_SCAN_MAP_LEN] = {
    /* cbdefghijklnrtuv */
    0x06, 0x05, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x11, 0x15, 0x17, 0x18,
    0x19,
    /* CBDEFGHIJKLNRTUV */
    0x86, 0x85, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x91, 0x95, 0x97, 0x98,
    0x99,
    /* 0123456789 */
    0x27, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
    /* ! Tabulator Eingabe */
    0x9e, 0x2b, 0x28};

static struct {
	osk_kbd_send_fn send;
	void *ctx;
	uint8_t map[OSK_KBD_SCAN_MAP_LEN];

	/* Die Tastencodes des laufenden Auftrags. Ein Geheimnis, solange
	 * sie liegen - darum wird nach dem letzten Zeichen geloescht. */
	uint8_t queue[OSK_KBD_QUEUE_MAX];
	size_t len;
	size_t pos;
	bool released; /* das Loslassen zum Zeichen pos ist schon heraus */
	bool ready;
} g;

osk_result_t osk_kbd_init(osk_kbd_send_fn send, void *ctx)
{
	if (send == NULL)
		return OSK_ERR_ARG;

	osk_wipe(&g, sizeof(g));
	g.send = send;
	g.ctx = ctx;
	memcpy(g.map, default_map, sizeof(g.map));
	g.ready = true;
	return OSK_OK;
}

osk_result_t osk_kbd_set_scan_map(const uint8_t map[OSK_KBD_SCAN_MAP_LEN])
{
	if (!g.ready)
		return OSK_ERR_INTERNAL;
	if (map == NULL)
		return OSK_ERR_ARG;
	/* Nicht mitten im Tippen umstellen: die Haelfte eines Passworts
	 * nach einer anderen Belegung ist Unsinn. */
	if (g.len != 0u)
		return OSK_ERR_EXISTS;

	memcpy(g.map, map, OSK_KBD_SCAN_MAP_LEN);
	return OSK_OK;
}

void osk_kbd_reset_scan_map(void)
{
	memcpy(g.map, default_map, sizeof(g.map));
}

const uint8_t *osk_kbd_scan_map(void)
{
	return g.map;
}

bool osk_kbd_scan_code(char c, uint8_t *code)
{
	static const char order[] = OSK_KBD_SCAN_ORDER;

	if (code == NULL)
		return false;

	/* sizeof zaehlt die abschliessende Null mit, darum minus eins.
	 * Die Zahl 45 steht nicht hier: sie steht in der Reihenfolge und
	 * wird daraus gelesen, sonst koennten die beiden auseinanderlaufen. */
	for (size_t i = 0; i + 1u < sizeof(order); i++) {
		if (order[i] == c) {
			*code = g.map[i];
			return true;
		}
	}
	return false;
}

osk_result_t osk_kbd_type_codes(const uint8_t *codes, size_t len)
{
	if (!g.ready)
		return OSK_ERR_INTERNAL;
	if (len == 0u || codes == NULL)
		return OSK_ERR_ARG;
	if (g.len != 0u)
		return OSK_ERR_EXISTS;
	/* Grenze vor dem Kopieren. */
	if (len > OSK_KBD_QUEUE_MAX)
		return OSK_ERR_FULL;

	memcpy(g.queue, codes, len);
	g.len = len;
	g.pos = 0u;
	g.released = false;
	return OSK_OK;
}

osk_result_t osk_kbd_type(const char *text, size_t len)
{
	uint8_t codes[OSK_KBD_QUEUE_MAX];
	osk_result_t r;

	if (!g.ready)
		return OSK_ERR_INTERNAL;
	if (len == 0u || text == NULL)
		return OSK_ERR_ARG;
	if (g.len != 0u)
		return OSK_ERR_EXISTS;
	if (len > OSK_KBD_QUEUE_MAX)
		return OSK_ERR_FULL;

	/* Erst ganz uebersetzen, dann einstellen. Scheitert ein Zeichen,
	 * ist nichts getippt und nichts eingestellt. */
	for (size_t i = 0; i < len; i++) {
		if (!osk_kbd_scan_code(text[i], &codes[i])) {
			osk_wipe(codes, sizeof(codes));
			return OSK_ERR_UNSUPPORTED;
		}
	}

	r = osk_kbd_type_codes(codes, len);
	osk_wipe(codes, sizeof(codes));
	return r;
}

bool osk_kbd_pump(void)
{
	uint8_t rpt[OSK_KBD_RPT_LEN];

	if (!g.ready || g.len == 0u)
		return false;

	memset(rpt, 0, sizeof(rpt));

	if (!g.released) {
		/* Drucken. */
		const uint8_t code = g.queue[g.pos];

		if ((code & OSK_KBD_SHIFT) != 0u)
			rpt[0] = (uint8_t)OSK_KBD_MOD_LSHIFT;
		rpt[2] = (uint8_t)(code & 0x7fu);
		g.released = true;
	} else {
		/* Loslassen. Es muss zwischen zwei Zeichen stehen, sonst
		 * kommt dasselbe Zeichen zweimal hintereinander beim
		 * Rechner als EIN Druck an. */
		g.released = false;
		g.pos++;
		if (g.pos >= g.len) {
			/* Fertig. Die Codes sind ein Geheimnis - weg damit. */
			osk_wipe(g.queue, sizeof(g.queue));
			g.len = 0u;
			g.pos = 0u;
			g.send(rpt, g.ctx);
			return false;
		}
	}

	g.send(rpt, g.ctx);
	return true;
}

void osk_kbd_cancel(void)
{
	osk_wipe(g.queue, sizeof(g.queue));
	g.len = 0u;
	g.pos = 0u;
	g.released = false;
}

bool osk_kbd_busy(void)
{
	return g.len != 0u;
}
