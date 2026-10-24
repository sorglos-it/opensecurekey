// Lizenzen im Tresor. Eine Lizenz sagt, bis wann, mit welchen Ausbaustufen
// und auf wie vielen Plaetzen ein Programm laufen darf. Der Hersteller
// unterschreibt sie, das Programm prueft sie - das Geraet traegt sie nur.
//
// Dass die Lizenz auf dem Stick liegt, bindet sie an ein Stueck Hardware:
// kopieren laesst sich die Datei, der Stick nicht. Faelschen laesst sie sich
// nicht, weil der private Schluessel des Herstellers nirgends mitgeliefert
// wird. Was bleibt: wer das Programm umschreibt, kann die Pruefung
// herausnehmen - siehe docs/dongle.md, Stufe 1 gegen Stufe 2.
package classes

import (
	"crypto/ed25519"
	"encoding/binary"
	"strings"
	"time"
)

// Aufbau einer Lizenz: Kopf, Nutzlast in CBOR, Unterschrift.
const (
	licenseMagic   = "OSKLIC1"
	licenseVersion = 1
	licenseMaxLoad = 1024
	licenseHeadLen = len(licenseMagic) + 1 + 2
)

// License ist eine geprüfte Lizenz. Alle Felder stehen so in der
// unterschriebenen Nutzlast - nichts davon ist eine Annahme des Programms.
type License struct {
	Product    string   `json:"product"`
	Holder     string   `json:"holder"`
	Number     string   `json:"number"`
	Tiers      []string `json:"tiers"`
	Seats      uint32   `json:"seats"`
	IssuedUTC  int64    `json:"issued"`
	ExpiresUTC int64    `json:"expires"` // 0: laeuft nicht ab
}

// Expired sagt, ob die Lizenz zum Zeitpunkt now abgelaufen ist.
func (l License) Expired(now time.Time) bool {
	return l.ExpiresUTC != 0 && now.UTC().Unix() > l.ExpiresUTC
}

// HasTier sagt, ob eine Ausbaustufe enthalten ist.
func (l License) HasTier(name string) bool {
	for _, have := range l.Tiers {
		if strings.EqualFold(have, name) {
			return true
		}
	}
	return false
}

// LicenseEncode baut die Nutzlast ohne Unterschrift. Der Hersteller
// unterschreibt genau diese Bytes.
func LicenseEncode(l License) ([]byte, error) {
	if l.Product == "" || len(l.Product) > MaxNameLen || hasControl(l.Product) {
		return nil, Fail("error.license.product")
	}
	if len(l.Holder) > MaxNameLen || hasControl(l.Holder) {
		return nil, Fail("error.license.holder")
	}
	if len(l.Number) > MaxNameLen || hasControl(l.Number) {
		return nil, Fail("error.license.number")
	}
	tiers := make([]any, 0, len(l.Tiers))
	for _, tier := range l.Tiers {
		if tier == "" || len(tier) > MaxNameLen || hasControl(tier) {
			return nil, Fail("error.license.tier")
		}
		tiers = append(tiers, tier)
	}
	if l.IssuedUTC < 0 || l.ExpiresUTC < 0 {
		return nil, Fail("error.license.time")
	}
	load, err := CborEncode(CborMap{
		"p":  l.Product,
		"h":  l.Holder,
		"nr": l.Number,
		"t":  tiers,
		"s":  uint64(l.Seats),
		"i":  uint64(l.IssuedUTC),
		"e":  uint64(l.ExpiresUTC),
	})
	if err != nil {
		return nil, Fail("error.license.format")
	}
	if len(load) > licenseMaxLoad {
		return nil, Fail("error.license.size")
	}
	return load, nil
}

// LicenseSign macht aus Angaben und dem privaten Schluessel des Herstellers
// die Datei, die in den Tresor kommt. Das laeuft beim Hersteller, nicht beim
// Kunden - der private Schluessel gehoert nicht ausgeliefert.
func LicenseSign(l License, issuerPrivate ed25519.PrivateKey) ([]byte, error) {
	if len(issuerPrivate) != ed25519.PrivateKeySize {
		return nil, Fail("error.license.issuerkey")
	}
	load, err := LicenseEncode(l)
	if err != nil {
		return nil, err
	}
	out := make([]byte, 0, licenseHeadLen+len(load)+GuardSigLen)
	out = append(out, licenseMagic...)
	out = append(out, licenseVersion)
	var size [2]byte
	binary.BigEndian.PutUint16(size[:], uint16(len(load)))
	out = append(out, size[:]...)
	out = append(out, load...)
	return append(out, ed25519.Sign(issuerPrivate, licenseSigned(load))...), nil
}

// licenseSigned ist, was wirklich unterschrieben wird: Zweck davor, damit
// eine Lizenz nicht als Antwort auf eine Frage oder als Auftrag durchgeht.
func licenseSigned(load []byte) []byte {
	out := make([]byte, 0, len(licenseMagic)+1+len(load))
	out = append(out, licenseMagic...)
	out = append(out, 0)
	return append(out, load...)
}

// LicenseVerify prueft Unterschrift und Aufbau. Ob die Lizenz abgelaufen
// ist, entscheidet der Aufrufer mit Expired - das ist eine Frage der Uhr,
// nicht der Unterschrift, und beides darf man nicht verwechseln.
func LicenseVerify(issuerPublic, blob []byte) (License, error) {
	if len(issuerPublic) != GuardKeyLen {
		return License{}, Fail("error.guard.issuer")
	}
	if len(blob) < licenseHeadLen+GuardSigLen {
		return License{}, Fail("error.license.format")
	}
	if string(blob[:len(licenseMagic)]) != licenseMagic {
		return License{}, Fail("error.license.format")
	}
	if blob[len(licenseMagic)] != licenseVersion {
		return License{}, Fail("error.license.version")
	}
	size := int(binary.BigEndian.Uint16(blob[len(licenseMagic)+1:]))
	if size == 0 || size > licenseMaxLoad ||
		licenseHeadLen+size+GuardSigLen != len(blob) {
		return License{}, Fail("error.license.format")
	}
	load := blob[licenseHeadLen : licenseHeadLen+size]
	signature := blob[licenseHeadLen+size:]
	if !ed25519.Verify(ed25519.PublicKey(issuerPublic), licenseSigned(load), signature) {
		return License{}, Fail("error.license.signature")
	}
	return licenseDecode(load)
}

func licenseDecode(load []byte) (License, error) {
	value, err := CborDecode(load)
	if err != nil {
		return License{}, Fail("error.license.format")
	}
	record, ok := value.(CborMap)
	if !ok {
		return License{}, Fail("error.license.format")
	}
	out := License{}
	out.Product, _ = CborText(record, "p")
	if out.Product == "" {
		return License{}, Fail("error.license.product")
	}
	out.Holder, _ = CborText(record, "h")
	out.Number, _ = CborText(record, "nr")
	if seats, found := CborUint(record, "s"); found && seats <= 0xffffffff {
		out.Seats = uint32(seats)
	}
	if issued, found := CborUint(record, "i"); found && issued <= 1<<62 {
		out.IssuedUTC = int64(issued)
	}
	if expires, found := CborUint(record, "e"); found && expires <= 1<<62 {
		out.ExpiresUTC = int64(expires)
	}
	if list, found := CborList(record, "t"); found {
		for _, entry := range list {
			name, isText := entry.(string)
			if !isText || name == "" {
				return License{}, Fail("error.license.tier")
			}
			out.Tiers = append(out.Tiers, name)
		}
	}
	return out, nil
}

// License holt die Lizenz fuer ein Produkt aus dem Tresor und prueft sie.
// Dafuer muss der Tresor offen sein, und das Geraet protokolliert den
// Zugriff wie jedes andere Geheimnis.
func (g *Guard) License(product string) (License, error) {
	license, _, err := g.LicenseBlob(product)
	return license, err
}

// LicenseBlob gibt zusaetzlich den unterschriebenen Block heraus, so wie er
// im Tresor liegt.
//
// Das braucht der lokale Dienst: eine Bibliothek, die nur die fertig
// gelesenen Felder bekaeme, muesste dem Dienst glauben - und ein falscher
// Dienst auf der Rueckschleife koennte Lizenzen erfinden, ohne das
// geschuetzte Programm anzufassen. Das waere eine niedrigere Huerde als
// ein Disassembler. Also geht der Block hinaus, und jede Bibliothek prueft
// die Unterschrift selbst.
func (g *Guard) LicenseBlob(product string) (License, []byte, error) {
	if len(g.cfg.IssuerPublicKey) != GuardKeyLen {
		return License{}, nil, Fail("error.guard.issuer")
	}
	if product == "" || len(product) > MaxNameLen {
		return License{}, nil, Fail("error.license.product")
	}
	items, err := g.client.List(0, "")
	if err != nil {
		return License{}, nil, err
	}
	// Erst die Eintraege, deren Name zum Produkt passt. Das ist kein
	// Beiwerk: jedes Aufdecken verlangt am echten Geraet einen
	// Tastendruck und steht im Protokoll. Wer der Reihe nach alle
	// Lizenzen aufdeckt, laesst den Anwender fuenfmal druecken, um eine
	// zu lesen.
	var broken error
	for _, round := range []bool{true, false} {
		for _, item := range items {
			if item.Type != ItemLicense {
				continue
			}
			if strings.EqualFold(item.Name, product) != round {
				continue
			}
			license, blob, readErr := g.readLicense(item.ID)
			if readErr != nil {
				if round {
					// Der Name passt, die Lizenz ist kaputt: das ist die
					// Antwort auf die Frage, nicht ein Fund am Rand.
					return License{}, nil, readErr
				}
				// Eine kaputte Lizenz eines ANDEREN Produkts darf dieses
				// hier nicht blockieren - aber verschwiegen wird sie auch
				// nicht, falls am Ende nichts gefunden wird.
				if broken == nil {
					broken = readErr
				}
				continue
			}
			if strings.EqualFold(license.Product, product) {
				return license, blob, nil
			}
		}
	}
	if broken != nil {
		return License{}, nil, broken
	}
	return License{}, nil, Fail("error.license.missing", product)
}

// readLicense holt eine Lizenz und prueft sie. Der Block kommt mit heraus,
// damit der lokale Dienst ihn weitergeben kann - eine Lizenz ist kein
// Geheimnis, sie ist eine unterschriebene Aussage ueber den Kunden.
func (g *Guard) readLicense(id uint32) (License, []byte, error) {
	blob, err := g.client.Reveal(id)
	if err != nil {
		return License{}, nil, err
	}
	license, err := LicenseVerify(g.cfg.IssuerPublicKey, blob)
	if err != nil {
		Wipe(blob)
		return License{}, nil, err
	}
	return license, blob, nil
}
