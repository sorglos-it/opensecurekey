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

/* Die Adresse wird ganz angezeigt oder gar nicht - gekuerzt nie. */
static void test_address(void)
{
	osk_wallet_view_t view;
	char out[OSK_WALLET_ADDR_SHOWN_MAX];
	char lang[OSK_WALLET_ADDR_MAX + 8];

	osk_wallet_view_init(&view);

	/* Eine kurze Adresse kommt woertlich an. */
	T_EQ("kurz geht", osk_wallet_set_address(&view, "bc1qabc"), OSK_OK);
	T_TRUE("woertlich", strcmp(view.recipient, "bc1qabc") == 0);
	T_EQ("als Adresse gemerkt", (long long)view.subject_kind,
	     (long long)OSK_WALLET_SUBJECT_ADDRESS);

	/* Die laengste Bech32-Adresse nach BIP173 geht noch. */
	memset(lang, 'q', 90);
	lang[90] = 0;
	T_EQ("90 Zeichen gehen", osk_wallet_set_address(&view, lang), OSK_OK);

	/* Ein Zeichen mehr ist keine Adresse, die dieses Geraet anzeigen
	 * kann - und dann wird nicht unterschrieben. */
	memset(lang, 'q', 91);
	lang[91] = 0;
	T_EQ("91 Zeichen nicht", osk_wallet_set_address(&view, lang), OSK_ERR_FORMAT);
	T_EQ("leer auch nicht", osk_wallet_set_address(&view, ""), OSK_ERR_FORMAT);

	/* Gegliedert, aber inhaltlich unangetastet: alle Zeichen in der
	 * Reihenfolge, nur Leerzeichen dazwischen. */
	T_EQ("gegliedert", osk_wallet_group_address("bc1qw508d6qejxtdg4", out, sizeof(out)),
	     OSK_OK);
	T_TRUE("Vierergruppen", strcmp(out, "bc1q w508 d6qe jxtd g4") == 0);
	if (strcmp(out, "bc1q w508 d6qe jxtd g4") != 0)
		printf("  war: %s\n", out);

	/* Und vor allem: NICHTS wird durch Punkte ersetzt. */
	T_EQ("lange Adresse gegliedert",
	     osk_wallet_group_address("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4", out,
				      sizeof(out)),
	     OSK_OK);
	T_TRUE("keine Punkte", strstr(out, "...") == NULL);
	{
		/* Jedes Zeichen der Adresse steht noch da, in der Reihenfolge. */
		const char *src = "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4";
		size_t j = 0;

		for (size_t i = 0; out[i] != 0; i++) {
			if (out[i] != ' ') {
				T_TRUE("Zeichen stimmt", out[i] == src[j]);
				j++;
			}
		}
		T_EQ("alle Zeichen da", (long long)j, (long long)strlen(src));
	}

	/* Zu wenig Platz heisst Fehler - nicht abschneiden. */
	T_EQ("kein Platz, kein Bild",
	     osk_wallet_group_address("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4", out, 10),
	     OSK_ERR_FORMAT);
}

/* DER ANGRIFF: ein Angreifer sucht sich eine Adresse, deren Anfang UND
 * Ende passen. Genau das ist der Grund, warum nicht gekuerzt wird. Diese
 * Pruefung stellt den Angriff nach und zeigt, dass er auffaellt. */
static void test_address_angriff(void)
{
	/* Diese zwei stimmen am Anfang und am Ende ueberein - unter der
	 * alten Kuerzung waeren sie auf dem Schirm dieselbe Adresse
	 * gewesen. */
	const char *echt = "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4";
	const char *falsch = "bc1qw508XXXXXXXXXXXXXXXXXXXXXXXXXXXXXv8f3t4";
	char a[OSK_WALLET_ADDR_SHOWN_MAX], b[OSK_WALLET_ADDR_SHOWN_MAX];
	char fa[OSK_WALLET_FINGERPRINT_DIGITS + 1], fb[OSK_WALLET_FINGERPRINT_DIGITS + 1];

	/* Erstens: Anfang und Ende sind wirklich gleich. Ohne das prueft
	 * der Rest hier nichts. */
	T_TRUE("Anfang gleich", strncmp(echt, falsch, 7) == 0);
	T_TRUE("Ende gleich",
	       strcmp(echt + strlen(echt) - 5, falsch + strlen(falsch) - 5) == 0);

	/* Zweitens: so, wie es jetzt angezeigt wird, sind sie verschieden -
	 * weil die Mitte mit dasteht. */
	T_EQ("echt gegliedert", osk_wallet_group_address(echt, a, sizeof(a)), OSK_OK);
	T_EQ("falsch gegliedert", osk_wallet_group_address(falsch, b, sizeof(b)), OSK_OK);
	T_TRUE("auf dem Schirm verschieden", strcmp(a, b) != 0);

	/* Drittens: auch die Pruefzahl ist verschieden - der schnelle
	 * Vergleich faellt also ebenfalls nicht darauf herein. */
	T_EQ("Pruefzahl echt", osk_wallet_fingerprint(echt, fa, sizeof(fa)), OSK_OK);
	T_EQ("Pruefzahl falsch", osk_wallet_fingerprint(falsch, fb, sizeof(fb)), OSK_OK);
	T_TRUE("Pruefzahlen verschieden", strcmp(fa, fb) != 0);
	T_EQ("acht Ziffern", (long long)strlen(fa),
	     (long long)OSK_WALLET_FINGERPRINT_DIGITS);

	/* Ein EINZELNES vertauschtes Zeichen mitten in der Adresse aendert
	 * die Pruefzahl. Das ist ihre ganze Aufgabe. */
	{
		char fast[64];
		char f1[OSK_WALLET_FINGERPRINT_DIGITS + 1];
		char f2[OSK_WALLET_FINGERPRINT_DIGITS + 1];

		memcpy(fast, echt, strlen(echt) + 1u);
		T_EQ("vorher", osk_wallet_fingerprint(fast, f1, sizeof(f1)), OSK_OK);
		fast[20] = (char)((fast[20] == 'a') ? 'b' : 'a');
		T_EQ("nachher", osk_wallet_fingerprint(fast, f2, sizeof(f2)), OSK_OK);
		T_TRUE("ein Zeichen genuegt", strcmp(f1, f2) != 0);
	}

	/* Nur Ziffern, damit die Zahl am Geraet vorlesbar ist. */
	for (size_t i = 0; i < strlen(fa); i++)
		T_TRUE("nur Ziffern", fa[i] >= '0' && fa[i] <= '9');
}

/* Es gibt keine Blindunterschrift. Nicht als Schalter, nicht als
 * Sonderfall, nicht nach Freigabe. */
static void test_keine_blindunterschrift(void)
{
	osk_wallet_view_t view;

	/* Nichts lesbar: es wird nicht gefragt und nicht unterschrieben -
	 * so oft man es auch versucht. */
	for (unsigned i = 0; i < 3u; i++) {
		mock_ui_reset();
		mock_ui_answer(OSK_OK); /* der Nutzer WUERDE ja sagen */
		osk_wallet_view_init(&view);
		T_EQ("nicht lesbar heisst nein", osk_wallet_confirm(&view, "test"),
		     OSK_ERR_DENIED);
		/* Der Punkt: es wird nicht einmal gefragt. Eine Frage, die
		 * man mit Ja beantworten kann, IST die Blindunterschrift. */
		T_EQ("gar nicht gefragt", (long long)mock_ui_calls(), 0);
	}

	/* Betrag da, Gebuehr fehlt: auch das ist nicht lesbar. */
	mock_ui_reset();
	mock_ui_answer(OSK_OK);
	osk_wallet_view_init(&view);
	T_EQ("Adresse", osk_wallet_set_address(&view, "bc1qabc"), OSK_OK);
	memcpy(view.amount, "1.0 BTC", 8);
	view.readable = true;
	T_EQ("ohne Gebuehr kein Ja", osk_wallet_confirm(&view, "test"), OSK_ERR_DENIED);
	T_EQ("nicht gefragt", (long long)mock_ui_calls(), 0);

	/* readable false schlaegt alles andere - selbst wenn jedes Feld
	 * gefuellt ist. */
	mock_ui_reset();
	mock_ui_answer(OSK_OK);
	memcpy(view.fee, "0.0001 BTC", 11);
	view.readable = false;
	T_EQ("readable false heisst nein", osk_wallet_confirm(&view, "test"), OSK_ERR_DENIED);
	T_EQ("auch dann nicht gefragt", (long long)mock_ui_calls(), 0);
}

static void test_rule(void)
{
	osk_wallet_view_t view;

	/* Nichts lesbar und keine Extrafreigabe: es wird nicht gefragt und
	 * nicht unterschrieben. */
	mock_ui_reset();
	osk_wallet_view_init(&view);
	T_EQ("nicht lesbar heisst nein", osk_wallet_confirm(&view, "test"), OSK_ERR_DENIED);
	T_EQ("gar nicht gefragt", mock_ui_calls(), 0);
	T_TRUE("Grund gemeldet", strcmp(mock_ui_status_key(), "wallet.nichts-lesbar") == 0);

	/* Fehlt nur die Gebuehr, gilt dasselbe. */
	mock_ui_reset();
	osk_wallet_view_init(&view);
	T_EQ("Adresse gesetzt", osk_wallet_set_address(&view, "bc1qabc"), OSK_OK);
	memcpy(view.amount, "1.0 BTC", 8);
	view.readable = true;
	T_EQ("ohne Gebuehr kein Ja", osk_wallet_confirm(&view, "test"), OSK_ERR_DENIED);
	T_EQ("nicht gefragt", mock_ui_calls(), 0);

	/* Vollstaendig: es wird gefragt, und zwar mit allem drin. */
	mock_ui_reset();
	mock_ui_answer(OSK_OK);
	memcpy(view.fee, "0.0001 BTC", 11);
	T_EQ("vollstaendig", osk_wallet_confirm(&view, "Electrum"), OSK_OK);
	T_EQ("einmal gefragt", mock_ui_calls(), 1);
	T_EQ("als Unterschrift", mock_ui_kind(), OSK_ASK_SIGN);
	/* Der Empfaenger steht gegliedert da, mit der Pruefzahl dahinter. */
	T_TRUE("Empfaenger gegliedert", strncmp(mock_ui_subject(), "bc1q abc", 8) == 0);
	T_TRUE("Pruefzahl dabei", strchr(mock_ui_subject(), '#') != NULL);
	if (strchr(mock_ui_subject(), '#') == NULL)
		printf("  Betreff: %s\n", mock_ui_subject());
	T_TRUE("Betrag und Gebuehr", strcmp(mock_ui_detail(), "1.0 BTC + 0.0001 BTC") == 0);
	if (strcmp(mock_ui_detail(), "1.0 BTC + 0.0001 BTC") != 0)
		printf("  Zeile: %s\n", mock_ui_detail());

	/* Sagt der Nutzer nein, kommt das durch. */
	mock_ui_reset();
	mock_ui_answer(OSK_ERR_DENIED);
	T_EQ("Nutzer sagt nein", osk_wallet_confirm(&view, "test"), OSK_ERR_DENIED);

	/* Mehrere Empfaenger stehen als Zahl dabei. */
	mock_ui_reset();
	mock_ui_answer(OSK_OK);
	view.more_recipients = 3;
	T_EQ("mehrere Empfaenger", osk_wallet_confirm(&view, "test"), OSK_OK);
	T_TRUE("Zahl steht dabei", strstr(mock_ui_detail(), "+3") != NULL);
	if (strstr(mock_ui_detail(), "+3") == NULL)
		printf("  Zeile: %s\n", mock_ui_detail());
}

int main(void)
{
	test_amounts();
	test_amount256();
	test_address();
	test_address_angriff();
	test_keine_blindunterschrift();
	test_rule();
	return t_report("confirm");
}
