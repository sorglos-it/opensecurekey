// Das Austauschformat: CXF (Credential Exchange Format) der FIDO Alliance.
//
// Beschrieben in der Fassung "Proposed Standard" vom 14.08.2025, mit der
// Nachtragsliste vom 09.03.2026. Hier nach der Beschreibung gebaut, nicht
// aus einem fremden Paket uebernommen - wie beim Dateiwerkzeug, und aus dem
// gleichen Grund: apps/desktop/go.mod hat keine Abhaengigkeiten.
//
// WOZU das Format da ist: ein Weg hinein aus fremden Passwortverwaltungen
// und ein Weg hinaus. Beide Richtungen aus einer Beschreibung, denn ein
// Format, das nur dieses Programm liest, ist kein Austausch.
//
// WAS ES BEDEUTET, dass es einen Weg hinaus gibt - und das ist die Stelle,
// an der dieses Geraet eine Zusage anfassen muss:
//
//	"Schluessel verlassen den Chip nie" gilt weiter. CXF widerspricht dem
//	nicht, es macht die Grenze nur sichtbar. Was ein Ausgang ausgeben kann,
//	ist genau das, was `reveal` ohnehin herausgibt: Geheimnisse eines
//	gespiegelten Profils, eines nach dem anderen, jedes mit PIN und
//	Tastendruck. Was im Chip entstanden ist und ihn nicht verlassen kann,
//	kommt nicht mit - siehe exchange_out.go, und es wird GENANNT, nicht
//	weggelassen.
//
// Die JSON-Namen in den Strukturen hier sind die aus der Beschreibung und
// nicht umbenannt. Ein eigener Name waere bequemer zu lesen und wertlos:
// das Format ist der Zweck der Uebung.
package classes

import (
	"encoding/base64"
	"encoding/json"
	"strings"
)

// Die Fassung des Formats, die dieses Programm schreibt. Steht einmal hier.
const (
	CxfVersionMajor = 1
	CxfVersionMinor = 0
)

// CxfRpID ist die Kennung, unter der dieses Programm als Ausgeber auftritt.
// Die Beschreibung verlangt dafuer einen Namen in Punktschreibweise; er
// steht auch in eigenen Erweiterungsnamen, darum nur an dieser Stelle.
const CxfRpID = "opensecurekey.org"

// Obergrenzen beim Lesen einer fremden Datei. Eine Importdatei ist eine
// fremde Eingabe, und ein Leser ohne Obergrenzen ist die Stelle, an der ein
// praeparierte Datei das Programm zum Stillstand bringt - nicht durch einen
// Fehler im Rechnen, sondern durch Speicher.
const (
	// CxfMaxFileLen ist die groesste Datei, die gelesen wird.
	CxfMaxFileLen = 64 << 20
	// CxfMaxItems ist die hoechste Zahl von Eintraegen in einer Datei.
	CxfMaxItems = 100000
	// CxfMaxTextLen ist die groesste Laenge eines einzelnen Textfeldes.
	CxfMaxTextLen = 64 << 10
	// CxfMaxDepth ist die tiefste Schachtelung von Sammlungen. Die
	// Beschreibung erlaubt Sammlungen in Sammlungen ohne Grenze; eine
	// Datei, die sich tausendfach schachtelt, soll hier anstossen und
	// nicht im Stapelspeicher.
	CxfMaxDepth = 16
	// CxfMaxIDLen ist die groesste Laenge einer Kennung in Byte, vor dem
	// Umschreiben in Base64. Die Beschreibung nennt 64 fuer die Kennung
	// eines Feldes; dieselbe Grenze gilt hier fuer alle Kennungen.
	CxfMaxIDLen = 64
)

// --- Base64 nach RFC 4648, Schreibweise fuer Adressen -------------------------

// CxfBytes sind Rohdaten, die in JSON als Base64 in der Adressschreibweise
// stehen. Die Beschreibung nennt den Typ `b64url`.
//
// OHNE Auffuellzeichen: das Beispiel der Nachtragsliste enthaelt kein
// einziges '=' - auch dort nicht, wo die Laenge eines auffordert
// ("MTIz" sind drei Byte). Geschrieben wird darum ohne, GELESEN werden
// beide Schreibweisen. Eine Datei abzuweisen, weil der andere auffuellt,
// waere Strenge an der falschen Stelle.
type CxfBytes []byte

func (b CxfBytes) MarshalJSON() ([]byte, error) {
	return json.Marshal(base64.RawURLEncoding.EncodeToString(b))
}

func (b *CxfBytes) UnmarshalJSON(data []byte) error {
	var text string
	if err := json.Unmarshal(data, &text); err != nil {
		return Fail("error.cxf.base64")
	}
	raw, err := cxfDecodeBase64(text)
	if err != nil {
		return err
	}
	*b = raw
	return nil
}

// cxfDecodeBase64 liest beide Schreibweisen. Das normale Alphabet (+ und /)
// wird NICHT angenommen: es ist ein anderes Alphabet, und es stillschweigend
// mitzulesen hiesse, eine Datei anzunehmen, die das Format nicht erfuellt.
func cxfDecodeBase64(text string) ([]byte, error) {
	if len(text) > 4*(CxfMaxTextLen/3+4) {
		return nil, Fail("error.cxf.toobig")
	}
	if strings.ContainsAny(text, "+/") {
		return nil, Fail("error.cxf.base64")
	}
	coding := base64.RawURLEncoding
	if strings.HasSuffix(text, "=") {
		coding = base64.URLEncoding
	}
	raw, err := coding.DecodeString(text)
	if err != nil {
		return nil, Fail("error.cxf.base64")
	}
	return raw, nil
}

// --- Der Aufbau einer Datei ---------------------------------------------------

// CxfVersion ist die Fassung des Formats.
type CxfVersion struct {
	Major uint8 `json:"major"`
	Minor uint8 `json:"minor"`
}

// CxfHeader ist die ganze Datei. Die Beschreibung nennt sie "Header", obwohl
// alles darin haengt - der Name bleibt, damit er mit der Beschreibung
// zusammengelesen werden kann.
type CxfHeader struct {
	Version CxfVersion `json:"version"`
	// ExporterRpID nennt, WER die Datei geschrieben hat. Das ist auch die
	// Angabe, die ein Anwender beim Einlesen sehen soll.
	ExporterRpID        string       `json:"exporterRpId"`
	ExporterDisplayName string       `json:"exporterDisplayName"`
	Timestamp           uint64       `json:"timestamp"`
	Accounts            []CxfAccount `json:"accounts"`
}

// CxfAccount ist ein Konto der fremden Verwaltung. Dieses Geraet hat
// Profile; ein Profil wird zu einem Konto.
type CxfAccount struct {
	ID          CxfBytes        `json:"id"`
	Username    string          `json:"username"`
	Email       string          `json:"email"`
	FullName    string          `json:"fullName,omitempty"`
	Collections []CxfCollection `json:"collections"`
	Items       []CxfItem       `json:"items"`
}

// CxfCollection ist eine Gruppe. Sie verweist auf Eintraege, sie enthaelt
// sie nicht - darum CxfLinkedItem.
type CxfCollection struct {
	ID             CxfBytes        `json:"id"`
	CreationAt     uint64          `json:"creationAt,omitempty"`
	ModifiedAt     uint64          `json:"modifiedAt,omitempty"`
	Title          string          `json:"title"`
	Subtitle       string          `json:"subtitle,omitempty"`
	Items          []CxfLinkedItem `json:"items"`
	SubCollections []CxfCollection `json:"subCollections,omitempty"`
}

// CxfLinkedItem ist ein Verweis auf einen Eintrag.
type CxfLinkedItem struct {
	Item    CxfBytes `json:"item"`
	Account CxfBytes `json:"account,omitempty"`
}

// CxfScope sagt, wo ein Eintrag gilt.
type CxfScope struct {
	URLs        []string `json:"urls"`
	AndroidApps []any    `json:"androidApps"`
}

// CxfItem ist ein Eintrag. Er traegt mehrere Zugangsdaten: ein Login mit
// Einmalkennwort sind zwei Zugangsdaten in einem Eintrag, und so steht es
// auch im Beispiel der Beschreibung.
type CxfItem struct {
	ID          CxfBytes       `json:"id"`
	CreationAt  uint64         `json:"creationAt,omitempty"`
	ModifiedAt  uint64         `json:"modifiedAt,omitempty"`
	Title       string         `json:"title"`
	Subtitle    string         `json:"subtitle,omitempty"`
	Favorite    bool           `json:"favorite,omitempty"`
	Scope       *CxfScope      `json:"scope,omitempty"`
	Credentials CxfCredentials `json:"credentials"`
	Tags        []string       `json:"tags,omitempty"`
}

// --- Felder -------------------------------------------------------------------

// Die Feldarten der Beschreibung. Nur diese zehn gibt es; eine unbekannte
// Art wird wie "string" behandelt, so steht es dort ausdruecklich.
const (
	CxfFieldString    = "string"
	CxfFieldConcealed = "concealed-string"
	CxfFieldEmail     = "email"
	CxfFieldNumber    = "number"
	CxfFieldBoolean   = "boolean"
	CxfFieldDate      = "date"
	CxfFieldYearMonth = "year-month"
	CxfFieldWifiType  = "wifi-network-security-type"
	CxfFieldCountry   = "country-code"
	CxfFieldSubdiv    = "subdivision-code"
)

// CxfField ist ein Feld, das der Anwender aendern kann. Der Wert ist IMMER
// ein Text, auch bei Zahl und Wahrheitswert - im Beispiel der Beschreibung
// steht der Wahrheitswert als "false" in Anfuehrungszeichen. Wer hier eine
// Zahl schreibt, baut eine Datei, die andere Programme abweisen.
type CxfField struct {
	ID        CxfBytes `json:"id,omitempty"`
	FieldType string   `json:"fieldType"`
	Value     string   `json:"value"`
	Label     string   `json:"label,omitempty"`
}

// cxfText baut ein offenes Feld.
func cxfText(value, label string) *CxfField {
	return &CxfField{FieldType: CxfFieldString, Value: value, Label: label}
}

// cxfSecret baut ein verdecktes Feld. Die Art steht nicht zur Zierde da:
// ein Programm, das die Datei einliest, entscheidet daran, ob es den Wert
// anzeigt oder abdeckt.
func cxfSecret(value, label string) *CxfField {
	return &CxfField{FieldType: CxfFieldConcealed, Value: value, Label: label}
}

// --- Zugangsdaten -------------------------------------------------------------

// Die Namen der Arten, genau die siebzehn aus der Beschreibung. Dieses
// Programm schreibt nur einen Teil davon und liest alle - eine Art, die es
// nicht ablegen kann, wird beim Einlesen GENANNT und nicht uebergangen.
const (
	CxfTypeAddress      = "address"
	CxfTypeAPIKey       = "api-key"
	CxfTypeBasicAuth    = "basic-auth"
	CxfTypeCreditCard   = "credit-card"
	CxfTypeCustomFields = "custom-fields"
	CxfTypeDriversLic   = "drivers-license"
	CxfTypeFile         = "file"
	CxfTypeGenPassword  = "generated-password"
	CxfTypeIdentityDoc  = "identity-document"
	CxfTypeItemRef      = "item-reference"
	CxfTypeNote         = "note"
	CxfTypePasskey      = "passkey"
	CxfTypePassport     = "passport"
	CxfTypePersonName   = "person-name"
	CxfTypeSSHKey       = "ssh-key"
	CxfTypeTotp         = "totp"
	CxfTypeWifi         = "wifi"
)

// CxfCredential ist ein Zugangsdatum. Die Beschreibung macht daraus eine
// Auswahl, die am Feld `type` haengt; in Go ist das eine Schnittstelle mit
// einer Frage.
type CxfCredential interface {
	// CxfType nennt die Art, so wie sie in der Datei steht.
	CxfType() string
}

// CxfBasicAuth ist Benutzername und Kennwort.
type CxfBasicAuth struct {
	Type     string    `json:"type"`
	Username *CxfField `json:"username,omitempty"`
	Password *CxfField `json:"password,omitempty"`
}

func (c *CxfBasicAuth) CxfType() string { return CxfTypeBasicAuth }

// CxfGenPassword ist ein maschinell erzeugtes Kennwort - ein nackter Text,
// kein Feld. Das ist in der Beschreibung so und sieht wie ein Versehen aus,
// ist aber keins: es gibt daran nichts zu beschriften.
type CxfGenPassword struct {
	Type     string `json:"type"`
	Password string `json:"password"`
}

func (c *CxfGenPassword) CxfType() string { return CxfTypeGenPassword }

// CxfTotp ist ein zeitbasiertes Einmalkennwort.
type CxfTotp struct {
	Type      string `json:"type"`
	Secret    string `json:"secret"`
	Period    uint16 `json:"period"`
	Digits    uint16 `json:"digits"`
	Username  string `json:"username,omitempty"`
	Algorithm string `json:"algorithm"`
	Issuer    string `json:"issuer,omitempty"`
}

func (c *CxfTotp) CxfType() string { return CxfTypeTotp }

// CxfSSHKey ist ein SSH-Schluesselpaar. `privateKey` ist Base64 in der
// Adressschreibweise ueber der DER-Form - nicht die Textform mit
// "BEGIN OPENSSH PRIVATE KEY", wie das Beispiel der Beschreibung zeigt.
type CxfSSHKey struct {
	Type                string    `json:"type"`
	KeyType             string    `json:"keyType"`
	PrivateKey          CxfBytes  `json:"privateKey"`
	KeyComment          string    `json:"keyComment,omitempty"`
	CreationDate        *CxfField `json:"creationDate,omitempty"`
	ExpiryDate          *CxfField `json:"expiryDate,omitempty"`
	KeyGenerationSource *CxfField `json:"keyGenerationSource,omitempty"`
}

func (c *CxfSSHKey) CxfType() string { return CxfTypeSSHKey }

// CxfNote ist eine Notiz.
type CxfNote struct {
	Type    string   `json:"type"`
	Content CxfField `json:"content"`
}

func (c *CxfNote) CxfType() string { return CxfTypeNote }

// CxfAPIKey ist ein Programmschluessel.
type CxfAPIKey struct {
	Type       string    `json:"type"`
	Key        *CxfField `json:"key,omitempty"`
	Username   *CxfField `json:"username,omitempty"`
	KeyType    *CxfField `json:"keyType,omitempty"`
	URL        *CxfField `json:"url,omitempty"`
	ValidFrom  *CxfField `json:"validFrom,omitempty"`
	ExpiryDate *CxfField `json:"expiryDate,omitempty"`
}

func (c *CxfAPIKey) CxfType() string { return CxfTypeAPIKey }

// CxfPasskey ist ein Passkey.
//
// Dieses Programm kann einen Passkey LESEN und wieder schreiben, aber es
// kann ihn nicht in den Tresor legen: die Eintragsarten in items.go kommen
// aus apps/firmware/include/osk_store.h, und dort gibt es keine fuer
// Passkeys. Eine hier zu erfinden hiesse, eine Zahl an einer zweiten Stelle
// zu fuehren - genau das, was dieses Projekt nicht tut. Der Eintrag wird
// beim Einlesen also GENANNT und nicht abgelegt; siehe docs/cxf.md.
//
// `fido2Extensions` wird als rohes JSON durchgetragen. Die Nachtragsliste
// vom 09.03.2026 hat dort Namen geaendert (hmacSecret ist weg,
// hmacCredentials ist neu). Was dieses Programm nicht auswertet, soll es
// auch nicht umschreiben: ein Rundlauf verliert sonst Felder, die ein
// anderes Programm braucht.
type CxfPasskey struct {
	Type            string          `json:"type"`
	CredentialID    CxfBytes        `json:"credentialId"`
	RpID            string          `json:"rpId"`
	Username        string          `json:"username"`
	UserDisplayName string          `json:"userDisplayName"`
	UserHandle      CxfBytes        `json:"userHandle"`
	Key             CxfBytes        `json:"key"`
	Fido2Extensions json.RawMessage `json:"fido2Extensions,omitempty"`
}

func (c *CxfPasskey) CxfType() string { return CxfTypePasskey }

// CxfOther ist eine Art, die dieses Programm nicht auseinandernimmt:
// Adresse, Karte, Ausweis, Datei und die uebrigen. Der Inhalt bleibt rohes
// JSON.
//
// WARUM nicht einfach weglassen: ein Rundlauf durch dieses Programm darf
// keine Zugangsdaten verlieren, auch keine, die der Tresor nicht halten
// kann. Was hier durchliegt, kommt beim Schreiben unveraendert wieder
// heraus, und was der Tresor nicht halten kann, steht im Bericht.
type CxfOther struct {
	Name string
	Raw  json.RawMessage
}

func (c *CxfOther) CxfType() string { return c.Name }

func (c *CxfOther) MarshalJSON() ([]byte, error) { return c.Raw, nil }

// CxfCredentials ist die Liste der Zugangsdaten eines Eintrags. Sie braucht
// eigenes Lesen, weil die Art erst im Feld `type` steht.
type CxfCredentials []CxfCredential

func (list *CxfCredentials) UnmarshalJSON(data []byte) error {
	var raw []json.RawMessage
	if err := json.Unmarshal(data, &raw); err != nil {
		return Fail("error.cxf.format")
	}
	out := make(CxfCredentials, 0, len(raw))
	for _, entry := range raw {
		credential, err := cxfReadCredential(entry)
		if err != nil {
			return err
		}
		out = append(out, credential)
	}
	*list = out
	return nil
}

// cxfReadCredential liest ein Zugangsdatum. Eine Art ohne `type` ist ein
// Fehler und kein leerer Eintrag: eine Datei, in der die Art fehlt, ist
// nicht halb gueltig.
func cxfReadCredential(data json.RawMessage) (CxfCredential, error) {
	var head struct {
		Type string `json:"type"`
	}
	if err := json.Unmarshal(data, &head); err != nil {
		return nil, Fail("error.cxf.format")
	}
	if head.Type == "" {
		return nil, Fail("error.cxf.credtype")
	}
	switch head.Type {
	case CxfTypeBasicAuth:
		return cxfInto(data, &CxfBasicAuth{})
	case CxfTypeGenPassword:
		return cxfInto(data, &CxfGenPassword{})
	case CxfTypeTotp:
		return cxfInto(data, &CxfTotp{})
	case CxfTypeSSHKey:
		return cxfInto(data, &CxfSSHKey{})
	case CxfTypeNote:
		return cxfInto(data, &CxfNote{})
	case CxfTypeAPIKey:
		return cxfInto(data, &CxfAPIKey{})
	case CxfTypePasskey:
		return cxfInto(data, &CxfPasskey{})
	}
	// Alles Weitere bleibt, wie es ist. Ein Name, den die Beschreibung
	// nicht kennt, wird nicht abgewiesen: das Format erlaubt Erweiterungen,
	// und eine unbekannte Art ist kein verfaelschtes Feld.
	return &CxfOther{Name: head.Type, Raw: append(json.RawMessage(nil), data...)}, nil
}

// cxfInto liest in die Struktur der Art. Unbekannte Felder sind ein Fehler:
// bei einer Art, die dieses Programm auseinandernimmt, bedeutet ein Feld,
// das es nicht kennt, dass es sie nicht vollstaendig kennt - und dann soll
// es die Datei nicht halb verstehen.
func cxfInto[T CxfCredential](data json.RawMessage, target T) (CxfCredential, error) {
	decoder := json.NewDecoder(strings.NewReader(string(data)))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(target); err != nil {
		return nil, Fail("error.cxf.credfields", target.CxfType())
	}
	return target, nil
}
