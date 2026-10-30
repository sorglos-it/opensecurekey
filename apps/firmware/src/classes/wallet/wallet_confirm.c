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

osk_result_t osk_wallet_set_address(osk_wallet_view_t *view, const char *address)
{
	size_t len;

	if (view == NULL || address == NULL)
		return OSK_ERR_ARG;
	len = strlen(address);
	if (len == 0u)
		return OSK_ERR_FORMAT;
	/* Hier wird nicht gekuerzt. Passt die Adresse nicht, dann kann das
	 * Geraet sie nicht anzeigen, und dann unterschreibt es nicht. */
	if (len >= (size_t)OSK_WALLET_ADDR_MAX || len >= sizeof(view->recipient))
		return OSK_ERR_FORMAT;
	memcpy(view->recipient, address, len + 1u);
	view->subject_kind = OSK_WALLET_SUBJECT_ADDRESS;
	return OSK_OK;
}

osk_result_t osk_wallet_group_address(const char *address, char *out, size_t cap)
{
	size_t len, written = 0;

	if (address == NULL || out == NULL || cap == 0u)
		return OSK_ERR_ARG;
	len = strlen(address);
	if (len == 0u || len >= (size_t)OSK_WALLET_ADDR_MAX)
		return OSK_ERR_FORMAT;

	for (size_t i = 0; i < len; i++) {
		/* Vor jeder weiteren Vierergruppe ein Leerzeichen. */
		if (i > 0u && (i % 4u) == 0u) {
			if (written + 1u >= cap)
				return OSK_ERR_FORMAT;
			out[written++] = ' ';
		}
		if (written + 1u >= cap)
			return OSK_ERR_FORMAT;
		out[written++] = address[i];
	}
	out[written] = '\0';
	return OSK_OK;
}

osk_result_t osk_wallet_fingerprint(const char *address, char *out, size_t cap)
{
	uint8_t hash[OSK_SHA256_LEN];
	uint64_t value = 0;
	size_t len;

	if (address == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (cap < (size_t)OSK_WALLET_FINGERPRINT_DIGITS + 1u)
		return OSK_ERR_ARG;
	len = strlen(address);
	if (len == 0u || len >= (size_t)OSK_WALLET_ADDR_MAX)
		return OSK_ERR_FORMAT;

	/* Ueber die GANZE Adresse. Genau das ist der Punkt: ein einzelnes
	 * vertauschtes Zeichen in der Mitte aendert die Zahl. */
	osk_sha256((const uint8_t *)address, len, hash);

	/* Fuenf Byte zu einer Zahl zusammenziehen - mehr als die acht
	 * Ziffern brauchen. */
	for (size_t i = 0; i < 5u; i++)
		value = (value << 8) | (uint64_t)hash[i];

	for (size_t i = (size_t)OSK_WALLET_FINGERPRINT_DIGITS; i > 0u; i--) {
		out[i - 1u] = (char)('0' + (char)(value % 10u));
		value /= 10u;
	}
	out[OSK_WALLET_FINGERPRINT_DIGITS] = '\0';
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

osk_result_t osk_wallet_confirm(const osk_wallet_view_t *view, const char *asked_by)
{
	osk_ask_req_t req;
	char shown[OSK_WALLET_ADDR_SHOWN_MAX + OSK_WALLET_FINGERPRINT_DIGITS + 16];
	char fingerprint[OSK_WALLET_FINGERPRINT_DIGITS + 1];
	char detail[OSK_WALLET_TEXT_MAX + 2u * OSK_WALLET_AMOUNT_MAX + 48];
	size_t written = 0;
	osk_result_t res;

	if (view == NULL)
		return OSK_ERR_ARG;

	if (!view->readable || view->recipient[0] == '\0' || view->amount[0] == '\0' ||
	    view->fee[0] == '\0') {
		/* Hier endet es, und es gibt keinen zweiten Weg. Das ist die
		 * Regel, nicht ein Notfall: was nicht lesbar angezeigt werden
		 * kann, wird nicht unterschrieben. */
		osk_ui_status(view->missing_key != NULL ? view->missing_key
							: "wallet.nichts-lesbar");
		return OSK_ERR_DENIED;
	}

	if (view->subject_kind == OSK_WALLET_SUBJECT_ADDRESS) {
		/* Die Adresse in Vierergruppen, dahinter die Pruefzahl ueber
		 * die ganze Adresse. Gekuerzt wird nichts; geht das nicht,
		 * wird nicht unterschrieben. */
		res = osk_wallet_group_address(view->recipient, shown, sizeof(shown));
		if (res != OSK_OK) {
			osk_ui_status("wallet.adresse-nicht-anzeigbar");
			return OSK_ERR_DENIED;
		}
		res = osk_wallet_fingerprint(view->recipient, fingerprint, sizeof(fingerprint));
		if (res != OSK_OK) {
			osk_ui_status("wallet.adresse-nicht-anzeigbar");
			return OSK_ERR_DENIED;
		}
		{
			size_t used = strlen(shown);

			append(shown, sizeof(shown), &used, " #");
			append(shown, sizeof(shown), &used, fingerprint);
		}
	} else {
		/* Ein Text, den ein Mensch liest. Der wird nicht gegliedert
		 * und bekommt keine Pruefzahl - er wird gelesen, nicht
		 * verglichen. Passt er nicht, wird nicht unterschrieben. */
		size_t len = strlen(view->recipient);

		if (len + 1u > sizeof(shown)) {
			osk_ui_status("wallet.nachricht-nicht-lesbar");
			return OSK_ERR_DENIED;
		}
		memcpy(shown, view->recipient, len + 1u);
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
	req.subject = shown;
	req.detail = detail;
	req.asked_by = asked_by;
	req.timeout_ms = CONFIRM_TIMEOUT_MS;
	return osk_ui_confirm(&req);
}
