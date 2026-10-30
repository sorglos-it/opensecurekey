// Der Weg hinein: eine CXF-Datei lesen und in den Tresor legen.
//
// EINE FREMDE DATEI IST EINE FREMDE EINGABE. Dieser Leser ist die Stelle,
// die ein Angreifer erreicht, ohne vorher irgendetwas zu duerfen - er
// braucht nur eine Datei, die der Anwender einliest. Darum:
//
//   - Obergrenzen vor dem Lesen, nicht danach: Dateilaenge, Zahl der
//     Eintraege, Laenge jedes Textes, Tiefe der Sammlungen.
//   - Jede Zahl aus der Datei wird geprueft, bevor mit ihr gerechnet wird.
//   - Kein Wert aus der Datei wird zu einem Pfad, einem Befehl oder einer
//     Laengenangabe fuer einen Puffer.
//   - Was nicht verstanden wird, wird ABGEWIESEN oder GENANNT, nie geraten.
//
// Und das Gegenstueck zum Ausgang: eine Datei kann Zugangsdaten enthalten,
// die dieser Tresor nicht halten kann - einen Passkey etwa. Die werden
// GENANNT. Ein Einlesen, das stillschweigend die Haelfte verschluckt, ist
// schlimmer als eines, das nichts tut: im ersten Fall glaubt der Anwender,
// er sei umgezogen.
package classes

import (
	"crypto/x509"
	"encoding/json"
	"encoding/pem"
	"io"
	"net/url"
	"strconv"
	"strings"
)

// CxfRead liest eine CXF-Datei.
//
// Die Obergrenze gilt VOR dem Auswerten: eine Datei, die laenger ist als
// CxfMaxFileLen, wird nicht erst eingelesen und dann abgelehnt.
func CxfRead(in io.Reader) (*CxfHeader, error) {
	if in == nil {
		return nil, Fail("error.cxf.format")
	}
	// Ein Byte mehr lesen als erlaubt: nur so ist zu unterscheiden, ob die
	// Datei genau die Grenze hat oder darueber liegt.
	data, err := io.ReadAll(io.LimitReader(in, CxfMaxFileLen+1))
	if err != nil {
		return nil, Fail("error.cxf.read")
	}
	if len(data) > CxfMaxFileLen {
		return nil, Fail("error.cxf.toobig")
	}
	return CxfParse(data)
}

// CxfParse wertet die Bytes aus.
func CxfParse(data []byte) (*CxfHeader, error) {
	if len(data) == 0 {
		return nil, Fail("error.cxf.empty")
	}
	var header CxfHeader
	decoder := json.NewDecoder(strings.NewReader(string(data)))
	// Unbekannte Felder im Rahmen sind ein Fehler. Im INHALT eines
	// Zugangsdatums ist das anders geregelt (siehe CxfOther in
	// exchange.go) - aber ein Rahmen, der anders aussieht als die
	// Beschreibung, ist keine CXF-Datei.
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&header); err != nil {
		return nil, Fail("error.cxf.format")
	}
	// Nach dem Rahmen darf nichts mehr kommen. Zwei JSON-Werte
	// hintereinander sind sonst eine Datei, von der jedes Programm einen
	// anderen Teil liest.
	if decoder.More() {
		return nil, Fail("error.cxf.trailing")
	}
	if err := cxfCheck(&header); err != nil {
		return nil, err
	}
	return &header, nil
}

// CxfWrite schreibt eine Datei. Eingerueckt, weil sie gelesen wird - von
// Menschen, die wissen wollen, was gerade ihr Geraet verlassen hat.
func CxfWrite(header *CxfHeader, out io.Writer) error {
	if header == nil || out == nil {
		return Fail("error.cxf.format")
	}
	encoder := json.NewEncoder(out)
	encoder.SetIndent("", "  ")
	// Kein Umschreiben von <, > und & in Fluchtfolgen: das ist der Schutz
	// eines HTML-Lesers, und diese Datei ist keine Webseite. Ein Kennwort
	// mit einem Kleinerzeichen soll genauso herauskommen, wie es
	// hineinging.
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(header); err != nil {
		return Fail("error.cxf.write")
	}
	return nil
}

// --- Pruefen ------------------------------------------------------------------

// cxfCheck prueft den Aufbau. Hier wird abgewiesen, was eine Datei
// unbrauchbar macht; was nur dieser Tresor nicht halten kann, wird spaeter
// gezaehlt und genannt.
func cxfCheck(header *CxfHeader) error {
	// Eine groessere Hauptnummer heisst: ein Format, das dieses Programm
	// nicht kennt. Es wird NICHT versucht - "liest wie 1.0" waere eine
	// Annahme ueber eine Beschreibung, die es noch nicht gibt.
	if header.Version.Major != CxfVersionMajor {
		return Fail("error.cxf.version", strconv.Itoa(int(header.Version.Major))+
			"."+strconv.Itoa(int(header.Version.Minor)))
	}
	if len(header.Accounts) == 0 {
		return Fail("error.cxf.noaccounts")
	}
	if err := cxfCheckText(header.ExporterRpID, CxfMaxTextLen); err != nil {
		return err
	}
	if err := cxfCheckText(header.ExporterDisplayName, CxfMaxTextLen); err != nil {
		return err
	}
	total := 0
	for i := range header.Accounts {
		account := &header.Accounts[i]
		if len(account.ID) == 0 || len(account.ID) > CxfMaxIDLen {
			return Fail("error.cxf.accountid")
		}
		if err := cxfCheckText(account.Username, MaxNameLen*4); err != nil {
			return err
		}
		total += len(account.Items)
		if total > CxfMaxItems {
			return Fail("error.cxf.manyitems")
		}
		for j := range account.Items {
			if err := cxfCheckItem(&account.Items[j]); err != nil {
				return err
			}
		}
		for j := range account.Collections {
			if err := cxfCheckCollection(&account.Collections[j], 1); err != nil {
				return err
			}
		}
	}
	return nil
}

func cxfCheckItem(item *CxfItem) error {
	if len(item.ID) == 0 || len(item.ID) > CxfMaxIDLen {
		return Fail("error.cxf.itemid")
	}
	if strings.TrimSpace(item.Title) == "" {
		return Fail("error.cxf.notitle")
	}
	if err := cxfCheckText(item.Title, CxfMaxTextLen); err != nil {
		return err
	}
	if err := cxfCheckText(item.Subtitle, CxfMaxTextLen); err != nil {
		return err
	}
	if len(item.Credentials) == 0 {
		return Fail("error.cxf.nocredentials")
	}
	for _, tag := range item.Tags {
		if err := cxfCheckText(tag, CxfMaxTextLen); err != nil {
			return err
		}
	}
	for _, credential := range item.Credentials {
		if err := cxfCheckCredential(credential); err != nil {
			return err
		}
	}
	return nil
}

// cxfCheckCollection prueft eine Sammlung und ihre Untersammlungen. Die
// Tiefe wird mitgezaehlt: eine Datei, die sich selbst tausendfach
// schachtelt, soll hier anstossen und nicht im Stapelspeicher.
func cxfCheckCollection(collection *CxfCollection, depth int) error {
	if depth > CxfMaxDepth {
		return Fail("error.cxf.deep")
	}
	if len(collection.ID) == 0 || len(collection.ID) > CxfMaxIDLen {
		return Fail("error.cxf.collectionid")
	}
	if err := cxfCheckText(collection.Title, CxfMaxTextLen); err != nil {
		return err
	}
	for i := range collection.SubCollections {
		if err := cxfCheckCollection(&collection.SubCollections[i], depth+1); err != nil {
			return err
		}
	}
	return nil
}

func cxfCheckCredential(credential CxfCredential) error {
	switch value := credential.(type) {
	case *CxfBasicAuth:
		if err := cxfCheckField(value.Username); err != nil {
			return err
		}
		return cxfCheckField(value.Password)
	case *CxfGenPassword:
		return cxfCheckText(value.Password, CxfMaxTextLen)
	case *CxfNote:
		return cxfCheckField(&value.Content)
	case *CxfAPIKey:
		return cxfCheckField(value.Key)
	case *CxfTotp:
		if value.Digits > 10 {
			return Fail("error.cxf.totpsecret")
		}
		return cxfCheckText(value.Secret, CxfMaxTextLen)
	case *CxfSSHKey:
		if len(value.PrivateKey) == 0 || len(value.PrivateKey) > CxfMaxTextLen {
			return Fail("error.cxf.sshkey")
		}
		return cxfCheckText(value.KeyType, MaxNameLen*4)
	case *CxfPasskey:
		if len(value.Key) == 0 || len(value.Key) > CxfMaxTextLen {
			return Fail("error.cxf.passkey")
		}
		return cxfCheckText(value.RpID, CxfMaxTextLen)
	}
	return nil
}

func cxfCheckField(field *CxfField) error {
	if field == nil {
		return nil
	}
	return cxfCheckText(field.Value, CxfMaxTextLen)
}

// cxfCheckText prueft die Laenge und dass kein Steuerzeichen darin steht.
// Zeilenumbruch und Tabulator sind erlaubt - eine Notiz hat Zeilen.
func cxfCheckText(text string, limit int) error {
	if len(text) > limit {
		return Fail("error.cxf.longtext")
	}
	for _, r := range text {
		if r == '\n' || r == '\r' || r == '\t' {
			continue
		}
		if r < 0x20 || r == 0x7f {
			return Fail("error.cxf.control")
		}
	}
	return nil
}

// --- In den Tresor ------------------------------------------------------------

// CxfCandidate ist ein Eintrag, der in den Tresor passen wuerde. Das
// Geheimnis liegt daneben und nicht in Item - genau wie ueberall sonst in
// diesem Programm.
type CxfCandidate struct {
	Meta   Item
	Secret []byte
}

// Wipe ueberschreibt das Geheimnis.
func (c *CxfCandidate) Wipe() { Wipe(c.Secret) }

// CxfPlan ist das Ergebnis des Lesens: was hineinkann und was nicht.
type CxfPlan struct {
	// From nennt, wer die Datei geschrieben hat. Das ist die Angabe, die
	// vor dem Einlesen angezeigt wird - eine Datei aus unbekannter Hand
	// soll nicht wie eine eigene aussehen.
	From string `json:"from"`
	// Candidates sind die Eintraege, die angelegt werden koennen.
	Candidates []CxfCandidate `json:"-"`
	// Taken zaehlt sie fuer den Bericht, ohne Geheimnis.
	Taken []CxfLine `json:"taken"`
	// Left sind die Zugangsdaten, die dieser Tresor NICHT halten kann.
	Left []CxfLine `json:"left"`
	// Warnings sind Hinweise zur Datei selbst, siehe CsvPlan.Warnings.
	Warnings []CxfLine `json:"warnings,omitempty"`
}

// Wipe ueberschreibt alle Geheimnisse des Plans.
func (p *CxfPlan) Wipe() {
	for i := range p.Candidates {
		p.Candidates[i].Wipe()
	}
}

// CxfImport macht aus einer gelesenen Datei einen Plan. Es schreibt NICHTS
// zum Geraet: erst der Plan, dann die Anzeige, dann - wenn der Anwender es
// sagt - CxfApply.
func CxfImport(header *CxfHeader, profile uint8) (*CxfPlan, error) {
	if header == nil {
		return nil, Fail("error.cxf.format")
	}
	plan := &CxfPlan{From: cxfFrom(header)}
	for _, account := range header.Accounts {
		for _, item := range account.Items {
			for _, credential := range item.Credentials {
				candidate, line, ok := cxfCandidateFor(item, credential, profile)
				if !ok {
					plan.Left = append(plan.Left, line)
					continue
				}
				plan.Candidates = append(plan.Candidates, candidate)
				plan.Taken = append(plan.Taken, line)
			}
		}
	}
	if len(plan.Candidates) == 0 && len(plan.Left) == 0 {
		return nil, Fail("error.cxf.nothing")
	}
	return plan, nil
}

// cxfFrom baut die Angabe, WER die Datei geschrieben hat.
func cxfFrom(header *CxfHeader) string {
	name := strings.TrimSpace(header.ExporterDisplayName)
	id := strings.TrimSpace(header.ExporterRpID)
	switch {
	case name != "" && id != "":
		return name + " (" + id + ")"
	case id != "":
		return id
	case name != "":
		return name
	}
	return ""
}

// cxfCandidateFor macht aus einem Zugangsdatum einen Eintrag des Tresors.
//
// Was NICHT hineinpasst, bekommt eine Zeile mit Grund. Der wichtigste Fall
// ist der Passkey: dafuer hat der Tresor keine Art (siehe CxfPasskey in
// exchange.go), und eine zu erfinden hiesse, eine Zahl an einer zweiten
// Stelle zu fuehren.
func cxfCandidateFor(item CxfItem, credential CxfCredential, profile uint8) (CxfCandidate, CxfLine, bool) {
	line := CxfLine{Name: item.Title}
	meta := Item{
		Profile:  profile,
		Name:     cxfShorten(item.Title, MaxNameLen),
		Site:     cxfShorten(cxfSiteFor(item), MaxSiteLen),
		Mirrored: true,
	}

	switch value := credential.(type) {
	case *CxfBasicAuth:
		if value.Password == nil || value.Password.Value == "" {
			// Ein Login ohne Kennwort ist kein Geheimnis. Es wird nicht
			// als leerer Eintrag angelegt, sondern genannt.
			line.ReasonKey = "cxf.in.nosecret"
			return CxfCandidate{}, line, false
		}
		meta.Type = ItemLogin
		if value.Username != nil && value.Username.Value != "" && meta.Site == "" {
			meta.Site = cxfShorten(value.Username.Value, MaxSiteLen)
		}
		line.Type = meta.Type
		return cxfFinish(meta, []byte(value.Password.Value), line)

	case *CxfGenPassword:
		if value.Password == "" {
			line.ReasonKey = "cxf.in.nosecret"
			return CxfCandidate{}, line, false
		}
		meta.Type = ItemLogin
		line.Type = meta.Type
		return cxfFinish(meta, []byte(value.Password), line)

	case *CxfNote:
		if value.Content.Value == "" {
			line.ReasonKey = "cxf.in.nosecret"
			return CxfCandidate{}, line, false
		}
		meta.Type = ItemNote
		line.Type = meta.Type
		return cxfFinish(meta, []byte(value.Content.Value), line)

	case *CxfAPIKey:
		if value.Key == nil || value.Key.Value == "" {
			line.ReasonKey = "cxf.in.nosecret"
			return CxfCandidate{}, line, false
		}
		meta.Type = ItemAPIKey
		line.Type = meta.Type
		return cxfFinish(meta, []byte(value.Key.Value), line)

	case *CxfTotp:
		meta.Type = ItemTotp
		line.Type = meta.Type
		secret := cxfTotpSecretOut(value)
		if secret == "" {
			line.ReasonKey = "cxf.in.nosecret"
			return CxfCandidate{}, line, false
		}
		return cxfFinish(meta, []byte(secret), line)

	case *CxfSSHKey:
		meta.Type = ItemSSHKey
		line.Type = meta.Type
		text, err := cxfSSHToPEM(value)
		if err != nil {
			line.ReasonKey = cxfRefusalKey(err)
			return CxfCandidate{}, line, false
		}
		return cxfFinish(meta, text, line)

	case *CxfPasskey:
		line.ReasonKey = "cxf.in.nopasskey"
		if value.RpID != "" {
			line.Name = item.Title + " (" + value.RpID + ")"
		}
		return CxfCandidate{}, line, false
	}

	// Alle uebrigen Arten der Beschreibung: Adresse, Karte, Ausweis,
	// Datei, Name, Verweis, eigene Felder, Funknetz - und jede, die ein
	// anderes Programm erweitert hat.
	line.ReasonKey = "cxf.in.nokind"
	line.Name = item.Title + " (" + credential.CxfType() + ")"
	return CxfCandidate{}, line, false
}

// cxfFinish prueft den gebauten Eintrag gegen die Regeln des Tresors. Ein
// Eintrag, den das Geraet abweisen wuerde, wird hier genannt und nicht
// hingeschickt - sonst faellt es erst nach der Freigabe auf.
func cxfFinish(meta Item, secret []byte, line CxfLine) (CxfCandidate, CxfLine, bool) {
	if len(secret) > MaxSecretLen {
		Wipe(secret)
		line.ReasonKey = "cxf.in.toolong"
		return CxfCandidate{}, line, false
	}
	if err := CheckItem(meta); err != nil {
		Wipe(secret)
		line.ReasonKey = cxfRefusalKey(err)
		return CxfCandidate{}, line, false
	}
	return CxfCandidate{Meta: meta, Secret: secret}, line, true
}

// cxfSiteFor nennt die Adresse eines Eintrags: der erste Geltungsbereich,
// sonst der Untertitel.
func cxfSiteFor(item CxfItem) string {
	if item.Scope != nil && len(item.Scope.URLs) > 0 {
		return item.Scope.URLs[0]
	}
	return item.Subtitle
}

// cxfShorten kuerzt auf die Grenze des Geraets - an einer Zeichengrenze, nie
// mitten in einem Zeichen. Gekuerzt wird nur, was der Tresor sonst gar nicht
// annehmen wuerde, und nur bei Namen und Adressen; ein GEHEIMNIS wird
// niemals gekuerzt, sondern genannt (siehe cxfFinish).
func cxfShorten(text string, limit int) string {
	text = strings.TrimSpace(strings.Map(func(r rune) rune {
		if r < 0x20 || r == 0x7f {
			return ' '
		}
		return r
	}, text))
	if len(text) <= limit {
		return text
	}
	cut := limit
	for cut > 0 && text[cut]&0xc0 == 0x80 {
		cut--
	}
	return strings.TrimSpace(text[:cut])
}

// cxfTotpSecretOut schreibt das Einmalkennwort so in den Tresor, dass die
// drei Werte nicht verlorengehen: als Adresse otpauth://, wenn einer von
// ihnen von der Vorgabe abweicht, sonst als nacktes Geheimnis.
//
// Das ist die Umkehrung von cxfTotpFrom, und darum ist der Rundlauf gleich.
func cxfTotpSecretOut(value *CxfTotp) string {
	secret := cxfCleanBase32(value.Secret)
	if secret == "" {
		return ""
	}
	period := value.Period
	if period == 0 {
		period = cxfTotpPeriod
	}
	digits := value.Digits
	if digits == 0 {
		digits = cxfTotpDigits
	}
	algorithm := strings.ToLower(value.Algorithm)
	if algorithm == "" {
		algorithm = cxfTotpAlgo
	}
	if period == cxfTotpPeriod && digits == cxfTotpDigits && algorithm == cxfTotpAlgo {
		return secret
	}
	query := "secret=" + secret +
		"&period=" + strconv.FormatUint(uint64(period), 10) +
		"&digits=" + strconv.FormatUint(uint64(digits), 10) +
		"&algorithm=" + algorithm
	if value.Issuer != "" {
		query += "&issuer=" + url.QueryEscape(value.Issuer)
	}
	return "otpauth://totp/" + url.PathEscape(value.Username) + "?" + query
}

// cxfSSHToPEM schreibt den privaten Teil so in den Tresor, wie
// ParseSSHKey ihn wieder liest: PKCS#8 in PEM. Der Tresor haelt
// Schluesseldateien, und das ist eine.
func cxfSSHToPEM(value *CxfSSHKey) ([]byte, error) {
	inner, err := x509.ParsePKCS8PrivateKey(value.PrivateKey)
	if err != nil {
		// Auch die beiden aelteren Formen kommen vor - CXF sagt nur
		// "DER", nicht welche.
		if key, err2 := x509.ParseECPrivateKey(value.PrivateKey); err2 == nil {
			inner = key
		} else if key, err3 := x509.ParsePKCS1PrivateKey(value.PrivateKey); err3 == nil {
			inner = key
		} else {
			return nil, Fail("error.agent.keyformat")
		}
	}
	der, err := x509.MarshalPKCS8PrivateKey(inner)
	if err != nil {
		return nil, Fail("error.agent.keytype", value.KeyType)
	}
	return pem.EncodeToMemory(&pem.Block{Type: "PRIVATE KEY", Bytes: der}), nil
}

// --- Anlegen ------------------------------------------------------------------

// CxfApply legt die Eintraege des Plans an. Jeder einzeln, und ein
// fehlgeschlagener bricht nicht ab - er kommt in die Liste der
// zurueckgebliebenen. Am Ende sagt der Bericht, was wirklich im Geraet
// liegt, nicht was geplant war.
func CxfApply(client *Client, plan *CxfPlan) (CxfReport, error) {
	report := CxfReport{Destination: plan.From}
	if client == nil {
		return report, Fail("error.guard.nodevice")
	}
	info, err := client.Hello()
	if err != nil {
		return report, err
	}
	if !info.Unlocked {
		return report, Fail("error.device.locked")
	}
	for i := range plan.Candidates {
		candidate := &plan.Candidates[i]
		line := CxfLine{Name: candidate.Meta.Name, Type: candidate.Meta.Type}
		id, err := client.Add(candidate.Meta, append([]byte(nil), candidate.Secret...))
		if err != nil {
			line.ReasonKey = cxfRefusalKey(err)
			report.Left = append(report.Left, line)
			continue
		}
		line.ID = id
		report.Taken = append(report.Taken, line)
	}
	// Was der Tresor von Anfang an nicht halten konnte, bleibt im Bericht
	// stehen. Es waere das Leichteste, diese Zeilen hier zu vergessen -
	// und genau dann glaubte der Anwender, er sei vollstaendig umgezogen.
	report.Left = append(report.Left, plan.Left...)
	return report, nil
}
