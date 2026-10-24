// Kommandozeile fuer Skripte. Gleicher Kern wie die Oberflaeche: dieselbe
// Verbindung, dieselben Pruefungen, dieselben Texte.
package classes

import (
	"bufio"
	"fmt"
	"io"
	"strings"
)

// RunCLI fuehrt einen Befehl aus und gibt den Ausstiegscode zurueck.
// Geheimnisse gehen nur nach out und werden danach ueberschrieben.
func RunCLI(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	command := "ui"
	if len(args) > 0 {
		command = args[0]
	}
	switch command {
	case "list":
		return cliList(app, out, errOut)
	case "get":
		if len(args) < 2 {
			fmt.Fprintln(errOut, app.Text("cli.usage"))
			return 2
		}
		return cliGet(app, strings.Join(args[1:], " "), out, errOut)
	case "unlock":
		return cliUnlock(app, in, out, errOut)
	case "help", "-h", "--help":
		fmt.Fprintln(out, app.Text("cli.usage"))
		return 0
	default:
		// Die Wege nach draussen (ssh-Agent, Befehle fuer Skripte, lokale
		// Schnittstelle) tragen sich in extraCommands ein - je Weg eine
		// eigene Datei, siehe native_cli.go.
		if run, found := extraCommands[command]; found {
			return run(app, args, in, out, errOut)
		}
		fmt.Fprintln(errOut, app.Text("cli.usage"))
		return 2
	}
}

func cliList(app *App, out, errOut io.Writer) int {
	client, err := app.Client()
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return 1
	}
	items, err := client.List(0, "")
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return 1
	}
	for _, item := range items {
		fmt.Fprintf(out, "%d\t%s\t%s\t%s\n", item.ID, item.Name,
			app.Text(item.Type.Key()), item.Site)
	}
	return 0
}

// cliGet gibt genau ein Geheimnis aus. Mehrere Treffer werden nicht geraten -
// dann sagt das Programm, dass der Name nicht eindeutig ist.
func cliGet(app *App, name string, out, errOut io.Writer) int {
	client, err := app.Client()
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return 1
	}
	items, err := client.List(0, "")
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return 1
	}
	var found []Item
	for _, item := range items {
		if strings.EqualFold(item.Name, name) {
			found = append(found, item)
		}
	}
	switch len(found) {
	case 0:
		fmt.Fprintln(errOut, app.Text("error.device.notfound"))
		return 1
	case 1:
	default:
		fmt.Fprintln(errOut, app.Text("error.device.exists"))
		return 1
	}
	secret, err := client.Reveal(found[0].ID)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return 1
	}
	defer Wipe(secret)
	out.Write(secret)
	fmt.Fprintln(out)
	return 0
}

// cliUnlock liest die PIN von der Standardeingabe. Sie steht nie in einem
// Aufrufparameter - der waere in der Prozessliste zu sehen.
func cliUnlock(app *App, in io.Reader, out, errOut io.Writer) int {
	client, err := app.Client()
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return 1
	}
	fmt.Fprint(out, app.Text("cli.pin"))
	reader := bufio.NewReaderSize(in, 128)
	line, err := reader.ReadString('\n')
	if err != nil && line == "" {
		fmt.Fprintln(errOut, app.Text("error.item.secret"))
		return 1
	}
	pin := []byte(strings.TrimRight(line, "\r\n"))
	line = ""
	if err := client.Unlock(pin); err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return 1
	}
	fmt.Fprintln(out, app.Text("cli.unlocked"))
	return 0
}
