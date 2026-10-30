// Der Weg hinaus: den Tresor als CXF-Datei schreiben.
//
// DIE SPANNUNG, die dieser Weg nicht wegbaut, sondern sichtbar macht:
//
// Ein Ausgang heisst, dass Zugangsdaten das Geraet verlassen koennen. Das
// steht neben dem Satz "Schluessel verlassen den Chip nie", und beides ist
// wahr - weil es zwei verschiedene Dinge sind. Ein Kennwort, das der
// Anwender in den Tresor gelegt hat, ist ein Geheimnis, das er auch wieder
// herausbekommen muss; `reveal` gibt es seit dem ersten Tag heraus, einzeln,
// mit Tastendruck, und jedes Mal steht es im Geraeteprotokoll. Ein
// Schluessel, der IM CHIP entstanden ist, kommt dagegen nicht heraus, und
// dieser Weg macht daraus keine Ausnahme: er bekommt vom Geraet eine
// Absage, und die Absage steht im Bericht.
//
// Darum verlangt dieser Weg DIE DREI, die auch ein `reveal` verlangt, und
// es gibt keinen zweiten Eingang, der ohne sie auskommt - auch keinen "nur
// fuer Pruefungen":
//
//  1. PIN. Nicht ein Merker in diesem Programm, sondern `hello` am Geraet:
//     ein gesperrtes Geraet gibt nichts heraus, und gefragt wird das Geraet.
//  2. Tastendruck. Den verlangt das Geraet je `reveal` selbst. Dieses
//     Programm kann ihn nicht umgehen und versucht es nicht.
//  3. Eine Anzeige, die sagt WOHIN. Das ist die, die es vorher nicht gab:
//     CxfExport nimmt ohne ein benanntes Ziel und ohne ausdrueckliches Ja
//     gar nichts auf - error.cxf.noconsent, bevor der erste Eintrag
//     geholt wird.
//
// Und die Grenze daneben, die KEIN Mangel ist: ein Eintrag, der nicht heraus
// kann, ist nicht exportierbar. Eine Sicherung, die stillschweigend die
// Haelfte auslaesst, ist schlimmer als keine - darum nennt der Bericht jede
// Zeile, die nicht mitgekommen ist, mit Grund.
package classes

import (
	"crypto/sha256"
	"crypto/x509"
	"encoding/base32"
	"errors"
	"net/url"
	"strconv"
	"strings"
	"time"
)

// CxfConsent ist die Freigabe fuer einen Ausgang: WOHIN es geht und dass der
// Anwender es gesehen hat.
//
// Beide Felder sind Pflicht, und zwar aus verschiedenen Gruenden. Ohne
// Destination gibt es nichts anzuzeigen; ohne Confirmed hat niemand
// hingesehen. Ein Vorgabewert, der eines von beiden ersetzt, waere die
// stille Vorgabe, die dieses Projekt nicht baut.
type CxfConsent struct {
	// Destination ist das Ziel in Worten des Anwenders: ein Dateiname, ein
	// Datentraeger, der Name eines Programms. Es wird angezeigt, es steht
	// im Bericht, und es wird nicht geraten.
	Destination string
	// Confirmed ist das Ja, nachdem das Ziel angezeigt wurde.
	Confirmed bool
}

// Check prueft die Freigabe.
func (c CxfConsent) Check() error {
	if !c.Confirmed {
		return Fail("error.cxf.noconsent")
	}
	target := strings.TrimSpace(c.Destination)
	if target == "" || len(target) > MaxSiteLen || hasControl(target) {
		return Fail("error.cxf.nodestination")
	}
	return nil
}

// --- Was beim Ausgang mit welcher Art geschieht --------------------------------

// cxfKind sagt je Eintragsart, wie sie nach CXF kommt. Eine Tabelle und
// keine Kette von Abfragen: so steht die Entscheidung je Art an einer
// Stelle, und eine neue Art in items.go faellt hier als Luecke auf, statt
// still in einem else zu landen.
type cxfKind struct {
	// kind ist die Art in CXF. Leer: dieser Eintrag kommt nicht mit.
	kind string
	// flattened heisst: er kommt mit, aber weniger genau als er im Tresor
	// liegt. Das steht im Bericht; es ist kein Fehler, aber auch kein
	// vollstaendiger Uebergang.
	flattened bool
	// reason ist der Sprachschluessel des Grundes, wenn er nicht mitkommt.
	reason string
}

var cxfKinds = map[ItemType]cxfKind{
	ItemLogin:  {kind: CxfTypeBasicAuth},
	ItemTotp:   {kind: CxfTypeTotp},
	ItemSSHKey: {kind: CxfTypeSSHKey},
	ItemAPIKey: {kind: CxfTypeAPIKey},
	ItemNote:   {kind: CxfTypeNote},

	// Diese drei sind Text und kommen als Notiz mit. Das ist ein Verlust
	// an Genauigkeit - ein Wiederherstellungscode ist keine Notiz - und
	// darum steht es im Bericht.
	ItemRecovery: {kind: CxfTypeNote, flattened: true},
	ItemSeed:     {kind: CxfTypeNote, flattened: true},
	ItemLicense:  {kind: CxfTypeNote, flattened: true},

	// Diese Arten haben in CXF eine Entsprechung, die EINZELNE FELDER
	// verlangt - Kartennummer, Pruefziffer, Gueltigkeit, Sicherheitsart
	// des Funknetzes. Der Tresor haelt je Eintrag ein Geheimnis und keine
	// Felder. Sie hier aus dem Geheimnis zu RATEN waere erfunden, und
	// erfundene Felder in einer Austauschdatei sind schlimmer als eine
	// Zeile im Bericht.
	ItemCard:     {reason: "cxf.left.structured"},
	ItemIdentity: {reason: "cxf.left.structured"},
	ItemWifi:     {reason: "cxf.left.structured"},

	// Fuer diese drei hat CXF 1.0 keine Art. Eine zu erfinden hiesse, eine
	// Datei zu schreiben, die kein anderes Programm liest.
	ItemHotp:   {reason: "cxf.left.nokind"},
	ItemCert:   {reason: "cxf.left.nokind"},
	ItemPGPKey: {reason: "cxf.left.nokind"},

	// Eine Datei liegt in CXF nicht in der Datei selbst: die Art `file`
	// nennt nur Name, Groesse und Pruefsumme, der Inhalt geht ueber das
	// zweite Verfahren (CXP). Das ist hier nicht gebaut.
	ItemFile: {reason: "cxf.left.needscxp"},
}

// --- Der Bericht --------------------------------------------------------------

// CxfLine ist eine Zeile des Berichts: ein Eintrag und was mit ihm geschah.
type CxfLine struct {
	ID   uint32   `json:"id"`
	Name string   `json:"name"`
	Type ItemType `json:"type"`
	// ReasonKey ist der Sprachschluessel. Der fertige Satz entsteht erst in
	// der Sprache des Anwenders - hier steht kein deutscher Text, sonst
	// waere der Bericht in der Oberflaeche einsprachig.
	ReasonKey string `json:"reason,omitempty"`
}

// CxfReport sagt, was mitgekommen ist und was nicht. Er wird IMMER
// zurueckgegeben, auch wenn alles gut ging - wer nur bei Fehlern hinsieht,
// sieht die stillen Luecken nicht.
type CxfReport struct {
	// Destination ist das Ziel, das freigegeben wurde.
	Destination string `json:"destination"`
	// Taken sind die Eintraege, die vollstaendig mitgekommen sind.
	Taken []CxfLine `json:"taken"`
	// Flattened sind die, die mitgekommen sind, aber ungenauer.
	Flattened []CxfLine `json:"flattened"`
	// Left sind die, die NICHT mitgekommen sind. Diese Liste ist der Grund,
	// warum es den Bericht gibt.
	Left []CxfLine `json:"left"`
}

// Complete sagt, ob nichts zurueckgeblieben ist.
func (r CxfReport) Complete() bool { return len(r.Left) == 0 && len(r.Flattened) == 0 }

// SummaryKey nennt den Satz, der ueber dem Bericht steht.
func (r CxfReport) SummaryKey() string {
	if len(r.Left) > 0 {
		return "cxf.report.incomplete"
	}
	if len(r.Flattened) > 0 {
		return "cxf.report.flattened"
	}
	return "cxf.report.complete"
}

// --- Der Ausgang --------------------------------------------------------------

// CxfExport schreibt den Tresor eines Profils als CXF.
//
// Die Reihenfolge ist Absicht: erst die Freigabe pruefen, dann das Geraet
// fragen, ob es offen ist, dann Eintrag fuer Eintrag. Eine Datei, die ohne
// Freigabe halb entsteht, gibt es nicht.
func CxfExport(client *Client, profile uint8, consent CxfConsent) (*CxfHeader, CxfReport, error) {
	report := CxfReport{Destination: strings.TrimSpace(consent.Destination)}
	if err := consent.Check(); err != nil {
		return nil, report, err
	}
	if client == nil {
		return nil, report, Fail("error.guard.nodevice")
	}
	// Die PIN wird nicht hier geprueft, sondern dort, wo sie gilt. Ein
	// Merker in diesem Programm waere eine zweite Wahrheit; das Geraet
	// weiss, ob es offen ist.
	info, err := client.Hello()
	if err != nil {
		return nil, report, err
	}
	if !info.Unlocked {
		return nil, report, Fail("error.device.locked")
	}
	items, err := client.List(profile, "")
	if err != nil {
		return nil, report, err
	}
	account := CxfAccount{
		ID:          cxfAccountID(info.Serial, profile),
		Username:    cxfProfileName(profile),
		Email:       "",
		Collections: []CxfCollection{},
		Items:       []CxfItem{},
	}
	for _, meta := range items {
		entry, line, taken := cxfExportItem(client, meta)
		switch {
		case !taken:
			report.Left = append(report.Left, line)
		case line.ReasonKey != "":
			report.Flattened = append(report.Flattened, line)
			account.Items = append(account.Items, entry)
		default:
			report.Taken = append(report.Taken, line)
			account.Items = append(account.Items, entry)
		}
	}
	header := &CxfHeader{
		Version:             CxfVersion{Major: CxfVersionMajor, Minor: CxfVersionMinor},
		ExporterRpID:        CxfRpID,
		ExporterDisplayName: "OpenSecureKey",
		Timestamp:           uint64(time.Now().UTC().Unix()),
		Accounts:            []CxfAccount{account},
	}
	return header, report, nil
}

// cxfExportItem holt einen Eintrag. Der dritte Rueckgabewert sagt, ob er
// mitkommt; die Zeile wird in jedem Fall gefuellt.
func cxfExportItem(client *Client, meta Item) (CxfItem, CxfLine, bool) {
	line := CxfLine{ID: meta.ID, Name: meta.Name, Type: meta.Type}

	kind, known := cxfKinds[meta.Type]
	if !known {
		// Eine Art, die items.go kennt und diese Tabelle nicht. Dann ist
		// die Tabelle alt - und das ist eine Luecke, die genannt wird,
		// nicht eine, die zu einem Standardfall wird.
		line.ReasonKey = "cxf.left.nokind"
		return CxfItem{}, line, false
	}
	if kind.kind == "" {
		line.ReasonKey = kind.reason
		return CxfItem{}, line, false
	}

	// Ein Eintrag eines Profils, das nur auf diesem Geraet lebt, kommt
	// nicht heraus. Dieser Merker kommt vom Geraet (`m` in `list`), nicht
	// von einer Annahme dieses Programms - aber er ist nur der erste von
	// zwei Filtern: das letzte Wort hat die Absage des Geraets unten.
	// Zuerst zu fragen erspart dem Anwender Tastendruecke fuer Eintraege,
	// die ohnehin nicht herauskommen.
	if !meta.Mirrored {
		line.ReasonKey = "cxf.left.chipbound"
		return CxfItem{}, line, false
	}

	secret, err := client.Reveal(meta.ID)
	if err != nil {
		// DAS GERAET ENTSCHEIDET. Eine Absage ist kein Fehler des
		// Ausgangs und auch kein Grund, abzubrechen: sie ist die Antwort
		// "dieser Eintrag kommt nicht heraus", und sie steht im Bericht.
		// Ein Abbruch waere hier das Schlechteste - dann haette der
		// Anwender eine halbe Datei und keine Liste.
		line.ReasonKey = cxfRefusalKey(err)
		return CxfItem{}, line, false
	}
	defer Wipe(secret)

	credential, flattenedKey, err := cxfCredentialFor(meta, kind, secret)
	if err != nil {
		line.ReasonKey = cxfRefusalKey(err)
		return CxfItem{}, line, false
	}
	if kind.flattened {
		line.ReasonKey = "cxf.flat.asnote"
	}
	if flattenedKey != "" {
		line.ReasonKey = flattenedKey
	}

	entry := CxfItem{
		ID:          cxfItemID(meta.ID),
		Title:       meta.Name,
		Subtitle:    meta.Site,
		Credentials: CxfCredentials{credential},
	}
	if meta.ChangedUTC > 0 {
		entry.ModifiedAt = uint64(meta.ChangedUTC)
	}
	if meta.Site != "" {
		if scope := cxfScopeFor(meta.Site); scope != nil {
			entry.Scope = scope
		}
	}
	return entry, line, true
}

// cxfRefusalKey macht aus einem Fehler des Geraets den Grund im Bericht. Ein
// unbekannter Fehler wird NICHT zu "chipgebunden" verallgemeinert: dann
// stuende im Bericht eine Begruendung, die niemand gemessen hat.
func cxfRefusalKey(err error) string {
	var problem *Problem
	if errors.As(err, &problem) {
		switch problem.Key {
		case "error.device.denied":
			return "cxf.left.denied"
		case "error.device.unsupported":
			return "cxf.left.unsupported"
		case "error.agent.keyformat", "error.agent.keylocked":
			return "cxf.left.keyformat"
		}
		return problem.Key
	}
	return "cxf.left.failed"
}

// cxfCredentialFor baut das Zugangsdatum. Der zweite Rueckgabewert ist ein
// Sprachschluessel, wenn beim Umschreiben etwas ungenauer wurde als es im
// Tresor lag.
func cxfCredentialFor(meta Item, kind cxfKind, secret []byte) (CxfCredential, string, error) {
	switch kind.kind {
	case CxfTypeBasicAuth:
		return &CxfBasicAuth{
			Type:     CxfTypeBasicAuth,
			Username: cxfText(cxfUserFor(meta), "username"),
			Password: cxfSecret(string(secret), "password"),
		}, "", nil
	case CxfTypeNote:
		return &CxfNote{
			Type:    CxfTypeNote,
			Content: CxfField{FieldType: CxfFieldString, Value: string(secret), Label: meta.Name},
		}, "", nil
	case CxfTypeAPIKey:
		return &CxfAPIKey{
			Type:     CxfTypeAPIKey,
			Key:      cxfSecret(string(secret), "key"),
			Username: cxfText(cxfUserFor(meta), "username"),
			URL:      cxfText(meta.Site, "url"),
		}, "", nil
	case CxfTypeTotp:
		return cxfTotpFrom(meta, secret)
	case CxfTypeSSHKey:
		credential, err := cxfSSHFrom(meta, secret)
		return credential, "", err
	}
	return nil, "", Fail("error.cxf.credtype")
}

// cxfUserFor nennt den Benutzernamen. Der Tresor haelt keinen eigenen - er
// steht im Namen oder in der Adresse des Eintrags. Erfunden wird keiner:
// ein leeres Feld ist ehrlicher als ein geratenes.
func cxfUserFor(meta Item) string {
	if at := strings.IndexByte(meta.Name, '@'); at > 0 {
		return meta.Name
	}
	return ""
}

// cxfScopeFor macht aus der Adresse des Eintrags den Geltungsbereich.
//
// NUR wenn dort schon eine vollstaendige Adresse steht. Frueher setzte diese
// Funktion ein "https://" davor, wenn keines dastand - das sah hilfreich aus
// und war eine Erfindung: aus dem Eintrag "b.example" wurde der
// Geltungsbereich "https://b.example", und beim Wiedereinlesen wurde daraus
// die Adresse des Eintrags. Der Rundlauf Export-Import-Export war damit
// NICHT gleich, und die zweite Datei behauptete ein Verfahren, das niemand
// eingegeben hatte. Gefunden von TestCxfRundlaufExportImportExport.
//
// Aus einem Wort wie "Arbeit" wird also kein Geltungsbereich, und aus
// "b.example" auch nicht. Das Wort steht weiter im Untertitel, wo es
// hingehoert.
func cxfScopeFor(site string) *CxfScope {
	text := strings.TrimSpace(site)
	if !strings.Contains(text, "://") {
		return nil
	}
	parsed, err := url.Parse(text)
	if err != nil || parsed.Host == "" || !strings.Contains(parsed.Host, ".") {
		return nil
	}
	return &CxfScope{URLs: []string{parsed.Scheme + "://" + parsed.Host}, AndroidApps: []any{}}
}

// --- Einmalkennwort -----------------------------------------------------------

// Die Werte, die ein Einmalkennwort braucht und die der Tresor nicht haelt.
// Sie stehen in RFC 6238 und sind das, was jedes Programm nimmt, wenn nichts
// anderes dasteht.
const (
	cxfTotpPeriod = 30
	cxfTotpDigits = 6
	cxfTotpAlgo   = "sha1"
)

// cxfTotpFrom liest das Geheimnis eines Einmalkennworts.
//
// Zwei Formen kommen vor: eine vollstaendige Adresse otpauth://, in der
// Takt, Stellen und Verfahren drinstehen, und das nackte Geheimnis in
// Base32. Im zweiten Fall sind die drei Werte NICHT gespeichert, und die
// Datei bekommt die Werte aus RFC 6238. Das ist eine Annahme, keine
// Messung - darum kommt die Zeile in den Bericht. Wer sie uebergeht und am
// Ende Zahlen bekommt, die nicht passen, soll es vorher gelesen haben.
func cxfTotpFrom(meta Item, secret []byte) (CxfCredential, string, error) {
	text := strings.TrimSpace(string(secret))
	if strings.HasPrefix(strings.ToLower(text), "otpauth://") {
		credential, err := cxfTotpFromURI(meta, text)
		return credential, "", err
	}
	base := cxfCleanBase32(text)
	if base == "" {
		return nil, "", Fail("error.cxf.totpsecret")
	}
	return &CxfTotp{
		Type:      CxfTypeTotp,
		Secret:    base,
		Period:    cxfTotpPeriod,
		Digits:    cxfTotpDigits,
		Algorithm: cxfTotpAlgo,
		Issuer:    meta.Site,
	}, "cxf.flat.totpdefaults", nil
}

// cxfTotpFromURI liest die Adressform otpauth://totp/...
func cxfTotpFromURI(meta Item, text string) (CxfCredential, error) {
	parsed, err := url.Parse(text)
	if err != nil {
		return nil, Fail("error.cxf.totpsecret")
	}
	if !strings.EqualFold(parsed.Host, "totp") {
		// hotp hat in CXF 1.0 keine Art. Es wird nicht zu totp gemacht -
		// das waere ein Zaehler, der als Uhr ausgegeben wird.
		return nil, Fail("error.cxf.totponly")
	}
	query := parsed.Query()
	base := cxfCleanBase32(query.Get("secret"))
	if base == "" {
		return nil, Fail("error.cxf.totpsecret")
	}
	credential := &CxfTotp{
		Type:      CxfTypeTotp,
		Secret:    base,
		Period:    cxfTotpPeriod,
		Digits:    cxfTotpDigits,
		Algorithm: cxfTotpAlgo,
		Issuer:    query.Get("issuer"),
	}
	if credential.Issuer == "" {
		credential.Issuer = meta.Site
	}
	if value := query.Get("period"); value != "" {
		number, err := strconv.ParseUint(value, 10, 16)
		if err != nil || number == 0 {
			return nil, Fail("error.cxf.totpsecret")
		}
		credential.Period = uint16(number)
	}
	if value := query.Get("digits"); value != "" {
		number, err := strconv.ParseUint(value, 10, 16)
		if err != nil || number < 6 || number > 10 {
			return nil, Fail("error.cxf.totpsecret")
		}
		credential.Digits = uint16(number)
	}
	if value := query.Get("algorithm"); value != "" {
		// CXF schreibt die Namen klein: das Beispiel der Beschreibung
		// nennt "sha256". Adressen aus anderen Programmen schreiben
		// "SHA256" gross.
		lower := strings.ToLower(value)
		switch lower {
		case "sha1", "sha256", "sha512":
			credential.Algorithm = lower
		default:
			return nil, Fail("error.cxf.totpalgo", value)
		}
	}
	if user := query.Get("issuer"); user == "" && parsed.Path != "" {
		credential.Username = strings.TrimPrefix(parsed.Path, "/")
	}
	return credential, nil
}

// cxfCleanBase32 prueft das Geheimnis. Leerzeichen und Kleinschreibung
// kommen vor und werden geglaettet; ein Zeichen, das nicht zum Alphabet
// gehoert, ist ein Fehler und wird nicht weggeworfen.
func cxfCleanBase32(text string) string {
	cleaned := strings.ToUpper(strings.ReplaceAll(strings.TrimSpace(text), " ", ""))
	cleaned = strings.TrimRight(cleaned, "=")
	if cleaned == "" {
		return ""
	}
	if _, err := base32.StdEncoding.WithPadding(base32.NoPadding).DecodeString(cleaned); err != nil {
		return ""
	}
	return cleaned
}

// --- SSH-Schluessel -----------------------------------------------------------

// cxfSSHFrom schreibt einen SSH-Schluessel so, wie CXF ihn will: der private
// Teil als DER in der PKCS#8-Form, Base64 in der Adressschreibweise. Das
// Beispiel der Beschreibung zeigt genau das - und NICHT die Textform mit
// "BEGIN OPENSSH PRIVATE KEY", die im Tresor liegt.
//
// Gelesen wird mit ParseSSHKey aus agent_key.go, also mit dem Leser, der
// schon da ist. Ein zweiter waere eine zweite Wahrheit.
func cxfSSHFrom(meta Item, secret []byte) (CxfCredential, error) {
	key, err := ParseSSHKey(secret)
	if err != nil {
		return nil, err
	}
	defer key.Wipe()
	inner := key.cryptoKey()
	if inner == nil {
		return nil, Fail("error.agent.keytype", key.Algorithm)
	}
	der, err := x509.MarshalPKCS8PrivateKey(inner)
	if err != nil {
		return nil, Fail("error.agent.keytype", key.Algorithm)
	}
	return &CxfSSHKey{
		Type:       CxfTypeSSHKey,
		KeyType:    key.Algorithm,
		PrivateKey: der,
		KeyComment: meta.Name,
	}, nil
}

// cryptoKey gibt den privaten Schluessel in der Form heraus, die
// crypto/x509 zum Schreiben braucht.
//
// Dass es diese Frage gibt, ist eine Ausnahme und keine Kehrtwende: dieser
// Schluessel ist nicht im Chip entstanden, sondern vom Anwender in den
// Tresor gelegt worden, und `reveal` hat ihn gerade herausgegeben. Fuer
// einen Schluessel aus dem Chip gibt es diese Frage nicht - dort rechnet das
// Geraet, siehe FileIdentity in file_tool.go, das absichtlich kein
// Private() hat.
func (k *SSHKey) cryptoKey() any {
	switch {
	case k.ed25519Key != nil:
		return k.ed25519Key
	case k.ecdsaKey != nil:
		return k.ecdsaKey
	case k.rsaKey != nil:
		return k.rsaKey
	}
	return nil
}

// --- Kennungen ----------------------------------------------------------------

// cxfItemID macht aus der Eintragsnummer eine Kennung. CXF will Rohdaten,
// kein Zahlwort; vier Byte in der Reihenfolge des Netzes sind eindeutig und
// kurz.
func cxfItemID(id uint32) CxfBytes {
	return CxfBytes{byte(id >> 24), byte(id >> 16), byte(id >> 8), byte(id)}
}

// cxfAccountID macht aus Seriennummer und Profil eine Kennung. Ueber eine
// Pruefsumme, damit die Seriennummer des Geraets nicht im Klartext in einer
// Datei steht, die weitergegeben wird - sie ist kein Geheimnis, aber sie ist
// auch nicht nötig.
func cxfAccountID(serial string, profile uint8) CxfBytes {
	sum := sha256.Sum256([]byte("opensecurekey/cxf/account/1\x00" + serial + "\x00" +
		strconv.FormatUint(uint64(profile), 10)))
	return CxfBytes(sum[:16])
}

// cxfProfileName nennt das Profil. Kein Name des Anwenders: der steht nicht
// im Tresor, und ihn zu erfinden brachte eine Angabe in die Datei, die
// niemand eingegeben hat.
func cxfProfileName(profile uint8) string {
	return "profile-" + strconv.FormatUint(uint64(profile), 10)
}
