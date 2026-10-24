//go:build !cgo

// Ohne C-Uebersetzer gibt es diese Bibliothek nicht.
//
// Das ist Absicht und kein Notbehelf: PKCS#11 ist eine C-Schnittstelle, und
// eine .dll oder .so kann nur entstehen, wo ein C-Uebersetzer steht. Diese
// Datei haelt das Paket trotzdem baubar, damit `go build ./...` und
// `tools/check.py` auf einem Rechner ohne Uebersetzer nicht scheitern -
// sonst waere die Pruefung des ganzen Programms von einem Werkzeug
// abhaengig, das nur fuer diesen einen Baustein gebraucht wird.
//
// Gebaut wird die Bibliothek so:
//
//	go build -buildmode=c-shared -o osk-pkcs11.dll ./classes/pkcs11   (Windows)
//	go build -buildmode=c-shared -o osk-pkcs11.so  ./classes/pkcs11   (Linux)
//	go build -buildmode=c-shared -o osk-pkcs11.dylib ./classes/pkcs11 (macOS)
package main

func main() {}
