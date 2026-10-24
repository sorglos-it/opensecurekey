// Adressen vergleichen - der Phishing-Schutz.
//
// Das ist die wichtigste Datei der Browser-Anbindung, und sie ist absichtlich
// streng und kurz. Eine Erweiterung, die ueberall ausfuellt, ist keine
// Bequemlichkeit, sondern eine Phishing-Hilfe: sie traegt das Passwort von
// bank.example.org auch auf bank.example.org.angreifer.tld ein, und der
// Anwender sieht nur, dass es "funktioniert" hat.
//
// Darum gilt hier eine einzige Regel, und sie hat keine Ausnahme:
//
//	Der RECHNERNAME des Eintrags und der RECHNERNAME der Seite muessen
//	ZEICHEN FUER ZEICHEN gleich sein.
//
// Was daraus folgt, und zwar mit Absicht:
//
//   - Kein Teilstring. "example.org" passt nicht auf "notexample.org", und
//     "bank.example.org" passt nicht auf "bank.example.org.boese.tld".
//   - Keine Unterdomaenen. Ein Eintrag fuer "example.org" fuellt NICHT auf
//     "login.example.org" aus. Wer beides braucht, legt zwei Eintraege an.
//     Das ist unbequem und richtig: wer Unterdomaenen zulaesst, muss wissen,
//     wo eine Domaene endet - und genau daran scheitern solche Regeln.
//   - Keine Platzhalter. "*.example.org" ist kein Rechnername und wird
//     abgewiesen.
//   - Kein http. Ueber eine unverschluesselte Verbindung geht kein
//     Geheimnis; die einzige Ausnahme ist der eigene Rechner, wo niemand
//     mithoeren kann.
//
// Und die Adresse der Seite kommt aus der ADRESSZEILE des Browsers, nie aus
// dem Inhalt der Seite. Das stellt die Erweiterung sicher (sie liest
// tab.url), und hier wird sie noch einmal geprueft.
package classes

import (
	"net"
	"net/url"
	"strings"
)

// Origin ist eine geprueft Adresse: Verfahren und Rechnername, sonst nichts.
type Origin struct {
	Scheme string
	Host   string
}

// String gibt die Adresse zurueck, wie sie angezeigt wird.
func (o Origin) String() string { return o.Scheme + "://" + o.Host }

// MaxOriginLen ist die Laenge, die eine Adresse haben darf. Ein Rechnername
// hat nach den Regeln des Netzes hoechstens 253 Zeichen; der Rest ist Platz
// fuer das Verfahren.
const MaxOriginLen = 300

// CheckOrigin prueft die Adresse einer Seite.
//
// Angenommen wird nur eine vollstaendige Adresse mit Verfahren. Ein nackter
// Rechnername waere mehrdeutig: "example.org:8080" koennte ein Rechner mit
// Port oder ein Verfahren mit Rechnernamen sein, und solche Zweideutigkeiten
// sind der Anfang jeder Umgehung.
func CheckOrigin(raw string) (Origin, error) {
	if len(raw) > MaxOriginLen {
		return Origin{}, Fail("error.native.origin")
	}
	// Steuerzeichen werden am ROHEN Text geprueft, vor dem Abschneiden der
	// Leerzeichen. Gemessen am 24.10.2026: wird erst abgeschnitten, dann
	// geprueft, kommt "https://example.org\n" durch - der Umbruch ist dann
	// schon weg und wird nie gesehen. Hier ist das harmlos, aber die
	// Reihenfolge ist es nicht: dieselbe Pruefung soll einen Umbruch MITTEN
	// in der Adresse fangen, und eine Pruefung, die nach dem Putzen
	// stattfindet, sieht nie, was das Putzen entfernt hat.
	for index := 0; index < len(raw); index++ {
		c := raw[index]
		if c == ' ' || c == '\t' {
			continue
		}
		if c < 0x20 || c == 0x7f {
			return Origin{}, Fail("error.native.origin")
		}
	}
	text := strings.TrimSpace(raw)
	if text == "" {
		return Origin{}, Fail("error.native.origin")
	}
	parsed, err := url.Parse(text)
	if err != nil {
		return Origin{}, Fail("error.native.origin")
	}
	scheme := strings.ToLower(parsed.Scheme)
	if scheme != "https" && scheme != "http" {
		return Origin{}, Fail("error.native.origin")
	}
	// Ein Benutzername in der Adresse ist der aelteste Trick der Zunft:
	// "https://bank.example.org@boese.tld/" sieht aus wie die Bank und
	// zeigt auf den Angreifer.
	if parsed.User != nil {
		return Origin{}, Fail("error.native.origin")
	}
	if parsed.Opaque != "" {
		return Origin{}, Fail("error.native.origin")
	}
	// Die Erweiterung schickt eine Adresse OHNE Pfad. Kommt doch einer, ist
	// das nicht das, was wir vereinbart haben.
	if parsed.Path != "" && parsed.Path != "/" {
		return Origin{}, Fail("error.native.origin")
	}
	if parsed.RawQuery != "" || parsed.Fragment != "" {
		return Origin{}, Fail("error.native.origin")
	}
	host, ok := cleanHost(parsed.Host)
	if !ok {
		return Origin{}, Fail("error.native.origin")
	}
	// http nur auf dem eigenen Rechner. Dort kann niemand mithoeren, und
	// ohne diese Ausnahme koennte man an der eigenen Software nicht
	// arbeiten.
	if scheme == "http" && !isLocalHostName(host) {
		return Origin{}, Fail("error.native.insecure")
	}
	return Origin{Scheme: scheme, Host: host}, nil
}

// cleanHost holt den Rechnernamen aus dem Teil hinter den zwei Schraegstrichen
// und putzt ihn: klein, ohne Port, ohne abschliessenden Punkt.
func cleanHost(raw string) (string, bool) {
	host := raw
	// Ein Port wird abgeschnitten: er gehoert nicht zum Rechnernamen, und
	// ein Geheimnis gehoert zum Rechner, nicht zu einer Tuer an ihm.
	if trimmed, _, err := net.SplitHostPort(host); err == nil {
		host = trimmed
	} else if strings.HasPrefix(host, "[") {
		// Eine Adresse in eckigen Klammern ohne Port.
		host = strings.TrimSuffix(strings.TrimPrefix(host, "["), "]")
	}
	host = strings.ToLower(strings.TrimSuffix(host, "."))
	if host == "" || len(host) > 253 {
		return "", false
	}
	// Ein Rechnername besteht aus Buchstaben, Zahlen, Strich und Punkt -
	// und bei einer Adresse in Zahlen zusaetzlich aus Doppelpunkten.
	// Alles andere (Leerzeichen, Schraegstrich, Backslash, Prozentzeichen)
	// ist ein Versuch, die Pruefung zu umgehen.
	for index := 0; index < len(host); index++ {
		c := host[index]
		ok := (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			c == '-' || c == '.' || c == ':'
		if !ok {
			return "", false
		}
	}
	// Zwei Punkte hintereinander oder ein Punkt am Anfang: kein Name.
	if strings.HasPrefix(host, ".") || strings.Contains(host, "..") {
		return "", false
	}
	// Doppelpunkte nur in einer Zahlenadresse.
	if strings.Contains(host, ":") && net.ParseIP(host) == nil {
		return "", false
	}
	return host, true
}

// isLocalHostName sagt, ob der Name den eigenen Rechner meint.
func isLocalHostName(host string) bool {
	if host == "localhost" || strings.HasSuffix(host, ".localhost") {
		return true
	}
	if ip := net.ParseIP(host); ip != nil {
		return ip.IsLoopback()
	}
	return false
}

// Matches sagt, ob ein Eintrag zu dieser Adresse gehoert.
//
// site ist das Feld aus dem Eintrag. Es darf als nackter Rechnername
// ("example.org") oder als Adresse ("https://example.org/login") dort
// stehen - beides schreiben Anwender, und beides wird auf denselben
// Rechnernamen gebracht. Verglichen wird dann Zeichen fuer Zeichen.
func (o Origin) Matches(site string) bool {
	host, ok := SiteHost(site)
	if !ok {
		return false
	}
	return host == o.Host
}

// SiteHost holt den Rechnernamen aus dem Feld eines Eintrags.
//
// Ein Platzhalter wird hier abgewiesen und nicht ausgewertet: "*.example.org"
// ist kein Rechnername, und wer ihn eintraegt, soll merken, dass er nichts
// ausfuellt - statt zu glauben, er habe eine Regel gesetzt.
func SiteHost(site string) (string, bool) {
	text := strings.TrimSpace(site)
	if text == "" || len(text) > MaxSiteLen || hasControl(text) {
		return "", false
	}
	// Ein Platzhalter wird abgewiesen und nicht ausgewertet. Diese Zeile
	// ist DOPPELT: cleanHost laesst ohnehin nur Buchstaben, Zahlen, Strich,
	// Punkt und Doppelpunkt durch, also auch kein Sternchen. Gemessen am
	// 24.10.2026 - der Mutant, der sie entfernt, lebt deshalb.
	//
	// Sie bleibt trotzdem stehen, und zwar mit Absicht: sie sagt, dass
	// "*.example.org" KEINE Regel ist, sondern ein Eintrag, der nirgends
	// ausfuellt. Wer eines Tages cleanHost lockert, faellt hier auf.
	if strings.Contains(text, "*") {
		return "", false
	}
	if !strings.Contains(text, "//") {
		// Ein nackter Rechnername, vielleicht mit Pfad: "example.org/login".
		if cut := strings.IndexAny(text, "/?#\\"); cut >= 0 {
			text = text[:cut]
		}
		return cleanHost(text)
	}
	parsed, err := url.Parse(text)
	if err != nil || parsed.Host == "" {
		return "", false
	}
	// Ein Benutzername im Feld des Eintrags ist genauso verdaechtig wie in
	// der Adresse der Seite.
	if parsed.User != nil {
		return "", false
	}
	return cleanHost(parsed.Host)
}
