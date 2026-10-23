//go:build (darwin && !cgo) || (!windows && !linux && !darwin)

// Auf Systemen ohne eigene HID-Datei gibt es keinen Geraetezugang. Das
// Programm laeuft trotzdem - gegen die Attrappe aus tests/ - und sagt klar,
// dass echte Geraete hier nicht erreichbar sind.
//
// Auf einem Mac greift diese Datei nur, wenn ohne cgo gebaut wird
// (CGO_ENABLED=0); mit cgo gilt hid_darwin.go.
package classes

// HidEnumerate findet hier nie ein Geraet.
func HidEnumerate() ([]HidInfo, error) {
	return nil, Fail("error.hid.unsupported")
}

// HidOpen gibt es hier nicht.
func HidOpen(path string) (Transport, error) {
	return nil, Fail("error.hid.unsupported")
}
