#include <string.h>
#include "fake_bus.h"
#include "se050_t1.h"
#include "se050_t1_wire.h"

static fake_bus_t *self_of(osk_se050_bus_t *bus)
{
	return (fake_bus_t *)bus->ctx;
}

static int fb_write(osk_se050_bus_t *bus, const uint8_t *data, size_t len)
{
	fake_bus_t *fb = self_of(bus);

	fb->writes++;
	if (len == 0) {
		/* Das darf der Treiber nie tun: ein Rahmen ohne Datenbyte
		 * laesst den echten SE050 haengen (Erratasheet I2C.2).
		 * Wir zaehlen es mit, statt es zu verzeihen. */
		fb->empty_writes++;
		return OSK_BUS_ARG;
	}

	if (fb->responder != NULL) {
		fb->resp_len = fb->responder(fb->responder_ctx, data, len,
					     fb->resp_buf, sizeof(fb->resp_buf));
		fb->read_pos = 0;
		fb->armed = (fb->resp_len > 0);
		return 0;
	}

	/* Ein neues Schreiben beendet den vorigen Schritt, auch wenn dessen
	 * Antwort nicht zu Ende gelesen wurde. Genau so verhaelt sich ein
	 * Bus: der Wirt hat die Antwort eben liegen lassen. Ohne das waere
	 * ein abgebrochener Schritt eine Endlosschleife, und der Fall "nach
	 * einem Bruch wird erst zurueckgesetzt" liesse sich nicht pruefen. */
	if (fb->armed) {
		fb->armed = false;
		fb->at_step++;
	}

	if (fb->at_step >= fb->n_steps) {
		fb->mismatch = true;
		fb->mismatch_step = fb->at_step;
		return OSK_BUS_IO;
	}

	fb_step_t *st = &fb->steps[fb->at_step];

	if (st->mode == FB_NO_CHIP) {
		/* Kein Baustein auf dem Bus: jedes Schreiben wird abgelehnt.
		 * Der Schritt bleibt stehen, damit der Treiber beliebig oft
		 * dagegen laufen kann. */
		return OSK_BUS_IO;
	}

	if (st->want_len > 0) {
		if (len != st->want_len ||
		    memcmp(data, st->want, len) != 0) {
			fb->mismatch = true;
			fb->mismatch_step = fb->at_step;
			return OSK_BUS_IO;
		}
	}

	/* Schreiben angenommen - jetzt darf die Antwort gelesen werden. */
	fb->armed = true;
	fb->read_pos = 0;
	return 0;
}

static int fb_read(osk_se050_bus_t *bus, uint8_t *data, size_t len)
{
	fake_bus_t *fb = self_of(bus);

	if (!fb->armed) {
		return OSK_BUS_AGAIN;
	}

	if (fb->responder != NULL) {
		if (fb->read_pos >= fb->resp_len) {
			fb->armed = false;
			return OSK_BUS_AGAIN;
		}
		size_t n = fb->resp_len - fb->read_pos;
		if (n > len) {
			n = len;
		}
		memcpy(data, &fb->resp_buf[fb->read_pos], n);
		fb->read_pos += n;
		if (fb->read_pos >= fb->resp_len) {
			fb->armed = false;
		}
		return (int)n;
	}

	if (fb->at_step >= fb->n_steps) {
		return OSK_BUS_AGAIN;
	}
	fb_step_t *st = &fb->steps[fb->at_step];

	if (st->mode == FB_SILENT) {
		return OSK_BUS_AGAIN;
	}

	size_t avail = st->give_len;
	if (st->mode == FB_TRUNCATE && st->cut < avail) {
		avail = st->cut;
	}

	if (fb->read_pos >= avail) {
		/* Antwort aufgebraucht. Beim Abbruch bleibt es dauerhaft
		 * still - genau das soll der Treiber als Zeitlimit merken. */
		if (st->mode == FB_TRUNCATE) {
			return OSK_BUS_AGAIN;
		}
		/* Sonst: Schritt fertig, der naechste ist dran. */
		fb->armed = false;
		fb->at_step++;
		return OSK_BUS_AGAIN;
	}

	size_t n = avail - fb->read_pos;
	if (n > len) {
		n = len;
	}
	memcpy(data, &st->give[fb->read_pos], n);
	fb->read_pos += n;

	if (fb->read_pos >= avail && st->mode != FB_TRUNCATE) {
		fb->armed = false;
		fb->at_step++;
	}
	return (int)n;
}

static void fb_wait(osk_se050_bus_t *bus, uint32_t ms)
{
	fake_bus_t *fb = self_of(bus);

	fb->waits++;
	/* Nicht wirklich warten: die Uhr wird vorgestellt. So laufen die
	 * Zeitlimit-Tests in Millisekunden statt in Sekunden. */
	fb->clock_ms += (ms == 0u) ? 1u : ms;
}

static uint32_t fb_now(osk_se050_bus_t *bus)
{
	return self_of(bus)->clock_ms;
}

void fb_init(fake_bus_t *fb)
{
	memset(fb, 0, sizeof(*fb));
	fb->bus.write = fb_write;
	fb->bus.read = fb_read;
	fb->bus.wait_ms = fb_wait;
	fb->bus.now_ms = fb_now;
	fb->bus.power_cycle = NULL;
	fb->bus.ctx = fb;
	/* Nicht bei 0 anfangen: so faellt auf, wenn irgendwo mit einem
	 * festen Nullpunkt gerechnet wird statt mit Differenzen. */
	fb->clock_ms = 0xFFFFFF00u;
}

void fb_set_responder(fake_bus_t *fb, fb_responder_t fn, void *ctx)
{
	fb->responder = fn;
	fb->responder_ctx = ctx;
}

void fb_add(fake_bus_t *fb, fb_mode_t mode,
	    const uint8_t *want, size_t want_len,
	    const uint8_t *give, size_t give_len)
{
	if (fb->n_steps >= FB_MAX_STEPS) {
		return;
	}
	fb_step_t *st = &fb->steps[fb->n_steps++];
	memset(st, 0, sizeof(*st));
	st->mode = mode;

	if (want != NULL && want_len > 0 && want_len <= FB_MAX_FRAME) {
		memcpy(st->want, want, want_len);
		st->want_len = want_len;
	}
	if (give != NULL && give_len > 0 && give_len <= FB_MAX_FRAME) {
		memcpy(st->give, give, give_len);
		st->give_len = give_len;
	}
	st->cut = st->give_len;
}

void fb_add_frame(fake_bus_t *fb, fb_mode_t mode, uint8_t nad, uint8_t pcb,
		  const uint8_t *inf, uint8_t inf_len)
{
	uint8_t frame[FB_MAX_FRAME];
	const size_t n = osk_se050_t1_build(frame, sizeof(frame), nad, pcb,
					    inf, inf_len);
	if (n == 0) {
		return;
	}

	if (mode == FB_BAD_CRC) {
		/* Ein Bit im Epilog umdrehen. Die Laenge bleibt richtig,
		 * nur die Pruefsumme nicht - genau der Fall, den der
		 * Treiber mit einer Wiederholung beantworten soll. */
		frame[n - 1u] ^= 0x01u;
		fb_add(fb, FB_OK, NULL, 0, frame, n);
		return;
	}
	fb_add(fb, mode, NULL, 0, frame, n);
}

void fb_set_cut(fake_bus_t *fb, size_t cut)
{
	if (fb->n_steps == 0) {
		return;
	}
	fb->steps[fb->n_steps - 1u].cut = cut;
}

void fb_add_open_reply(fake_bus_t *fb)
{
	/* Antwort auf den S-Block "Schnittstelle zuruecksetzen": derselbe
	 * Code mit gesetztem Antwortbit, dazu ein kurzer Kennblock. Was im
	 * Kennblock steht, wertet der Treiber nicht aus. */
	static const uint8_t atr[] = { 0x01, 0x02, 0x03, 0x04 };
	const uint8_t pcb = SE050_PCB_TYPE_S | SE050_PCB_S_RESPONSE |
			    SE050_S_IF_SOFT_RESET;
	fb_add_frame(fb, FB_OK, SE050_NAD_TO_HOST, pcb, atr, sizeof(atr));
}

void fb_add_close_reply(fake_bus_t *fb)
{
	const uint8_t pcb = SE050_PCB_TYPE_S | SE050_PCB_S_RESPONSE |
			    SE050_S_END_APDU;
	fb_add_frame(fb, FB_OK, SE050_NAD_TO_HOST, pcb, NULL, 0);
}

bool fb_clean(const fake_bus_t *fb)
{
	if (fb->mismatch || fb->empty_writes > 0u) {
		return false;
	}
	/* Mit Antwortgeber gibt es keine Schrittliste, die aufgehen muesste. */
	if (fb->responder != NULL) {
		return true;
	}
	return fb->at_step == fb->n_steps;
}
