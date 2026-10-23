// Die HID-Schicht, selbst geschrieben: kein fremdes Paket, nur die
// Systemaufrufe. Je System eine Datei - hid_windows.go, hid_linux.go,
// hid_darwin.go. Diese Datei ist der gemeinsame Teil.
package classes

import "strings"

// Kennungen des Geraets. Noch nicht bestellt, also vorlaeufig: beides steht
// auch in shared/protocol.md und wird mit der Firmware abgeglichen.
const (
	OskVendorID     = 0x1209 // pid.codes, freier Bereich fuer eigene Geraete
	OskProductID    = 0x05E1
	OskUsagePage    = 0xF1D1 // eigener Kanal, neben FIDO (0xF1D0)
	OskUsage        = 0x0001
	OskProductMatch = "opensecurekey"
)

// HidInfo beschreibt ein gefundenes Geraet. Der Pfad ist das, was HidOpen
// versteht; er stammt vom System, nicht vom Geraet.
type HidInfo struct {
	Path      string
	Serial    string
	Product   string
	VendorID  uint16
	ProductID uint16
	UsagePage uint16
	Usage     uint16
}

// Label ist der Name, den die Oberflaeche zeigt. Ohne Produktnamen bleibt
// der Pfad - raten wird nichts.
func (h HidInfo) Label() string {
	name := strings.TrimSpace(h.Product)
	if name == "" {
		return h.Path
	}
	if s := strings.TrimSpace(h.Serial); s != "" {
		return name + " (" + s + ")"
	}
	return name
}

// HidFind sucht nur Geraete, die zu unseren Kennungen passen.
func HidFind() ([]HidInfo, error) {
	all, err := HidEnumerate()
	if err != nil {
		return nil, err
	}
	var out []HidInfo
	for _, d := range all {
		if matchesOsk(d) {
			out = append(out, d)
		}
	}
	return out, nil
}

// matchesOsk entscheidet nach Kennung und Verwendungsseite. Der Produktname
// allein genuegt nie - den kann jedes Geraet behaupten.
func matchesOsk(d HidInfo) bool {
	if d.VendorID != OskVendorID || d.ProductID != OskProductID {
		return false
	}
	// Das Geraet meldet mehrere Schnittstellen (FIDO, Tastatur, eigener
	// Kanal). Gewollt ist nur der eigene Kanal; meldet das System keine
	// Verwendungsseite, wird die Schnittstelle trotzdem angeboten.
	return d.UsagePage == OskUsagePage || d.UsagePage == 0
}
