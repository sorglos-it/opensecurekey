// Beispiel in Go: ein Programm, das nur mit Stick laeuft, eine Datei
// verschliesst, die ohne Stick unlesbar bleibt, seine Lizenz prueft und
// einen Auftrag unterschreibt.
//
//	go run ./example -stand ..\..\stand.json
//
// Jede Zeile, die mit "ergebnis:" anfaengt, liest der Pruefstand mit. Bricht
// das Beispiel ab, ist der Ausstiegscode nicht 0.
package main

import (
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"time"

	"oskguard"
)

type stand struct {
	Address   string `json:"address"`
	DeviceKey string `json:"deviceKey"`
	IssuerKey string `json:"issuerKey"`
	Product   string `json:"product"`
	Label     string `json:"label"`
	NodeKey   string `json:"nodeKey"`
}

func main() {
	path := flag.String("stand", "stand.json", "Datei des Pruefstands")
	flag.Parse()
	if err := run(*path); err != nil {
		fmt.Fprintln(os.Stderr, "FEHLER:", err)
		os.Exit(1)
	}
}

func run(path string) error {
	data, err := os.ReadFile(path)
	if err != nil {
		return err
	}
	var s stand
	if err := json.Unmarshal(data, &s); err != nil {
		return err
	}
	deviceKey, err := hex.DecodeString(s.DeviceKey)
	if err != nil {
		return err
	}
	issuerKey, err := hex.DecodeString(s.IssuerKey)
	if err != nil {
		return err
	}

	// 1. Ohne Stick laeuft das Programm nicht los.
	guard, err := oskguard.Open(s.Address, deviceKey, issuerKey)
	if err != nil {
		return fmt.Errorf("kein Sicherheitsschluessel: %w", err)
	}
	if err := guard.Present(""); err != nil {
		return fmt.Errorf("der Stick steckt nicht: %w", err)
	}
	fmt.Println("ergebnis: praesenz ok")

	// 2. Eine Datei, die ohne Stick unlesbar bleibt.
	plain := []byte("die Tabelle, ohne die das Programm nichts kann")
	blob, err := guard.Seal(s.Label, []byte("version-1"), plain)
	if err != nil {
		return err
	}
	back, err := guard.Unseal(s.Label, []byte("version-1"), blob)
	if err != nil {
		return err
	}
	if string(back) != string(plain) {
		return fmt.Errorf("heraus kam etwas anderes")
	}
	// Mit der falschen Programmversion geht sie nicht auf - das muss so sein.
	if _, err := guard.Unseal(s.Label, []byte("version-2"), blob); err == nil {
		return fmt.Errorf("mit falschem Zusatz aufgegangen")
	}
	fmt.Printf("ergebnis: verschlossen %d byte, wieder offen\n", len(blob))

	// 2b. Ein verschluesselter PROGRAMMTEIL. Das ist der starke Weg: ohne
	// Stick fehlt dem Angreifer nicht die Antwort, sondern der Teil selbst.
	code := []byte("die Regeln, nach denen dieses Programm rechnet")
	codeBlob, codeHash, err := guard.SealCode(s.Label, code)
	if err != nil {
		return err
	}
	gotCode, err := guard.UnsealCode(s.Label, codeBlob, codeHash)
	if err != nil {
		return err
	}
	if string(gotCode) != string(code) {
		return fmt.Errorf("der Programmteil kam anders zurueck")
	}
	// Mit einem anderen Hash gibt es KEINE Bytes.
	other := make([]byte, len(codeHash))
	copy(other, codeHash)
	other[0] ^= 0x01
	if _, err := guard.UnsealCode(s.Label, codeBlob, other); err == nil {
		return fmt.Errorf("ein fremder Programmteil ging auf")
	} else if oskguard.Code(err) == "" {
		return fmt.Errorf("kein Fehlerschluessel: %v", err)
	}
	fmt.Printf("ergebnis: programmteil %d byte, nur mit stick lesbar\n",
		len(codeBlob))

	// 3. Die Lizenz.
	license, err := guard.License(s.Product)
	if err != nil {
		return err
	}
	if license.Expired() {
		return fmt.Errorf("Lizenz abgelaufen")
	}
	if !license.HasTier("berichte") {
		return fmt.Errorf("Ausbaustufe fehlt")
	}
	fmt.Printf("ergebnis: lizenz %s, %d plaetze, stufen %v\n",
		license.Product, license.Seats, license.Tiers)

	// 4. Ein Auftrag, unterschrieben vom Schluessel im Chip.
	public, err := guard.NodePublic(s.NodeKey, "ed25519")
	if err != nil {
		return err
	}
	job := []byte(`{"tat":"abbild-holen","wann":1700000000}`)
	signature, err := guard.NodeSign(s.NodeKey, job)
	if err != nil {
		return err
	}
	if err := oskguard.NodeVerify(public, s.NodeKey, job, signature); err != nil {
		return err
	}
	bent := append([]byte{}, job...)
	bent[3] ^= 1
	if err := oskguard.NodeVerify(public, s.NodeKey, bent, signature); err == nil {
		return fmt.Errorf("ein veraenderter Auftrag galt")
	}
	fmt.Println("ergebnis: auftrag unterschrieben und geprueft")

	// 5. Der Schluesselaustausch - und der Fall, dass der Chip ihn nicht kann.
	peer, err := guard.NodePublic(s.NodeKey, "x25519")
	if err != nil {
		if oskguard.NoECDH(err) {
			fmt.Println("ergebnis: kein schluesselaustausch - " +
				"serverbetrieb mit schluessel im chip faellt weg")
		} else {
			return err
		}
	} else {
		secret, agreeErr := guard.NodeAgree(s.NodeKey, peer)
		if agreeErr != nil {
			if !oskguard.NoECDH(agreeErr) {
				return agreeErr
			}
			fmt.Println("ergebnis: kein schluesselaustausch - " +
				"serverbetrieb mit schluessel im chip faellt weg")
		} else {
			fmt.Printf("ergebnis: gemeinsames geheimnis %d byte\n", len(secret))
		}
	}

	// 6. Weiterlaufen und beim Abziehen abbrechen - hier nur eine Runde,
	// damit das Beispiel von selbst endet.
	done := make(chan error, 1)
	go guard.Watch(200*time.Millisecond, "", func(err error) { done <- err })
	select {
	case err := <-done:
		return fmt.Errorf("abgebrochen: %w", err)
	case <-time.After(600 * time.Millisecond):
	}
	fmt.Println("ergebnis: ueberwachung laeuft")
	return nil
}
