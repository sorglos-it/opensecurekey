// Pruefstand fuer die Beispiele: Attrappe und lokaler Dienst in einem
// Programm. Damit laufen die Beispiele in C, Go, PHP, Python und .NET ohne
// Hardware - und ohne dass jede Sprache das Protokoll nachbauen muesste.
//
//	go run ./tests/guard-sim -out stand.json
//
// Geschrieben wird eine kleine Datei mit Adresse und Geraeteschluessel; die
// Beispiele lesen sie. Kein echtes Geraet ist dabei: alles liegt im
// Arbeitsspeicher, der Tastendruck ist behauptet.
package main

import (
	"crypto/ed25519"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"path/filepath"
	"time"

	"osk/classes"
)

// Stand ist, was die Beispiele brauchen.
type Stand struct {
	Address   string `json:"address"`
	DeviceKey string `json:"deviceKey"`
	IssuerKey string `json:"issuerKey"`
	Product   string `json:"product"`
	Label     string `json:"label"`
	NodeKey   string `json:"nodeKey"`
}

func main() {
	out := flag.String("out", "", "Datei fuer Adresse und Schluessel")
	port := flag.Int("port", 0, "Port, 0 nimmt einen freien")
	noECDH := flag.Bool("no-ecdh", false,
		"ein Chip, der keinen Schluesselaustausch kann (FIPS)")
	touch := flag.Bool("touch", false, "je Unterschrift einen Tastendruck verlangen")
	once := flag.Bool("once", false, "nach dem Schreiben sofort beenden (fuer Pruefungen)")
	flag.Parse()

	device, err := classes.NewGuardFakeDevice(classes.GuardFakeOptions{
		Seed:         []byte("osk-guard-pruefstand-1"),
		NoECDH:       *noECDH,
		RequireTouch: *touch,
	})
	if err != nil {
		fmt.Fprintln(os.Stderr, "Attrappe:", err)
		os.Exit(1)
	}

	// Eine Lizenz hineinlegen, damit die Beispiele etwas zu pruefen haben.
	issuerSeed := make([]byte, ed25519.SeedSize)
	copy(issuerSeed, "osk-guard-pruefstand-hersteller")
	issuer := ed25519.NewKeyFromSeed(issuerSeed)
	license, err := classes.LicenseSign(classes.License{
		Product:    "beispiel-programm",
		Holder:     "Sorglos Thomas Weirich",
		Number:     "PRUEF-0001",
		Tiers:      []string{"basis", "berichte"},
		Seats:      3,
		IssuedUTC:  1700000000,
		ExpiresUTC: 4070908800, // 01.01.2099
	}, issuer)
	if err != nil {
		fmt.Fprintln(os.Stderr, "Lizenz:", err)
		os.Exit(1)
	}
	device.AddItem(classes.Item{Type: classes.ItemLicense,
		Name: "beispiel-programm"}, license)

	// Die Attrappe verlangt keine PIN, darum ist der Tresor offen. Der
	// Dongle-Schutz braucht die Programmdatei des AUFRUFERS, nicht die
	// hier - die Pruefsumme kommt mit jeder Frage herein.
	guard, err := classes.NewGuard(device.GuardFakeClient("guard-sim"),
		classes.GuardConfig{
			DevicePublicKey: device.PublicKey(),
			IssuerPublicKey: issuer.Public().(ed25519.PublicKey),
			BindBinary:      false,
			Interval:        time.Second,
		})
	if err != nil {
		fmt.Fprintln(os.Stderr, "Guard:", err)
		os.Exit(1)
	}
	agent, err := classes.NewGuardAgent(guard, *port)
	if err != nil {
		fmt.Fprintln(os.Stderr, "Dienst:", err)
		os.Exit(1)
	}
	defer agent.Close()

	stand := Stand{
		Address:   agent.Address(),
		DeviceKey: hex.EncodeToString(device.PublicKey()),
		IssuerKey: hex.EncodeToString(issuer.Public().(ed25519.PublicKey)),
		Product:   "beispiel-programm",
		Label:     "datenmodell",
		NodeKey:   "auftraege",
	}
	if *out != "" {
		if err := writeStand(*out, stand); err != nil {
			fmt.Fprintln(os.Stderr, "Stand schreiben:", err)
			os.Exit(1)
		}
	}
	fmt.Printf("Pruefstand laeuft auf %s\n", stand.Address)
	fmt.Printf("Geraeteschluessel %s\n", stand.DeviceKey)
	if *once {
		return
	}
	// Bis Strg-C oder bis der Aufrufer das Programm beendet.
	stop := make(chan os.Signal, 1)
	signal.Notify(stop, os.Interrupt)
	<-stop
}

// writeStand schreibt atomar: erst daneben, dann umbenennen.
func writeStand(path string, stand Stand) error {
	data, err := json.MarshalIndent(stand, "", "  ")
	if err != nil {
		return err
	}
	if dir := filepath.Dir(path); dir != "" {
		if err := os.MkdirAll(dir, 0o700); err != nil {
			return err
		}
	}
	temp := path + ".new"
	if err := os.WriteFile(temp, append(data, '\n'), 0o600); err != nil {
		return err
	}
	if err := os.Rename(temp, path); err != nil {
		os.Remove(temp)
		return err
	}
	return nil
}
