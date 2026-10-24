// Die Lizenz-Pruefwerte aus tests/license-vectors.json durch den eigenen
// Leser laufen lassen.
//
// Der Zweck dieser Datei ist der CBOR-Leser. Die Regeln darin - kuerzeste
// Schreibweise, keine angehaengten Bytes, kein Schluessel zweimal - liessen
// sich sonst nicht messen: geprueft wird die Unterschrift VOR dem Lesen,
// also braucht eine falsch geschriebene Nutzlast eine gueltige Unterschrift,
// um ueberhaupt bis zum Leser zu kommen. Die Pruefwerte sind darum fertig
// unterschrieben. Vorgeschlagen hat das die .NET-Fassung.
package oskguard

import (
	"encoding/hex"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

type licenseVectorFile struct {
	IssuerPublicKey string          `json:"issuerPublicKey"`
	Cases           []licenseVector `json:"cases"`
}

type licenseVector struct {
	Name  string `json:"name"`
	Blob  string `json:"blob"`
	Valid bool   `json:"valid"`
	Error string `json:"error"`
	Want  struct {
		Product string   `json:"product"`
		Holder  string   `json:"holder"`
		Number  string   `json:"number"`
		Tiers   []string `json:"tiers"`
		Seats   uint32   `json:"seats"`
		Issued  int64    `json:"issued"`
		Expires int64    `json:"expires"`
	} `json:"want"`
	Why string `json:"why"`
}

// Mindestzahlen: findet der Satz weniger, ist er selbst kaputt und nicht
// der Code. Null gelesene Faelle waeren sonst ein gruener Lauf.
const (
	wantLicenseCases = 24
	wantLicenseValid = 3
)

func TestLicenseVectors(t *testing.T) {
	path := filepath.Join("..", "tests", "license-vectors.json")
	data, err := os.ReadFile(path)
	if err != nil {
		// Fehlt die Datei, ist das ein Fehler und kein Grund, die
		// Pruefung zu ueberspringen.
		t.Fatalf("%s: %v", path, err)
	}
	var file licenseVectorFile
	if err := json.Unmarshal(data, &file); err != nil {
		t.Fatalf("%s: %v", path, err)
	}
	issuer, err := hex.DecodeString(file.IssuerPublicKey)
	if err != nil || len(issuer) != KeyLen {
		t.Fatalf("Herstellerschluessel unbrauchbar")
	}
	if len(file.Cases) != wantLicenseCases {
		t.Fatalf("%d Faelle, erwartet %d", len(file.Cases), wantLicenseCases)
	}

	var valid int
	for _, one := range file.Cases {
		t.Run(one.Name, func(t *testing.T) {
			blob, decodeErr := hex.DecodeString(one.Blob)
			if decodeErr != nil {
				t.Fatalf("Block ist kein Hex: %v", decodeErr)
			}
			license, verifyErr := LicenseVerify(issuer, blob)
			if one.Valid {
				if verifyErr != nil {
					t.Fatalf("%s: abgewiesen, obwohl gueltig: %v",
						one.Why, verifyErr)
				}
				checkLicenseFields(t, license, one)
				return
			}
			if verifyErr == nil {
				t.Fatalf("%s: angenommen, obwohl falsch", one.Why)
			}
			if one.Error != "" && Code(verifyErr) != one.Error {
				t.Fatalf("%s: erwartet %s, bekommen %s (%s)",
					one.Why, one.Error, Code(verifyErr), one.Why)
			}
		})
		if one.Valid {
			valid++
		}
	}
	if valid != wantLicenseValid {
		t.Fatalf("%d gueltige Faelle, erwartet %d", valid, wantLicenseValid)
	}
}

func checkLicenseFields(t *testing.T, got License, one licenseVector) {
	t.Helper()
	if got.Product != one.Want.Product {
		t.Errorf("Produkt %q, erwartet %q", got.Product, one.Want.Product)
	}
	if got.Holder != one.Want.Holder {
		t.Errorf("Lizenznehmer %q, erwartet %q", got.Holder, one.Want.Holder)
	}
	if got.Number != one.Want.Number {
		t.Errorf("Nummer %q, erwartet %q", got.Number, one.Want.Number)
	}
	if got.Seats != one.Want.Seats {
		t.Errorf("Plaetze %d, erwartet %d", got.Seats, one.Want.Seats)
	}
	if got.IssuedUTC != one.Want.Issued {
		t.Errorf("ausgestellt %d, erwartet %d", got.IssuedUTC, one.Want.Issued)
	}
	if got.ExpiresUTC != one.Want.Expires {
		t.Errorf("Ablauf %d, erwartet %d", got.ExpiresUTC, one.Want.Expires)
	}
	if len(got.Tiers) != len(one.Want.Tiers) {
		t.Fatalf("%d Ausbaustufen, erwartet %d", len(got.Tiers),
			len(one.Want.Tiers))
	}
	for i, tier := range one.Want.Tiers {
		if got.Tiers[i] != tier {
			t.Errorf("Ausbaustufe %d ist %q, erwartet %q", i, got.Tiers[i], tier)
		}
	}
}

// Ein falscher Herstellerschluessel macht jede Lizenz ungueltig - auch die
// richtige. Das ist der Fall, um den der ganze Umbau gemacht wurde.
func TestLicenseFromAnotherIssuerIsRejected(t *testing.T) {
	path := filepath.Join("..", "tests", "license-vectors.json")
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("%s: %v", path, err)
	}
	var file licenseVectorFile
	if err := json.Unmarshal(data, &file); err != nil {
		t.Fatalf("%s: %v", path, err)
	}
	issuer, err := hex.DecodeString(file.IssuerPublicKey)
	if err != nil {
		t.Fatalf("Herstellerschluessel: %v", err)
	}
	var checked int
	for _, one := range file.Cases {
		if !one.Valid {
			continue
		}
		blob, decodeErr := hex.DecodeString(one.Blob)
		if decodeErr != nil {
			t.Fatalf("Block: %v", decodeErr)
		}
		// Ein Bit im Schluessel des Herstellers gedreht.
		stranger := make([]byte, len(issuer))
		copy(stranger, issuer)
		stranger[0] ^= 0x01
		if _, err := LicenseVerify(stranger, blob); Code(err) != "error.license.signature" {
			t.Fatalf("%s: unter fremdem Herstellerschluessel kam %v", one.Name, err)
		}
		checked++
	}
	if checked != wantLicenseValid {
		t.Fatalf("%d gueltige Faelle geprueft, erwartet %d", checked,
			wantLicenseValid)
	}
}
