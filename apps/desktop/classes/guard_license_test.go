// Pruefungen der Lizenzen: unterschrieben vom Hersteller, geprueft vom
// Programm, getragen vom Stick.
package classes

import (
	"crypto/ed25519"
	"errors"
	"testing"
	"time"
)

// licenseTestIssuer gibt ein wiederholbares Schluesselpaar des Herstellers.
func licenseTestIssuer(t *testing.T, seedText string) (ed25519.PublicKey, ed25519.PrivateKey) {
	t.Helper()
	seed := make([]byte, ed25519.SeedSize)
	copy(seed, seedText)
	private := ed25519.NewKeyFromSeed(seed)
	return private.Public().(ed25519.PublicKey), private
}

func licenseTestSample() License {
	return License{
		Product:    "mein-programm",
		Holder:     "Sorglos Thomas Weirich",
		Number:     "A-0001",
		Tiers:      []string{"basis", "berichte"},
		Seats:      5,
		IssuedUTC:  1700000000,
		ExpiresUTC: 1800000000,
	}
}

func TestLicenseSignAndVerify(t *testing.T) {
	public, private := licenseTestIssuer(t, "hersteller-1")
	want := licenseTestSample()
	blob, err := LicenseSign(want, private)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	got, err := LicenseVerify(public, blob)
	if err != nil {
		t.Fatalf("pruefen: %v", err)
	}
	if got.Product != want.Product || got.Holder != want.Holder ||
		got.Number != want.Number || got.Seats != want.Seats ||
		got.IssuedUTC != want.IssuedUTC || got.ExpiresUTC != want.ExpiresUTC {
		t.Fatalf("heraus kam etwas anderes: %+v", got)
	}
	if len(got.Tiers) != 2 || !got.HasTier("BASIS") || !got.HasTier("berichte") {
		t.Fatalf("Ausbaustufen falsch: %v", got.Tiers)
	}
	if got.HasTier("alles") {
		t.Fatal("eine Ausbaustufe erfunden")
	}
}

// Die Unterschrift eines anderen gilt nicht - das ist der ganze Zweck.
func TestLicenseFromStrangerIsRejected(t *testing.T) {
	public, _ := licenseTestIssuer(t, "hersteller-1")
	_, strangerPrivate := licenseTestIssuer(t, "irgendwer-sonst")
	blob, err := LicenseSign(licenseTestSample(), strangerPrivate)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	_, err = LicenseVerify(public, blob)
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.license.signature" {
		t.Fatalf("erwartet error.license.signature, bekommen %v", err)
	}
}

// Jedes veraenderte Byte faellt auf.
func TestLicenseNoticesEveryChangedByte(t *testing.T) {
	public, private := licenseTestIssuer(t, "hersteller-1")
	blob, err := LicenseSign(licenseTestSample(), private)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	for at := range blob {
		bent := make([]byte, len(blob))
		copy(bent, blob)
		bent[at] ^= 0x01
		if _, err := LicenseVerify(public, bent); err == nil {
			t.Fatalf("Byte %d veraendert und trotzdem angenommen", at)
		}
	}
}

// Abgelaufen ist eine Frage der Uhr, nicht der Unterschrift. Beides muss
// sich trennen lassen: eine abgelaufene Lizenz ist echt, aber vorbei.
func TestLicenseExpiryIsSeparateFromSignature(t *testing.T) {
	public, private := licenseTestIssuer(t, "hersteller-1")
	sample := licenseTestSample()
	sample.ExpiresUTC = 1700001000
	blob, err := LicenseSign(sample, private)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	license, err := LicenseVerify(public, blob)
	if err != nil {
		t.Fatalf("die Unterschrift muesste gelten: %v", err)
	}
	if license.Expired(time.Unix(1700000500, 0)) {
		t.Fatal("vorher schon abgelaufen")
	}
	if !license.Expired(time.Unix(1700002000, 0)) {
		t.Fatal("nachher nicht abgelaufen")
	}
	// Ohne Ablauf laeuft sie nie ab.
	sample.ExpiresUTC = 0
	blob, err = LicenseSign(sample, private)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	license, err = LicenseVerify(public, blob)
	if err != nil {
		t.Fatalf("pruefen: %v", err)
	}
	if license.Expired(time.Unix(1<<40, 0)) {
		t.Fatal("eine Lizenz ohne Ablauf ist abgelaufen")
	}
}

// Aus dem Tresor geholt und geprueft - der ganze Weg.
func TestGuardLicenseFromVault(t *testing.T) {
	issuerPublic, issuerPrivate := licenseTestIssuer(t, "hersteller-1")
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("lizenz")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	blob, err := LicenseSign(licenseTestSample(), issuerPrivate)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	device.AddItem(Item{Type: ItemLicense, Name: "mein-programm"}, blob)

	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.IssuerPublicKey = issuerPublic
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("egal"))
	guard, err := NewGuard(device.GuardFakeClient("test"), cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	license, err := guard.License("mein-programm")
	if err != nil {
		t.Fatalf("Lizenz holen: %v", err)
	}
	if license.Seats != 5 || !license.HasTier("berichte") {
		t.Fatalf("falsche Lizenz: %+v", license)
	}

	// Ein anderes Produkt liegt nicht darauf, und das wird gesagt.
	_, err = guard.License("fremdes-programm")
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.license.missing" {
		t.Fatalf("erwartet error.license.missing, bekommen %v", err)
	}

	// Jeder Zugriff steht im Geraeteprotokoll.
	var revealed int
	for _, line := range device.Log() {
		if line.Event == "reveal" {
			revealed++
		}
	}
	if revealed < 2 {
		t.Fatalf("nur %d Zugriffe im Protokoll - erwartet mindestens 2", revealed)
	}
}

// Ohne Herstellerschluessel wird nicht geprueft, sondern abgelehnt. Eine
// ungeprueft angenommene Lizenz waere schlimmer als keine.
func TestGuardLicenseRefusesWithoutIssuerKey(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	_, err := guard.License("mein-programm")
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.guard.issuer" {
		t.Fatalf("erwartet error.guard.issuer, bekommen %v", err)
	}
}

func TestLicenseEncodeRejectsBadFields(t *testing.T) {
	long := string(make([]byte, MaxNameLen+1))
	cases := map[string]License{
		"ohne Produkt":             {},
		"Produkt zu lang":          {Product: long},
		"Steuerzeichen":            {Product: "a\nb"},
		"Lizenznehmer zu lang":     {Product: "a", Holder: long},
		"Nummer mit Steuerzeichen": {Product: "a", Number: "x\x00y"},
		"leere Ausbaustufe":        {Product: "a", Tiers: []string{""}},
		"negative Zeit":            {Product: "a", IssuedUTC: -1},
	}
	for name, license := range cases {
		t.Run(name, func(t *testing.T) {
			if _, err := LicenseEncode(license); err == nil {
				t.Fatal("wurde angenommen")
			}
		})
	}
}

func TestLicenseVerifyRejectsBrokenShapes(t *testing.T) {
	public, private := licenseTestIssuer(t, "hersteller-1")
	good, err := LicenseSign(licenseTestSample(), private)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	cases := []struct {
		name string
		blob []byte
		want string
	}{
		{"leer", nil, "error.license.format"},
		{"zu kurz", good[:10], "error.license.format"},
		{"falsche Kennung", append([]byte("XXXXXXX"), good[7:]...), "error.license.format"},
		{"zu lange Angabe", append(good, 0), "error.license.format"},
	}
	for _, one := range cases {
		t.Run(one.name, func(t *testing.T) {
			_, err := LicenseVerify(public, one.blob)
			var problem *Problem
			if !errors.As(err, &problem) || problem.Key != one.want {
				t.Fatalf("erwartet %s, bekommen %v", one.want, err)
			}
		})
	}
	// Eine Fassung, die wir noch nicht kennen.
	bent := make([]byte, len(good))
	copy(bent, good)
	bent[len(licenseMagic)] = licenseVersion + 1
	_, err = LicenseVerify(public, bent)
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.license.version" {
		t.Fatalf("erwartet error.license.version, bekommen %v", err)
	}
	// Und ein unbrauchbarer Herstellerschluessel.
	_, err = LicenseVerify(make([]byte, 3), good)
	if !errors.As(err, &problem) || problem.Key != "error.guard.issuer" {
		t.Fatalf("erwartet error.guard.issuer, bekommen %v", err)
	}
}

func TestLicenseSignRejectsBadIssuerKey(t *testing.T) {
	_, err := LicenseSign(licenseTestSample(), make([]byte, 5))
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.license.issuerkey" {
		t.Fatalf("erwartet error.license.issuerkey, bekommen %v", err)
	}
}

// Eine kaputte Lizenz eines ANDEREN Produkts darf dieses hier nicht
// blockieren. Vorher tat sie es: der erste Fehler im Tresor beendete die
// Suche, und wer zwei Produkte auf einem Stick hat, war verloren.
func TestBrokenLicenseOfAnotherProductDoesNotBlock(t *testing.T) {
	issuerPublic, issuerPrivate := licenseTestIssuer(t, "hersteller-1")
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("zwei-produkte")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	// Zuerst eine kaputte Lizenz eines fremden Produkts ...
	device.AddItem(Item{Type: ItemLicense, Name: "fremdes-programm"},
		[]byte("das ist keine Lizenz"))
	// ... dann die gute.
	good, err := LicenseSign(licenseTestSample(), issuerPrivate)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	device.AddItem(Item{Type: ItemLicense, Name: "mein-programm"}, good)

	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.IssuerPublicKey = issuerPublic
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("egal"))
	guard, err := NewGuard(device.GuardFakeClient("test"), cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	license, err := guard.License("mein-programm")
	if err != nil {
		t.Fatalf("die gute Lizenz wurde von der kaputten verdeckt: %v", err)
	}
	if license.Seats != 5 {
		t.Fatalf("falsche Lizenz: %+v", license)
	}

	// Und sie wurde gefunden, OHNE die fremde aufzudecken - jedes
	// Aufdecken kostet am echten Geraet einen Tastendruck.
	var revealed int
	for _, line := range device.Log() {
		if line.Event == "reveal" {
			revealed++
		}
	}
	if revealed != 1 {
		t.Fatalf("%d Lizenzen aufgedeckt, erwartet genau 1", revealed)
	}
}

// Ist die Lizenz DES GESUCHTEN Produkts kaputt, ist das die Antwort - nicht
// "nicht gefunden". Sonst sucht der Anwender an der falschen Stelle.
func TestBrokenLicenseOfTheWantedProductIsTheAnswer(t *testing.T) {
	issuerPublic, _ := licenseTestIssuer(t, "hersteller-1")
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("kaputt")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	device.AddItem(Item{Type: ItemLicense, Name: "mein-programm"},
		[]byte("das ist keine Lizenz"))
	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.IssuerPublicKey = issuerPublic
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("egal"))
	guard, err := NewGuard(device.GuardFakeClient("test"), cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	_, err = guard.License("mein-programm")
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.license.format" {
		t.Fatalf("erwartet error.license.format, bekommen %v", err)
	}
}

// Der Name des Eintrags im Tresor ist freier Text und kann vom Produkt
// abweichen - verbindlich ist, was in der unterschriebenen Nutzlast steht.
// Also muss die Suche auch dann finden, wenn kein Name passt, und eine
// kaputte Lizenz davor darf sie nicht beenden.
//
// Dieser Fall ist nachgetragen, weil ein Mutant ueberlebt hat: der erste
// Fall traf immer schon den passenden Namen und kam an dieser Stelle nie
// vorbei.
func TestLicenseIsFoundEvenWhenTheItemNameDiffers(t *testing.T) {
	issuerPublic, issuerPrivate := licenseTestIssuer(t, "hersteller-1")
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("namen")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	// Beide Namen passen NICHT zum gesuchten Produkt.
	device.AddItem(Item{Type: ItemLicense, Name: "irgendwas"},
		[]byte("das ist keine Lizenz"))
	good, err := LicenseSign(licenseTestSample(), issuerPrivate)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	device.AddItem(Item{Type: ItemLicense, Name: "umbenannt-vom-anwender"}, good)

	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.IssuerPublicKey = issuerPublic
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("egal"))
	guard, err := NewGuard(device.GuardFakeClient("test"), cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	license, err := guard.License("mein-programm")
	if err != nil {
		t.Fatalf("die Lizenz wurde nicht gefunden: %v", err)
	}
	if license.Product != "mein-programm" || license.Seats != 5 {
		t.Fatalf("falsche Lizenz: %+v", license)
	}
}

// Liegt gar nichts Passendes darauf, aber eine kaputte Lizenz, dann wird
// die kaputte genannt - nicht "nicht gefunden". Sonst sucht der Anwender
// an der falschen Stelle.
func TestBrokenLicenseIsNamedWhenNothingElseFits(t *testing.T) {
	issuerPublic, _ := licenseTestIssuer(t, "hersteller-1")
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("nurkaputt")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	device.AddItem(Item{Type: ItemLicense, Name: "irgendwas"},
		[]byte("das ist keine Lizenz"))
	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.IssuerPublicKey = issuerPublic
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("egal"))
	guard, err := NewGuard(device.GuardFakeClient("test"), cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	_, err = guard.License("mein-programm")
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.license.format" {
		t.Fatalf("erwartet error.license.format, bekommen %v", err)
	}
}
