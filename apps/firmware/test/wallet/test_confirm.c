/* Die Regel, um die es beim Display geht: was nicht lesbar angezeigt
 * werden kann, wird nicht unterschrieben - ausser der Nutzer hat die
 * Blindunterschrift ausdruecklich erlaubt. Und: Betraege ohne
 * Fliesskommazahl. */

#include "test_util.h"
#include "wallet_confirm.h"
#include "mocks.h"

static void amount(const char *what, uint64_t value, unsigned decimals, const char *unit,
		   const char *want)
{
	char text[OSK_WALLET_AMOUNT_MAX];

	T_EQ(what, osk_wallet_format_amount(value, decimals, unit, text, sizeof(text)), OSK_OK);
	if (strcmp(text, want) != 0)
		printf("FEHLER %s\n  bekam    %s\n  erwartet %s\n", what, text, want);
	T_TRUE(what, strcmp(text, want) == 0);
}

static void test_amounts(void)
{
	amount("ein ganzer Bitcoin", 100000000ULL, 8, "BTC", "1.0 BTC");
	amount("ein Satoshi", 1, 8, "BTC", "0.00000001 BTC");
	amount("null", 0, 8, "BTC", "0.0 BTC");
	amount("sechzigtausend Satoshi", 60000, 8, "BTC", "0.0006 BTC");
	amount("21 Millionen", 2100000000000000ULL, 8, "BTC", "21000000.0 BTC");
	amount("ohne Einheit", 123456789ULL, 8, NULL, "1.23456789");
	amount("ohne Nachkommastellen", 42, 0, "Stueck", "42 Stueck");
	amount("Nullen hinten fallen weg", 123400000ULL, 8, "BTC", "1.234 BTC");
	amount("fuehrende Nullen bleiben", 1000ULL, 8, "BTC", "0.00001 BTC");
}

static void test_amount256(void)
{
	uint8_t value[32];
	char text[OSK_WALLET_AMOUNT_MAX];

	/* 1 Ether = 10^18 Wei - das passt nicht in 64 Bit, wenn man
	 * Vielfache rechnet, und darum gibt es diesen Weg. */
	T_EQ("1 Ether gelesen",
	     t_unhex("0000000000000000000000000000000000000000000000000de0b6b3a7640000", value,
		     sizeof(value)),
	     32);
	T_EQ("gerechnet", osk_wallet_format_amount256(value, 18, "ETH", text, sizeof(text)),
	     OSK_OK);
	T_TRUE("1.0 ETH", strcmp(text, "1.0 ETH") == 0);
	if (strcmp(text, "1.0 ETH") != 0)
		printf("  bekam %s\n", text);

	/* 1 Wei. */
	memset(value, 0, sizeof(value));
	value[31] = 1;
	T_EQ("gerechnet", osk_wallet_format_amount256(value, 18, "ETH", text, sizeof(text)),
	     OSK_OK);
	T_TRUE("ein Wei", strcmp(text, "0.000000000000000001 ETH") == 0);
	if (strcmp(text, "0.000000000000000001 ETH") != 0)
		printf("  bekam %s\n", text);

	/* Null. */
	memset(value, 0, sizeof(value));
	T_EQ("gerechnet", osk_wallet_format_amount256(value, 18, "ETH", text, sizeof(text)),
	     OSK_OK);
	T_TRUE("null", strcmp(text, "0.0 ETH") == 0);

	/* Eine absurd grosse Zahl passt nicht in die Zeile und muss als
	 * Fehler kommen, nicht abgeschnitten. */
	memset(value, 0xff, sizeof(value));
	T_EQ("zu lang", osk_wallet_format_amount256(value, 18, "ETH", text, sizeof(text)),
	     OSK_ERR_ARG);
}

static void test_shorten(void)
{
	char out[32];

	T_EQ("passt", osk_wallet_shorten("bc1qabc", out, sizeof(out)), OSK_OK);
	T_TRUE("unveraendert", strcmp(out, "bc1qabc") == 0);
	T_EQ("gekuerzt", osk_wallet_shorten("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4", out, 20),
	     OSK_OK);
	/* Anfang und Ende bleiben stehen - das ist, was ein Mensch
	 * vergleicht. */
	T_TRUE("Anfang bleibt", strncmp(out, "bc1qw508", 8) == 0);
	T_TRUE("Punkte in der Mitte", strstr(out, "...") != NULL);
	T_TRUE("Ende bleibt", strstr(out, "f3t4") != NULL);
	T_TRUE("passt in 20", strlen(out) < 20u);
	T_EQ("zu wenig Platz heisst nein", osk_wallet_shorten("bc1qlang", out, 5), OSK_ERR_ARG);
}

static void test_rule(void)
{
	osk_wallet_view_t view;

	/* Nichts lesbar und keine Extrafreigabe: es wird nicht gefragt und
	 * nicht unterschrieben. */
	mock_ui_reset();
	osk_wallet_view_init(&view);
	T_EQ("nicht lesbar heisst nein", osk_wallet_confirm(&view, "test", false), OSK_ERR_DENIED);
	T_EQ("gar nicht gefragt", mock_ui_calls(), 0);
	T_TRUE("Grund gemeldet", strcmp(mock_ui_status_key(), "wallet.nichts-lesbar") == 0);

	/* Mit Extrafreigabe wird gefragt - aber mit einer anderen Frage. */
	mock_ui_reset();
	mock_ui_answer(OSK_OK);
	T_EQ("Blindunterschrift", osk_wallet_confirm(&view, "test", true), OSK_OK);
	T_EQ("einmal gefragt", mock_ui_calls(), 1);
	T_TRUE("deutlich andere Frage",
	       strcmp(mock_ui_subject(), "wallet.blind-unterschrift") == 0);

	/* Fehlt nur die Gebuehr, gilt dasselbe. */
	mock_ui_reset();
	osk_wallet_view_init(&view);
	memcpy(view.recipient, "bc1qabc", 8);
	memcpy(view.amount, "1.0 BTC", 8);
	view.readable = true;
	T_EQ("ohne Gebuehr kein Ja", osk_wallet_confirm(&view, "test", false), OSK_ERR_DENIED);
	T_EQ("nicht gefragt", mock_ui_calls(), 0);

	/* Vollstaendig: es wird gefragt, und zwar mit allem drin. */
	mock_ui_reset();
	mock_ui_answer(OSK_OK);
	memcpy(view.fee, "0.0001 BTC", 11);
	T_EQ("vollstaendig", osk_wallet_confirm(&view, "Electrum", false), OSK_OK);
	T_EQ("einmal gefragt", mock_ui_calls(), 1);
	T_EQ("als Unterschrift", mock_ui_kind(), OSK_ASK_SIGN);
	T_TRUE("Empfaenger", strcmp(mock_ui_subject(), "bc1qabc") == 0);
	T_TRUE("Betrag und Gebuehr", strcmp(mock_ui_detail(), "1.0 BTC + 0.0001 BTC") == 0);
	if (strcmp(mock_ui_detail(), "1.0 BTC + 0.0001 BTC") != 0)
		printf("  Zeile: %s\n", mock_ui_detail());

	/* Sagt der Nutzer nein, kommt das durch. */
	mock_ui_reset();
	mock_ui_answer(OSK_ERR_DENIED);
	T_EQ("Nutzer sagt nein", osk_wallet_confirm(&view, "test", false), OSK_ERR_DENIED);

	/* Mehrere Empfaenger stehen als Zahl dabei. */
	mock_ui_reset();
	mock_ui_answer(OSK_OK);
	view.more_recipients = 3;
	T_EQ("mehrere Empfaenger", osk_wallet_confirm(&view, "test", false), OSK_OK);
	T_TRUE("Zahl steht dabei", strstr(mock_ui_detail(), "+3") != NULL);
	if (strstr(mock_ui_detail(), "+3") == NULL)
		printf("  Zeile: %s\n", mock_ui_detail());
}

int main(void)
{
	test_amounts();
	test_amount256();
	test_shorten();
	test_rule();
	return t_report("confirm");
}
