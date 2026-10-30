// Der Weg hinein aus CSV: aus Browsern und aus Passwortverwaltungen, die
// nichts Besseres anbieten.
//
// WARUM NACH SPALTENNAMEN und nicht je Programm: jeder Browser und jede
// Verwaltung schreibt eine eigene Spaltenfolge, aber fast alle benutzen
// dieselben Woerter dafuer - url, username, password, name, notes. Eine
// Tabelle von Namen deckt damit Chrome, Edge, Firefox, Safari, Bitwarden,
// KeePass und 1Password ab, und eine neue Fassung eines dieser Programme
// bricht sie nicht, solange die Woerter bleiben. Eine Liste "wenn Chrome,
// dann Spalte 4" waere am Tag der naechsten Fassung falsch, ohne dass es
// auffaellt.
//
// Was NICHT geraten wird: welche Spalte das Kennwort ist, wenn keine so
// heisst. Dann sagt das Programm, welche Spalten es gefunden hat, und tut
// nichts. Eine geratene Kennwortspalte waere ein Tresor voll Unsinn - und
// der Anwender hat die Quelldatei danach vielleicht schon geloescht.
//
// Eine CSV-Datei ist eine fremde Eingabe. Die Obergrenzen stehen unten, und
// sie gelten beim Lesen, nicht danach.
package classes

import (
	"encoding/csv"
	"io"
	"strings"
)

// Obergrenzen beim Lesen einer Tabelle.
// csvBOM ist die Byte-Reihenfolgemarke, die Windows-Programme vor eine
// Textdatei setzen. Sie gehoert nicht zur ersten Spalte und wird abgenommen,
// sonst heisst die erste Spalte "\ufeffname" und wird nicht erkannt.
const csvBOM = "\ufeff"

const (
	// CsvMaxFileLen ist die groesste Datei.
	CsvMaxFileLen = 64 << 20
	// CsvMaxColumns ist die hoechste Zahl von Spalten. Eine Zeile mit
	// Millionen Spalten ist keine Tabelle, sondern ein Angriff auf den
	// Speicher.
	CsvMaxColumns = 256
	// CsvMaxRows ist die hoechste Zahl von Zeilen.
	CsvMaxRows = CxfMaxItems
	// CsvMaxFieldLen ist die groesste Laenge eines Feldes.
	CsvMaxFieldLen = 64 << 10
)

// csvRole ist, was eine Spalte bedeutet.
type csvRole int

const (
	csvIgnore csvRole = iota
	csvName
	csvURL
	csvUser
	csvPassword
	csvNote
	csvTotp
)

// csvHeaders ordnet Spaltennamen eine Bedeutung zu. Kleingeschrieben und
// ohne Zeichen, die keine Buchstaben sind - "Login Name", "login_name" und
// "loginname" sind dasselbe Wort.
//
// Die Namen stammen aus den Ausgabedateien, die diese Programme schreiben.
// Eine Spalte, die hier nicht steht, wird uebergangen und am Ende GENANNT:
// so sieht der Anwender, was in der Quelldatei stand und nicht mitkam.
var csvHeaders = map[string]csvRole{
	// Name des Eintrags
	"name": csvName, "title": csvName, "account": csvName,
	"displayname": csvName, "itemname": csvName,
	// Adresse
	"url": csvURL, "urls": csvURL, "website": csvURL, "websites": csvURL,
	"loginuri": csvURL, "loginurl": csvURL, "site": csvURL, "hostname": csvURL,
	"webaddress": csvURL,
	// Benutzername
	"username": csvUser, "user": csvUser, "loginname": csvUser,
	"loginusername": csvUser, "email": csvUser, "userid": csvUser,
	// Kennwort
	"password": csvPassword, "pass": csvPassword, "loginpassword": csvPassword,
	"passwort": csvPassword,
	// Notiz
	"note": csvNote, "notes": csvNote, "comment": csvNote, "comments": csvNote,
	"extra": csvNote, "notiz": csvNote,
	// Einmalkennwort
	"totp": csvTotp, "logintotp": csvTotp, "otpauth": csvTotp,
	"otp": csvTotp, "otpsecret": csvTotp, "totpsecret": csvTotp,
}

// CsvPlan ist das Ergebnis des Lesens.
type CsvPlan struct {
	// Candidates sind die Eintraege, die angelegt werden koennen.
	Candidates []CxfCandidate
	// Taken zaehlt sie, ohne Geheimnis.
	Taken []CxfLine
	// Left sind die Zeilen, die nicht mitkommen - mit Grund.
	Left []CxfLine
	// Warnings sind Hinweise zur Datei selbst - nicht zu einzelnen
	// Zeilen. Etwa: dieser Behaelter traegt keine Pruefsumme ueber seinem
	// Dateikopf, eine Veraenderung daran waere also nicht zu bemerken.
	// Sie stehen im Bericht; eine Warnung, die nur im Quelltext steht,
	// hat niemand gelesen.
	Warnings []CxfLine
	// UnusedColumns nennt die Spalten der Quelldatei, die dieses Programm
	// nicht verwendet hat. Sie stehen im Bericht: eine Spalte, die
	// stillschweigend verschwindet, ist der Verlust, den man erst Monate
	// spaeter merkt.
	UnusedColumns []string
}

// Wipe ueberschreibt alle Geheimnisse.
func (p *CsvPlan) Wipe() {
	for i := range p.Candidates {
		p.Candidates[i].Wipe()
	}
}

// CsvImport liest eine Tabelle.
func CsvImport(in io.Reader, profile uint8) (*CsvPlan, error) {
	if in == nil {
		return nil, Fail("error.csv.read")
	}
	data, err := io.ReadAll(io.LimitReader(in, CsvMaxFileLen+1))
	if err != nil {
		return nil, Fail("error.csv.read")
	}
	if len(data) > CsvMaxFileLen {
		return nil, Fail("error.csv.toobig")
	}
	text := strings.TrimPrefix(string(data), csvBOM)
	if strings.TrimSpace(text) == "" {
		return nil, Fail("error.csv.empty")
	}

	reader := csv.NewReader(strings.NewReader(text))
	// Verschiedene Zeilenlaengen sind erlaubt: Browser schreiben die
	// letzte Spalte gern weg, wenn sie leer ist. Eine Datei deswegen
	// abzuweisen waere Strenge an der falschen Stelle - fehlende Felder
	// werden unten als leer genommen.
	reader.FieldsPerRecord = -1
	// Anfuehrungszeichen mitten im Feld kommen in Notizen vor. Ein
	// Abbruch dort hiesse, dass eine Datei an einem Zeichen scheitert, das
	// kein Angriff ist.
	reader.LazyQuotes = true

	head, err := reader.Read()
	if err != nil {
		return nil, Fail("error.csv.format")
	}
	if len(head) > CsvMaxColumns {
		return nil, Fail("error.csv.columns")
	}
	roles, unused, hasPassword := csvRoles(head)
	if !hasPassword {
		// Der Name der Spalten geht in die Meldung. Ohne ihn stuende dort
		// "unbekanntes Format", und der Anwender wuesste nicht, ob die
		// Datei falsch ist oder dieses Programm.
		return nil, Fail("error.csv.nopassword", strings.Join(csvTrimList(head), ", "))
	}

	plan := &CsvPlan{UnusedColumns: unused}
	rows := 0
	for {
		row, err := reader.Read()
		if err == io.EOF {
			break
		}
		if err != nil {
			// Eine kaputte Zeile beendet das Lesen nicht: der Rest der
			// Datei ist oft in Ordnung. Sie wird GENANNT.
			plan.Left = append(plan.Left, CxfLine{ReasonKey: "csv.left.badrow"})
			continue
		}
		rows++
		if rows > CsvMaxRows {
			return nil, Fail("error.csv.manyrows")
		}
		// Das Einmalkennwort zuerst, denn es haengt nicht davon ab, ob in
		// derselben Zeile ein Kennwort steht: eine Zeile kann nur ein
		// Einmalkennwort enthalten, und das soll nicht verlorengehen.
		if candidate, line, ok := csvTotpRow(roles, row, profile); ok {
			plan.Candidates = append(plan.Candidates, candidate)
			plan.Taken = append(plan.Taken, line)
		} else if line.ReasonKey != "" {
			plan.Left = append(plan.Left, line)
		}
		candidate, line, ok := csvRow(roles, row, profile)
		if !ok {
			plan.Left = append(plan.Left, line)
			continue
		}
		plan.Candidates = append(plan.Candidates, candidate)
		plan.Taken = append(plan.Taken, line)
	}
	if len(plan.Candidates) == 0 && len(plan.Left) == 0 {
		return nil, Fail("error.csv.nothing")
	}
	return plan, nil
}

// csvRoles liest die Kopfzeile. Die ZWEITE Spalte mit derselben Bedeutung
// wird uebergangen und genannt - eine Datei mit zwei Kennwortspalten ist
// nichts, worueber dieses Programm entscheiden sollte.
func csvRoles(head []string) ([]csvRole, []string, bool) {
	roles := make([]csvRole, len(head))
	seen := map[csvRole]bool{}
	var unused []string
	hasPassword := false
	for i, name := range head {
		role, found := csvHeaders[csvNormalize(name)]
		if !found || seen[role] {
			roles[i] = csvIgnore
			if trimmed := strings.TrimSpace(name); trimmed != "" {
				unused = append(unused, trimmed)
			}
			continue
		}
		seen[role] = true
		roles[i] = role
		if role == csvPassword {
			hasPassword = true
		}
	}
	return roles, unused, hasPassword
}

// csvNormalize macht aus einem Spaltennamen das Wort, nach dem gesucht wird:
// kleingeschrieben, ohne Leerzeichen, Striche und Unterstriche.
func csvNormalize(name string) string {
	var b strings.Builder
	for _, r := range strings.ToLower(strings.TrimSpace(name)) {
		if r >= 'a' && r <= 'z' {
			b.WriteRune(r)
		}
	}
	return b.String()
}

func csvTrimList(head []string) []string {
	out := make([]string, 0, len(head))
	for _, name := range head {
		name = strings.TrimSpace(name)
		if name == "" {
			continue
		}
		if len(name) > MaxNameLen {
			name = name[:MaxNameLen]
		}
		out = append(out, name)
	}
	return out
}

// csvRow macht aus einer Zeile einen Eintrag.
//
// Eine Zeile OHNE Kennwort wird nicht als leerer Eintrag angelegt: Browser
// schreiben Zeilen, in denen nur eine Adresse steht, weil der Anwender dort
// nie ein Kennwort gespeichert hat. Sie kommen in den Bericht.
func csvRow(roles []csvRole, row []string, profile uint8) (CxfCandidate, CxfLine, bool) {
	fields := map[csvRole]string{}
	for i, role := range roles {
		if role == csvIgnore || i >= len(row) {
			continue
		}
		value := row[i]
		if len(value) > CsvMaxFieldLen {
			value = value[:CsvMaxFieldLen]
		}
		fields[role] = value
	}
	name := strings.TrimSpace(fields[csvName])
	site := strings.TrimSpace(fields[csvURL])
	if name == "" {
		name = csvHostOf(site)
	}
	if name == "" {
		name = strings.TrimSpace(fields[csvUser])
	}
	line := CxfLine{Name: name}

	// Ein Einmalkennwort in derselben Zeile ist ein EIGENER Eintrag im
	// Tresor - der Tresor haelt je Eintrag ein Geheimnis. Diese Zeile
	// liefert also bis zu zwei; hier kommt das Kennwort, das
	// Einmalkennwort holt csvTotpRow.
	password := fields[csvPassword]
	if strings.TrimSpace(password) == "" {
		line.ReasonKey = "csv.left.nopassword"
		return CxfCandidate{}, line, false
	}
	if name == "" {
		line.ReasonKey = "csv.left.noname"
		return CxfCandidate{}, line, false
	}
	meta := Item{
		Profile:  profile,
		Type:     ItemLogin,
		Name:     cxfShorten(name, MaxNameLen),
		Site:     cxfShorten(csvSiteOf(site, fields[csvUser]), MaxSiteLen),
		Mirrored: true,
	}
	line.Type = meta.Type
	line.Name = meta.Name
	return cxfFinish(meta, []byte(password), line)
}

// csvTotpRow macht aus dem Einmalkennwort derselben Zeile einen zweiten
// Eintrag, wenn eines dasteht.
func csvTotpRow(roles []csvRole, row []string, profile uint8) (CxfCandidate, CxfLine, bool) {
	var totp, name, site string
	for i, role := range roles {
		if i >= len(row) {
			continue
		}
		switch role {
		case csvTotp:
			totp = strings.TrimSpace(row[i])
		case csvName:
			name = strings.TrimSpace(row[i])
		case csvURL:
			site = strings.TrimSpace(row[i])
		}
	}
	if totp == "" {
		return CxfCandidate{}, CxfLine{}, false
	}
	if name == "" {
		name = csvHostOf(site)
	}
	if name == "" {
		return CxfCandidate{}, CxfLine{ReasonKey: "csv.left.noname"}, false
	}
	meta := Item{
		Profile:  profile,
		Type:     ItemTotp,
		Name:     cxfShorten(name+" (TOTP)", MaxNameLen),
		Site:     cxfShorten(site, MaxSiteLen),
		Mirrored: true,
	}
	line := CxfLine{Name: meta.Name, Type: meta.Type}
	return cxfFinish(meta, []byte(totp), line)
}

// csvSiteOf nennt die Adresse des Eintrags: die Adresse, sonst der
// Benutzername. Beides steht ohne Geheimnis in der Liste und hilft beim
// Wiederfinden.
func csvSiteOf(site, user string) string {
	if site != "" {
		return site
	}
	return strings.TrimSpace(user)
}

// csvHostOf holt den Rechnernamen aus einer Adresse - als Name eines
// Eintrags, der keinen hat. "https://www.example.com/login" wird
// "example.com".
func csvHostOf(site string) string {
	text := strings.TrimSpace(site)
	if text == "" {
		return ""
	}
	if cut := strings.Index(text, "://"); cut >= 0 {
		text = text[cut+3:]
	}
	if cut := strings.IndexAny(text, "/?#"); cut >= 0 {
		text = text[:cut]
	}
	if cut := strings.IndexByte(text, '@'); cut >= 0 {
		text = text[cut+1:]
	}
	if cut := strings.IndexByte(text, ':'); cut >= 0 {
		text = text[:cut]
	}
	text = strings.TrimPrefix(text, "www.")
	return strings.TrimSpace(text)
}
