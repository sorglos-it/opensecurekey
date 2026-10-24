// Lizenzen: Aufbau pruefen, Unterschrift pruefen, und ERST DANN die Felder
// aus der geprueften Nutzlast lesen.
//
// Das ist der Punkt, an dem diese Bibliothek dem lokalen Dienst nichts
// glaubt. Frueher gab der Dienst die fertig gelesenen Felder heraus - dann
// haette eine falsche Attrappe auf der Rueckschleife Lizenzen erfinden
// koennen, ohne das geschuetzte Programm anzufassen. Das ist eine
// niedrigere Huerde als ein Disassembler, also ist es jetzt anders: der
// Dienst gibt den unterschriebenen Block heraus, und geprueft wird hier.
package oskguard

import (
	"crypto/ed25519"
	"encoding/binary"
	"strings"
	"time"
)

// Aufbau einer Lizenz.
const (
	licenseMagic   = "OSKLIC1"
	licenseVersion = 1
	licenseHeadLen = len(licenseMagic) + 1 + 2
	licenseMaxLoad = 1024
)

// License ist eine geprüfte Lizenz. Jedes Feld steht so in der
// unterschriebenen Nutzlast - nichts davon ist eine Zusage des Dienstes.
type License struct {
	Product    string
	Holder     string
	Number     string
	Tiers      []string
	Seats      uint32
	IssuedUTC  int64
	ExpiresUTC int64
}

// Expired sagt, ob die Lizenz jetzt abgelaufen ist. Das ist eine Frage der
// Uhr, nicht der Unterschrift - eine abgelaufene Lizenz ist echt und vorbei.
func (l License) Expired() bool {
	return l.ExpiresUTC != 0 && time.Now().UTC().Unix() > l.ExpiresUTC
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

// LicenseVerify prueft den Block gegen den Schluessel des Herstellers.
func LicenseVerify(issuerPublicKey, blob []byte) (License, error) {
	if len(issuerPublicKey) != KeyLen {
		return License{}, fail("error.guard.issuer")
	}
	if len(blob) < licenseHeadLen+SigLen ||
		string(blob[:len(licenseMagic)]) != licenseMagic {
		return License{}, fail("error.license.format")
	}
	if blob[len(licenseMagic)] != licenseVersion {
		return License{}, fail("error.license.version")
	}
	size := int(binary.BigEndian.Uint16(blob[len(licenseMagic)+1:]))
	if size == 0 || size > licenseMaxLoad ||
		licenseHeadLen+size+SigLen != len(blob) {
		return License{}, fail("error.license.format")
	}
	load := blob[licenseHeadLen : licenseHeadLen+size]
	signature := blob[licenseHeadLen+size:]

	signed := make([]byte, 0, len(licenseMagic)+1+len(load))
	signed = append(signed, licenseMagic...)
	signed = append(signed, 0)
	signed = append(signed, load...)
	if !ed25519.Verify(ed25519.PublicKey(issuerPublicKey), signed, signature) {
		return License{}, fail("error.license.signature")
	}
	return licenseFields(load)
}

// licenseFields liest die Felder aus der GEPRUEFTEN Nutzlast.
func licenseFields(load []byte) (License, error) {
	fields, err := cborMap(load)
	if err != nil {
		return License{}, err
	}
	out := License{}
	product, ok := fields["p"].(string)
	if !ok || product == "" {
		return License{}, fail("error.license.product")
	}
	out.Product = product
	out.Holder, _ = fields["h"].(string)
	out.Number, _ = fields["nr"].(string)
	if seats, found := fields["s"].(uint64); found {
		if seats > 0xffffffff {
			return License{}, fail("error.license.format")
		}
		out.Seats = uint32(seats)
	}
	if issued, found := fields["i"].(uint64); found {
		if issued > 1<<62 {
			return License{}, fail("error.license.time")
		}
		out.IssuedUTC = int64(issued)
	}
	if expires, found := fields["e"].(uint64); found {
		if expires > 1<<62 {
			return License{}, fail("error.license.time")
		}
		out.ExpiresUTC = int64(expires)
	}
	if list, found := fields["t"].([]any); found {
		for _, entry := range list {
			tier, isText := entry.(string)
			if !isText || tier == "" {
				return License{}, fail("error.license.tier")
			}
			out.Tiers = append(out.Tiers, tier)
		}
	}
	return out, nil
}
