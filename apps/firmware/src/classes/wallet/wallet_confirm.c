/* Umsetzung von wallet_confirm.h. */

#include <string.h>
#include "wallet_confirm.h"
#include "osk_ui.h"
#include "osk_crypto.h"

/* Wie lange der Nutzer Zeit hat. Lange genug, um eine Adresse wirklich zu
 * vergleichen - zu kurz waere eine Einladung zum Durchklicken. */
#define CONFIRM_TIMEOUT_MS 60000u

void osk_wallet_view_init(osk_wallet_view_t *view)
{
	if (view == NULL)
		return;
	memset(view, 0, sizeof(*view));
	view->missing_key = "wallet.nichts-lesbar";
}

/* Dezimalstellen einer Zahl, die als Bytefolge vorliegt. Teilt die Zahl
 * fortlaufend durch 10 und sammelt die Reste - niederwertige Stelle zuerst.
 * Arbeitet auf einer Kopie, veraendert nichts beim Aufrufer. */
static size_t digits_of(const uint8_t *value, size_t len, char *out, size_t cap)
{
	uint8_t buf[32];
	size_t count = 0, start = 0;

	if (len > sizeof(buf))
		return 0;
	memcpy(buf, value, len);
	while (start < len && buf[start] == 0)
		start++;
	if (start == len) {
		if (cap < 1u)
			return 0;
		out[0] = '0';
		return 1;
	}
	while (start < len) {
		uint32_t remainder = 0;
		size_t first_nonzero = len;

		for (size_t i = start; i < len; i++) {
			uint32_t cur = (remainder << 8) | buf[i];

			buf[i] = (uint8_t)(cur / 10u);
			remainder = cur % 10u;
			if (buf[i] != 0 && first_nonzero == len)
				first_nonzero = i;
		}
		if (count >= cap) {
			osk_wipe(buf, sizeof(buf));
			return 0;
		}
		out[count++] = (char)('0' + (char)remainder);
		start = first_nonzero;
	}
	osk_wipe(buf, sizeof(buf));
	return count;
}

static osk_result_t format_digits(const char *digits_low_first, size_t count, unsigned decimals,
				  const char *unit, char *out, size_t cap)
{
	char text[OSK_WALLET_AMOUNT_MAX];
	size_t written = 0, unit_len = (unit != NULL) ? strlen(unit) : 0u;
	size_t whole = (count > decimals) ? count - decimals : 0u;
	size_t drop = 0;

	if (decimals > 24u)
		return OSK_ERR_ARG;

	/* Nullen am ENDE der Nachkommastellen weglassen - das sind die
	 * niederwertigen, also die mit dem kleinsten Index. Eine Stelle
	 * bleibt immer stehen, damit "0." nicht nackt dasteht. */
	while (drop + 1u < decimals) {
		char digit = (drop < count) ? digits_low_first[drop] : '0';

		if (digit != '0')
			break;
		drop++;
	}

	if (whole == 0) {
		if (written + 1u >= sizeof(text))
			return OSK_ERR_ARG;
		text[written++] = '0';
	} else {
		for (size_t i = whole; i > 0; i--) {
			if (written + 1u >= sizeof(text))
				return OSK_ERR_ARG;
			text[written++] = digits_low_first[i - 1u + decimals];
		}
	}
	if (decimals > 0) {
		if (written + 1u >= sizeof(text))
			return OSK_ERR_ARG;
		text[written++] = '.';
		for (size_t i = decimals; i > drop; i--) {
			size_t index = i - 1u;

			if (written + 1u >= sizeof(text))
				return OSK_ERR_ARG;
			text[written++] = (index < count) ? digits_low_first[index] : '0';
		}
	}
	if (unit_len > 0) {
		if (written + 1u + unit_len >= sizeof(text))
			return OSK_ERR_ARG;
		text[written++] = ' ';
		memcpy(text + written, unit, unit_len);
		written += unit_len;
	}
	text[written] = '\0';
	if (written + 1u > cap)
		return OSK_ERR_ARG;
	memcpy(out, text, written + 1u);
	return OSK_OK;
}

osk_result_t osk_wallet_format_amount(uint64_t value, unsigned decimals, const char *unit,
				      char *out, size_t cap)
{
	uint8_t raw[8];
	char digits[32];
	size_t count;

	if (out == NULL)
		return OSK_ERR_ARG;
	for (unsigned i = 0; i < 8; i++)
		raw[i] = (uint8_t)((value >> (56u - 8u * i)) & 0xFFu);
	count = digits_of(raw, sizeof(raw), digits, sizeof(digits));
	if (count == 0)
		return OSK_ERR_INTERNAL;
	return format_digits(digits, count, decimals, unit, out, cap);
}

osk_result_t osk_wallet_format_amount256(const uint8_t value[32], unsigned decimals,
					 const char *unit, char *out, size_t cap)
{
	char digits[80];
	size_t count;

	if (value == NULL || out == NULL)
		return OSK_ERR_ARG;
	count = digits_of(value, 32, digits, sizeof(digits));
	if (count == 0)
		return OSK_ERR_INTERNAL;
	return format_digits(digits, count, decimals, unit, out, cap);
}

osk_result_t osk_wallet_shorten(const char *text, char *out, size_t cap)
{
	size_t len;

	if (text == NULL || out == NULL)
		return OSK_ERR_ARG;
	len = strlen(text);
	if (cap < 13u)
		return OSK_ERR_ARG; /* zu wenig Platz: lieber nichts zeigen */
	if (len + 1u <= cap) {
		memcpy(out, text, len + 1u);
		return OSK_OK;
	}
	{
		/* Platz fuer "..." abziehen, Rest halbieren - vorne eine
		 * Stelle mehr, denn der Anfang nennt die Art der Adresse. */
		size_t room = cap - 1u - 3u;
		size_t tail = room / 2u;
		size_t head = room - tail;

		memcpy(out, text, head);
		memcpy(out + head, "...", 3);
		memcpy(out + head + 3u, text + len - tail, tail);
		out[head + 3u + tail] = '\0';
	}
	return OSK_OK;
}

/* Text anhaengen, ohne je ueber das Ende zu schreiben. Passt es nicht
 * ganz, wird abgeschnitten - und das sieht der Nutzer, weil die Zeile dann
 * mittendrin endet. */
static void append(char *buf, size_t cap, size_t *used, const char *text)
{
	size_t len = strlen(text);

	if (*used + len + 1u > cap)
		len = (cap > *used + 1u) ? cap - *used - 1u : 0u;
	memcpy(buf + *used, text, len);
	*used += len;
	buf[*used] = '\0';
}

/* Kleine Zahl als Text, ohne printf. */
static void decimal(uint32_t value, char *out, size_t cap)
{
	char rev[12];
	size_t count = 0, written = 0;

	do {
		rev[count++] = (char)('0' + (char)(value % 10u));
		value /= 10u;
	} while (value > 0 && count < sizeof(rev));
	while (count > 0 && written + 1u < cap)
		out[written++] = rev[--count];
	out[written] = '\0';
}

osk_result_t osk_wallet_confirm(const osk_wallet_view_t *view, const char *asked_by,
				bool allow_blind)
{
	osk_ask_req_t req;
	char detail[OSK_WALLET_TEXT_MAX + 2u * OSK_WALLET_AMOUNT_MAX + 48];
	size_t written = 0;

	if (view == NULL)
		return OSK_ERR_ARG;

	if (!view->readable || view->recipient[0] == '\0' || view->amount[0] == '\0' ||
	    view->fee[0] == '\0') {
		if (!allow_blind) {
			/* Hier endet es. Das ist die Regel, nicht ein Notfall. */
			osk_ui_status(view->missing_key != NULL ? view->missing_key
								: "wallet.nichts-lesbar");
			return OSK_ERR_DENIED;
		}
		memset(&req, 0, sizeof(req));
		req.kind = OSK_ASK_SIGN;
		req.subject = "wallet.blind-unterschrift";
		req.detail = (view->missing_key != NULL) ? view->missing_key : NULL;
		req.asked_by = asked_by;
		req.timeout_ms = CONFIRM_TIMEOUT_MS;
		return osk_ui_confirm(&req);
	}

	/* Betrag, Gebuehr und alles Weitere in eine Zeile, in dieser
	 * Reihenfolge: zuerst was abgeht, dann was es kostet. Zusammengesetzt
	 * von Hand, damit keine Formatzeichenkette aus fremden Daten entsteht. */
	append(detail, sizeof(detail), &written, view->amount);
	append(detail, sizeof(detail), &written, " + ");
	append(detail, sizeof(detail), &written, view->fee);
	if (view->extra[0] != '\0') {
		append(detail, sizeof(detail), &written, " / ");
		append(detail, sizeof(detail), &written, view->extra);
	}
	if (view->more_recipients > 0) {
		char count[12];

		append(detail, sizeof(detail), &written, " +");
		decimal(view->more_recipients, count, sizeof(count));
		append(detail, sizeof(detail), &written, count);
	}
	detail[written] = '\0';

	memset(&req, 0, sizeof(req));
	req.kind = OSK_ASK_SIGN;
	req.subject = view->recipient;
	req.detail = detail;
	req.asked_by = asked_by;
	req.timeout_ms = CONFIRM_TIMEOUT_MS;
	return osk_ui_confirm(&req);
}
