// Pruefungen fuer den Phishing-Schutz.
//
// Das ist der zweite Riegel: der erste sitzt in der Erweiterung (sie liest
// die Adresse aus der Adresszeile), dieser hier haelt auch dann, wenn die
// Erweiterung uebernommen ist und eine beliebige Adresse behauptet.
//
// Darum stehen hier vor allem die Faelle, mit denen eine solche Pruefung
// ueblicherweise ueberlistet wird: Teilstrings, Unterdomaenen, Punkte,
// Benutzernamen in der Adresse, Platzhalter, Steuerzeichen.
package classes

import "testing"

// Adressen, die gelesen werden koennen - und das Ergebnis.
func TestCheckOriginAccepts(t *testing.T) {
	cases := map[string]string{
		"https://example.org":           "https://example.org",
		"https://example.org/":          "https://example.org",
		"https://EXAMPLE.ORG":           "https://example.org",
		"https://example.org.":          "https://example.org",
		"https://example.org:8443":      "https://example.org",
		"https://login.example.org":     "https://login.example.org",
		"  https://example.org  ":       "https://example.org",
		"http://localhost:8101":         "http://localhost",
		"http://osk.localhost":          "http://osk.localhost",
		"http://127.0.0.1:8101":         "http://127.0.0.1",
		"https://[2001:db8::1]":         "https://2001:db8::1",
		"https://xn--bcher-kva.example": "https://xn--bcher-kva.example",
	}
	for raw, want := range cases {
		got, err := CheckOrigin(raw)
		if err != nil {
			t.Errorf("%q: %v", raw, err)
			continue
		}
		if got.String() != want {
			t.Errorf("%q ergab %q, erwartet %q", raw, got.String(), want)
		}
	}
}

// Adressen, die nicht durchkommen duerfen.
func TestCheckOriginRefuses(t *testing.T) {
	cases := []string{
		"",
		"   ",
		"example.org",       // ohne Verfahren: mehrdeutig
		"//example.org",     // ohne Verfahren
		"ftp://example.org", // falsches Verfahren
		"file:///C:/geheim.txt",
		"about:blank",
		"javascript:alert(1)",
		"data:text/html,<p>x",
		"chrome-extension://abc/",
		"https://bank.example.org@boese.tld", // Benutzername in der Adresse
		"https://user:pass@example.org",
		"https://example.org/anmelden", // mit Pfad
		"https://example.org/?x=1",
		"https://example.org/#oben",
		"https://",
		"https://.example.org",
		"https://example..org",
		"https://exam ple.org",
		"https://example.org\\boese.tld",
		"https://example.org\x00",
		"https://example.org\n",
		"http://example.org", // unverschluesselt und nicht lokal
		"http://192.168.1.10",
		"http://localhost.boese.tld", // sieht lokal aus, ist es nicht
		"https://*.example.org",
	}
	for _, raw := range cases {
		if got, err := CheckOrigin(raw); err == nil {
			t.Errorf("%q wurde angenommen als %q", raw, got.String())
		}
	}
	// Eine Adresse, die laenger ist als erlaubt.
	long := "https://"
	for len(long) < MaxOriginLen+10 {
		long += "a"
	}
	if _, err := CheckOrigin(long); err == nil {
		t.Error("eine zu lange Adresse wurde angenommen")
	}
}

// Das Feld eines Eintrags wird auf denselben Rechnernamen gebracht.
func TestSiteHost(t *testing.T) {
	ok := map[string]string{
		"example.org":                  "example.org",
		"EXAMPLE.ORG":                  "example.org",
		"example.org.":                 "example.org",
		"example.org/anmelden":         "example.org",
		"example.org:443":              "example.org",
		"https://example.org":          "example.org",
		"https://example.org/anmelden": "example.org",
		"http://example.org":           "example.org",
		"  example.org  ":              "example.org",
		"login.example.org":            "login.example.org",
	}
	for site, want := range ok {
		got, found := SiteHost(site)
		if !found {
			t.Errorf("%q wurde nicht gelesen", site)
			continue
		}
		if got != want {
			t.Errorf("%q ergab %q, erwartet %q", site, got, want)
		}
	}
	bad := []string{
		"",
		"   ",
		"*.example.org", // Platzhalter: kein Rechnername
		"*",
		"https://*.example.org",
		"example org",
		"example.org\x01",
		"..example.org",
		"https://user@example.org", // Benutzername im Eintrag
		"https://",
	}
	for _, site := range bad {
		if got, found := SiteHost(site); found {
			t.Errorf("%q wurde gelesen als %q", site, got)
		}
	}
}

// Der Kern: was passt zu was. Jede Zeile hier ist eine Entscheidung
// darueber, ob ein Passwort herausgeht.
func TestOriginMatches(t *testing.T) {
	origin, err := CheckOrigin("https://bank.example.org")
	if err != nil {
		t.Fatalf("Adresse: %v", err)
	}

	passt := []string{
		"bank.example.org",
		"BANK.EXAMPLE.ORG",
		"bank.example.org.",
		"https://bank.example.org",
		"https://bank.example.org/anmelden",
		"http://bank.example.org",
		"bank.example.org:443",
		// Ein Pfad aendert den Rechnernamen nicht - auch kein wirrer.
		// Diese Zeile stand erst in der falschen Liste: ich hielt sie fuer
		// einen Umgehungsversuch, aber der Rechnername IST bank.example.org.
		"bank.example.org/../boese.tld",
	}
	for _, site := range passt {
		if !origin.Matches(site) {
			t.Errorf("%q passt nicht auf bank.example.org, sollte aber", site)
		}
	}

	// Die Liste, um die es wirklich geht.
	passtNicht := []string{
		"example.org",                // der Elternname
		"www.bank.example.org",       // eine Unterdomaene
		"bank.example.org.boese.tld", // der klassische Phishing-Name
		"bank-example.org",
		"bankexample.org",
		"notbank.example.org",
		"boese.tld",
		"bank.example.com",
		"*.example.org",
		"",
	}
	for _, site := range passtNicht {
		if origin.Matches(site) {
			t.Errorf("%q passt auf bank.example.org, darf aber nicht", site)
		}
	}
}

// Eine Unterdomaene und ihr Elternname sind verschiedene Adressen - in
// BEIDE Richtungen. Wer nur eine Richtung prueft, hat die halbe Luecke.
func TestOriginSubdomainsAreSeparate(t *testing.T) {
	parent, err := CheckOrigin("https://example.org")
	if err != nil {
		t.Fatalf("Adresse: %v", err)
	}
	child, err := CheckOrigin("https://login.example.org")
	if err != nil {
		t.Fatalf("Adresse: %v", err)
	}
	if parent.Matches("login.example.org") {
		t.Error("ein Eintrag fuer login.example.org fuellt auf example.org aus")
	}
	if child.Matches("example.org") {
		t.Error("ein Eintrag fuer example.org fuellt auf login.example.org aus")
	}
	if !parent.Matches("example.org") || !child.Matches("login.example.org") {
		t.Error("der eigene Name passt nicht mehr - die Pruefung ist zu streng geworden")
	}
}
