// osk - das Verwaltungsprogramm von OpenSecureKey.
//
//	osk                 Oberflaeche starten (lokale Webseite)
//	osk list            Eintraege auflisten
//	osk get <name>      ein Geheimnis ausgeben
//	osk unlock          Tresor oeffnen, PIN von der Standardeingabe
//
// Eine Datei je System, ohne cgo (Ausnahme: macOS, siehe docs/desktop.md).
// Seiten, Texte und Gestaltung stecken im Programm - es laeuft auch von
// einem schreibgeschuetzten Datentraeger, ohne Installation.
package main

import (
	"embed"
	"fmt"
	"os"
	"strings"

	"osk/classes"
)

//go:embed lang templates views assets VERSION
var files embed.FS

func main() {
	classes.Version = version()
	cfg, cfgErr := classes.LoadConfig(classes.ConfigPath())
	app := classes.NewApp(files, cfg)
	if cfgErr != nil {
		// Die Vorgaben gelten weiter; der Grund gehoert aber gesagt.
		fmt.Fprintln(os.Stderr, app.Message(cfgErr))
	}
	defer app.Disconnect()

	args := os.Args[1:]
	if len(args) == 0 || args[0] == "ui" {
		os.Exit(serve(app))
	}
	os.Exit(classes.RunCLI(app, args, os.Stdin, os.Stdout, os.Stderr))
}

// serve startet die Oberflaeche und nennt die Adresse.
func serve(app *classes.App) int {
	server, err := classes.NewServer(app)
	if err != nil {
		fmt.Fprintln(os.Stderr, app.Message(err))
		return 1
	}
	fmt.Printf(app.Text("cli.started")+"\n", server.Address())
	if err := server.Serve(); err != nil {
		fmt.Fprintln(os.Stderr, app.Message(classes.Fail("error.internal")))
		return 1
	}
	return 0
}

// version liest die Fassung aus der eingebetteten Datei VERSION - eine
// einzige Quelle fuer die Nummer.
func version() string {
	data, err := files.ReadFile("VERSION")
	if err != nil {
		return "0.0.0.0"
	}
	return strings.TrimSpace(string(data))
}
