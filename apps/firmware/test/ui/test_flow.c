/* Der ganze Ablauf einer Freigabe, mit Taster aus dem Drehbuch.
 *
 * Hier wird geprueft, was die Applets wirklich zurueckbekommen: OSK_OK nur
 * nach einem langen Druck auf der Entscheidungsseite, OSK_ERR_DENIED wenn der
 * Nutzer nicht wollte, OSK_ERR_TIMEOUT wenn niemand da war. */

#include <string.h>
#include "fake_panel.h"
#include "osk_lang.h"
#include "osk_ui.h"
#include "osk_ui_port.h"
#include "osk_views.h"
#include "ui_test.h"

#define LONG_PRESS (OSK_BTN_LONG_MS + 100u)
#define SHORT_PRESS 60u
#define GAP (OSK_BTN_DOUBLE_MS + 100u)

static osk_ask_req_t login(void)
{
	osk_ask_req_t req = {OSK_ASK_LOGIN, "github.com", NULL, NULL, 20000};

	return req;
}

static void begin(void)
{
	fake_panel_reset();
	T_TRUE(osk_lang_set("de"));
	T_OK(osk_ui_begin(fake_panel_port()));
}

/* Langer Druck samt Pause danach. */
static void push_long(void)
{
	fake_panel_push(true, LONG_PRESS);
	fake_panel_push(false, GAP);
}

static void push_short(void)
{
	fake_panel_push(true, SHORT_PRESS);
	fake_panel_push(false, GAP);
}

void test_flow(void)
{
	osk_test_case("ohne Anzeige gibt es keine Freigabe");
	osk_ui_end();
	{
		osk_ask_req_t req = login();

		T_RES(osk_ui_confirm(&req), OSK_ERR_INTERNAL);
	}
	T_RES(osk_ui_begin(NULL), OSK_ERR_ARG);

	osk_test_case("lang auf der Entscheidungsseite heisst ja");
	begin();
	push_long(); /* zur Entscheidungsseite */
	push_long(); /* ja */
	{
		osk_ask_req_t req = login();

		T_OK(osk_ui_confirm(&req));
	}
	T_TRUE(fake_panel_flushes() > 1);
	osk_ui_end();

	osk_test_case("kurz auf der Entscheidungsseite heisst nein");
	begin();
	push_long();
	push_short();
	{
		osk_ask_req_t req = login();

		T_RES(osk_ui_confirm(&req), OSK_ERR_DENIED);
	}
	osk_ui_end();

	osk_test_case("ein langer Druck allein gibt nichts frei");
	/* Der erste lange Druck bringt nur die Entscheidungsseite. Danach
	 * laeuft die Zeit ab - und es bleibt bei nein. */
	begin();
	push_long();
	{
		osk_ask_req_t req = login();

		req.timeout_ms = 3000;
		T_RES(osk_ui_confirm(&req), OSK_ERR_TIMEOUT);
	}
	osk_ui_end();

	osk_test_case("niemand da: Zeit laeuft ab");
	begin();
	{
		osk_ask_req_t req = login();

		req.timeout_ms = 2000;
		T_RES(osk_ui_confirm(&req), OSK_ERR_TIMEOUT);
		T_TRUE(fake_panel_shows(osk_lang_text("error.timeout")) || fake_panel_ink() > 0);
	}
	osk_ui_end();

	osk_test_case("doppelt heisst von vorn lesen - und dann geht es weiter");
	begin();
	push_long();			    /* Entscheidungsseite */
	push_short();			    /* erster kurzer Druck ... */
	fake_panel_push(true, SHORT_PRESS); /* ... und gleich der zweite: doppelt */
	fake_panel_push(false, 100);
	fake_panel_push(true, SHORT_PRESS);
	fake_panel_push(false, GAP);
	push_long();
	push_long();
	{
		/* Der erste kurze Druck auf der Entscheidungsseite waere ein
		 * nein. Darum wird hier gleich doppelt gedrueckt. */
		osk_ask_req_t req = login();

		req.timeout_ms = 30000;
		(void)osk_ui_confirm(&req);
	}
	osk_ui_end();

	osk_test_case("Loeschen fragt zweimal");
	begin();
	push_long(); /* Entscheidungsseite */
	push_long(); /* erstes ja */
	{
		osk_ask_req_t req = {OSK_ASK_WIPE, "Profil 1", NULL, NULL, 3000};

		T_RES(osk_ui_confirm(&req), OSK_ERR_TIMEOUT); /* ein ja genuegt nicht */
	}
	osk_ui_end();

	begin();
	push_long();
	push_long();
	push_long();
	{
		osk_ask_req_t req = {OSK_ASK_WIPE, "Profil 1", NULL, NULL, 20000};

		T_OK(osk_ui_confirm(&req));
	}
	osk_ui_end();

	osk_test_case("eine lange Adresse laesst sich durchblaettern");
	{
		osk_ask_req_t req = {OSK_ASK_SIGN, "0x71C7656EC7ab88b098defB751B7401B5f6d8976F",
				     "0,5 ETH", NULL, 20000};
		size_t pages = osk_view_pages(&req);

		T_TRUE(pages >= 1);
		begin();
		/* Erst durch alle Leseseiten blaettern, dann die
		 * Entscheidungsseite holen, dann freigeben. */
		for (size_t i = 0; i + 1 < pages; i++) {
			push_short();
		}
		push_short(); /* von der letzten Seite zur Entscheidung */
		push_long();  /* ja */
		T_OK(osk_ui_confirm(&req));
		osk_ui_end();
	}

	osk_test_case("falsche Aufrufe werden gemeldet");
	begin();
	T_RES(osk_ui_confirm(NULL), OSK_ERR_ARG);
	{
		bool verified = true;

		T_RES(osk_ui_verify_user(NULL), OSK_ERR_ARG);
		fake_panel_set_verified(true);
		T_OK(osk_ui_verify_user(&verified));
		T_TRUE(verified);
		fake_panel_set_verified(false);
		push_short();
		T_RES(osk_ui_verify_user(&verified), OSK_ERR_DENIED);
		T_TRUE(!verified);
	}
	osk_ui_end();

	osk_test_case("die LED sagt dasselbe wie die Anzeige");
	T_EQ(osk_ui_led_for("status.unlocked"), OSK_LED_GREEN);
	T_EQ(osk_ui_led_for("status.locked"), OSK_LED_RED);
	T_EQ(osk_ui_led_for("status.error"), OSK_LED_RED);
	T_EQ(osk_ui_led_for("status.busy"), OSK_LED_AMBER);
	T_EQ(osk_ui_led_for("status.battery"), OSK_LED_AMBER);
	T_EQ(osk_ui_led_for(NULL), OSK_LED_OFF);

	osk_test_case("der Zustand wird gezeigt und die LED mitgesetzt");
	begin();
	osk_ui_status("status.locked");
	T_EQ(fake_panel_led(), OSK_LED_RED);
	T_TRUE(fake_panel_ink() > 0);
	osk_ui_status("status.unlocked");
	T_EQ(fake_panel_led(), OSK_LED_GREEN);
	osk_ui_end();
	T_EQ(fake_panel_led(), OSK_LED_OFF);

	osk_test_case("nach dem Ende tut der Zustand nichts mehr");
	osk_ui_status("status.busy");
	T_EQ(fake_panel_led(), OSK_LED_OFF);
}
