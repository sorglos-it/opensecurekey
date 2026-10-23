/* Anzeige-Attrappe - siehe fake_ui.h. */

#include <string.h>
#include "fake_ui.h"

static bool g_yes = true;
static size_t g_asks;
static osk_ask_t g_kind;
static char g_subject[128];
static char g_detail[128];
static bool g_had_detail;

void fake_ui_reset(void)
{
	g_yes = true;
	g_asks = 0;
	g_kind = OSK_ASK_LOGIN;
	memset(g_subject, 0, sizeof(g_subject));
	memset(g_detail, 0, sizeof(g_detail));
	g_had_detail = false;
}

void fake_ui_answer(bool yes)
{
	g_yes = yes;
}

size_t fake_ui_asks(void)
{
	return g_asks;
}

osk_ask_t fake_ui_last_kind(void)
{
	return g_kind;
}

const char *fake_ui_last_subject(void)
{
	return g_subject;
}

const char *fake_ui_last_detail(void)
{
	return g_had_detail ? g_detail : NULL;
}

static void copy(char *dst, size_t cap, const char *src)
{
	memset(dst, 0, cap);
	if (src != NULL) {
		size_t n = strlen(src);

		if (n > cap - 1u) {
			n = cap - 1u;
		}
		memcpy(dst, src, n);
	}
}

osk_result_t osk_ui_confirm(const osk_ask_req_t *req)
{
	if (req == NULL) {
		return OSK_ERR_ARG;
	}
	g_asks++;
	g_kind = req->kind;
	copy(g_subject, sizeof(g_subject), req->subject);
	copy(g_detail, sizeof(g_detail), req->detail);
	g_had_detail = req->detail != NULL;
	return g_yes ? OSK_OK : OSK_ERR_DENIED;
}

osk_result_t osk_ui_verify_user(bool *verified_out)
{
	if (verified_out == NULL) {
		return OSK_ERR_ARG;
	}
	*verified_out = g_yes;
	return g_yes ? OSK_OK : OSK_ERR_DENIED;
}

void osk_ui_status(const char *text_key)
{
	(void)text_key;
}
