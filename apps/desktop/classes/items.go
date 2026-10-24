// Die Eintragstypen des Tresors. Die Zahlen sind dieselben wie in
// apps/firmware/include/osk_store.h (osk_item_type_t) - werden sie dort
// geaendert, muss diese Liste mitgehen.
package classes

import "strings"

// ItemType ist der Typ eines Eintrags.
type ItemType uint8

// Die Typen in der Reihenfolge des Headers.
const (
	ItemLogin ItemType = iota + 1
	ItemTotp
	ItemHotp
	ItemSSHKey
	ItemCert
	ItemPGPKey
	ItemAPIKey
	ItemRecovery
	ItemSeed
	ItemLicense
	ItemCard
	ItemIdentity
	ItemWifi
	ItemNote
	ItemFile
)

// itemKeys nennt je Typ den Schluessel der Sprachdatei.
var itemKeys = map[ItemType]string{
	ItemLogin:    "item.type.login",
	ItemTotp:     "item.type.totp",
	ItemHotp:     "item.type.hotp",
	ItemSSHKey:   "item.type.sshkey",
	ItemCert:     "item.type.cert",
	ItemPGPKey:   "item.type.pgpkey",
	ItemAPIKey:   "item.type.apikey",
	ItemRecovery: "item.type.recovery",
	ItemSeed:     "item.type.seed",
	ItemLicense:  "item.type.license",
	ItemCard:     "item.type.card",
	ItemIdentity: "item.type.identity",
	ItemWifi:     "item.type.wifi",
	ItemNote:     "item.type.note",
	ItemFile:     "item.type.file",
}

// ItemTypes gibt alle Typen in fester Reihenfolge zurueck.
func ItemTypes() []ItemType {
	out := make([]ItemType, 0, len(itemKeys))
	for t := ItemLogin; t <= ItemFile; t++ {
		out = append(out, t)
	}
	return out
}

// Key ist der Sprachschluessel des Typs.
func (t ItemType) Key() string {
	if key, ok := itemKeys[t]; ok {
		return key
	}
	return "item.type.unknown"
}

// Valid sagt, ob der Typ zum Header passt.
func (t ItemType) Valid() bool {
	_, ok := itemKeys[t]
	return ok
}

// Item sind die Angaben, die ohne Freigabe sichtbar sein duerfen. Das
// Geheimnis selbst steht hier nie drin - dafuer gibt es Reveal.
type Item struct {
	ID             uint32   `json:"id"`
	Type           ItemType `json:"type"`
	Profile        uint8    `json:"profile"`
	Name           string   `json:"name"`
	Site           string   `json:"site"`
	ChangedUTC     int64    `json:"changed"`
	Mirrored       bool     `json:"mirrored"`
	PairedHostOnly bool     `json:"pairedHostOnly"`

	// Der oeffentliche Teil, bei SSH-Schluesseln, Zertifikaten und
	// PGP-Schluesseln. Er ist KEIN Geheimnis: das Geraet gibt ihn ohne
	// Freigabe heraus, und wer ihn wie ein Geheimnis behandelt, kostet nur
	// Tastendruecke. Leer, wenn der Typ keinen hat oder das Geraet ein
	// aelteres Protokoll spricht.
	Pub []byte `json:"pub,omitempty"`
}

// Obergrenzen fuer Eingaben. Sie gelten in Oberflaeche und Kommandozeile und
// muessen mit dem Geraet uebereinstimmen: OSK_NAME_MAX und OSK_SITE_MAX in
// apps/firmware/src/classes/store/osk_vault_format.h, dort 64 und 128.
//
// Sie standen frueher auf 128/256. Damit nahm das Programm Eintraege an, die
// das Geraet abgelehnt haette - der Fehler waere erst beim Speichern
// aufgefallen, nach dem Tippen und nach der Freigabe. Das Geraet entscheidet,
// weil der Indexsatz im Flash die Laenge vorgibt.
const (
	MaxNameLen   = 64
	MaxSiteLen   = 128
	MaxSecretLen = 2048
)

// CheckItem prueft einen Eintrag, bevor er zum Geraet geht. Das Geraet prueft
// noch einmal selbst - hier geht es darum, dem Anwender frueh zu antworten.
func CheckItem(it Item) error {
	if !it.Type.Valid() {
		return Fail("error.item.type")
	}
	name := strings.TrimSpace(it.Name)
	if name == "" || len(name) > MaxNameLen || hasControl(name) {
		return Fail("error.item.name")
	}
	if len(it.Site) > MaxSiteLen || hasControl(it.Site) {
		return Fail("error.item.site")
	}
	return nil
}

// hasControl findet Steuerzeichen - die haben in Namen nichts zu suchen.
func hasControl(text string) bool {
	for _, r := range text {
		if r < 0x20 || r == 0x7f {
			return true
		}
	}
	return false
}

// Wipe ueberschreibt einen Puffer. Jedes Geheimnis geht hier durch, sobald es
// nicht mehr gebraucht wird.
func Wipe(buf []byte) {
	for i := range buf {
		buf[i] = 0
	}
}
