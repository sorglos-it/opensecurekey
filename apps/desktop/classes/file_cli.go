// Die Kommandozeile des Dateiwerkzeugs.
//
//	osk recipient --key <bezeichnung>
//	    nennt den eigenen Empfaenger (age1...). Er ist oeffentlich und darf
//	    weitergegeben werden.
//
//	osk encrypt --format <age|pgp> [--to age1... ...] [--key <bezeichnung>]
//	            [--in <datei>] [--out <datei>]
//	    verschluesselt. Ohne --in und --out laeuft es durch die Leitung,
//	    damit es sich in eine Kette einsetzen laesst.
//
//	osk decrypt --format <age|pgp> --key <bezeichnung>
//	            [--in <datei>] [--out <datei>]
//	    entschluesselt. Das Geraet rechnet, und je Datei fragt es einmal -
//	    ohne Freischaltung mit Tastendruck.
//
// Das Format wird immer genannt. Es gibt keine Vorgabe und auch kein Erraten
// aus der Datei: wer die beiden verwechselt, soll es hier merken und nicht
// beim Empfaenger.
//
// Was hier nicht vorkommt und auch nicht nachgereicht wird: ein Befehl, der
// einen privaten Schluessel ausgibt oder einlegt. Das Geraet ist der Ort, an
// dem er lebt.
package classes

import (
	"fmt"
	"io"
	"os"
)

func init() {
	extraCommands["recipient"] = cliRecipient
	extraCommands["encrypt"] = cliEncrypt
	extraCommands["decrypt"] = cliDecrypt
}

// fileArgs sind die gelesenen Aufrufparameter.
type fileArgs struct {
	format FileFormat
	label  string
	to     []string
	in     string
	out    string
}

// parseFileArgs liest die Parameter. Ein unbekannter Parameter ist ein
// Fehler und wird nicht uebergangen - sonst wirkt ein Tippfehler wie eine
// Einstellung, die gar nicht gesetzt wurde.
func parseFileArgs(args []string) (fileArgs, error) {
	var parsed fileArgs
	for i := 1; i < len(args); i++ {
		name := args[i]
		value := ""
		if i+1 < len(args) {
			value = args[i+1]
		}
		switch name {
		case "--format":
			if !ValidFileFormat(value) {
				return parsed, Fail("error.file.format")
			}
			parsed.format = FileFormat(value)
			i++
		case "--key":
			parsed.label = value
			i++
		case "--to":
			if value == "" {
				return parsed, Fail("error.file.recipient")
			}
			parsed.to = append(parsed.to, value)
			i++
		case "--in":
			parsed.in = value
			i++
		case "--out":
			parsed.out = value
			i++
		default:
			return parsed, Fail("error.file.usage")
		}
	}
	return parsed, nil
}

// fileIdentityFor baut die Kennung im Chip zu einer Bezeichnung.
func fileIdentityFor(app *App, route, label string) (*FileDeviceIdentity, error) {
	if label == "" {
		return nil, Fail("error.file.nolabel")
	}
	client, err := app.Client()
	if err != nil {
		return nil, err
	}
	_ = route
	return NewFileDeviceIdentity(fileNodeGuard(client), label)
}

// fileOpenIn oeffnet die Eingabe: eine Datei oder die Leitung.
func fileOpenIn(path string, in io.Reader) (io.Reader, func(), error) {
	if path == "" {
		return in, func() {}, nil
	}
	handle, err := os.Open(path)
	if err != nil {
		return nil, nil, Fail("error.file.read")
	}
	return handle, func() { handle.Close() }, nil
}

// fileOpenOut oeffnet die Ausgabe. Eine bestehende Datei wird NICHT
// ueberschrieben: ein vertipptes --out darf nicht die einzige Fassung des
// Klartexts kosten.
func fileOpenOut(path string, out io.Writer) (io.Writer, func() error, error) {
	if path == "" {
		return out, func() error { return nil }, nil
	}
	handle, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o600)
	if err != nil {
		if os.IsExist(err) {
			return nil, nil, Fail("error.file.exists")
		}
		return nil, nil, Fail("error.file.write")
	}
	return handle, handle.Close, nil
}

// --- osk recipient ------------------------------------------------------------

func cliRecipient(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	parsed, err := parseFileArgs(args)
	if err != nil {
		fmt.Fprintln(errOut, app.Text("cli.recipient.usage"))
		return ExitUsage
	}
	identity, err := fileIdentityFor(app, "recipient", parsed.label)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	recipient, err := identity.Recipient()
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	fmt.Fprintln(out, recipient.String())
	return ExitOK
}

// --- osk encrypt --------------------------------------------------------------

func cliEncrypt(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	parsed, err := parseFileArgs(args)
	if err != nil || parsed.format == "" {
		fmt.Fprintln(errOut, app.Text("cli.encrypt.usage"))
		return ExitUsage
	}
	var recipients []FileRecipient
	for _, text := range parsed.to {
		recipient, err := ParseFileRecipient(text)
		if err != nil {
			fmt.Fprintln(errOut, app.Message(err))
			return exitFor(err)
		}
		recipients = append(recipients, recipient)
	}
	// Mit --key kommt der eigene Chipschluessel dazu - sonst waere eine
	// Datei, die man fuer andere verschluesselt, fuer einen selbst zu.
	if parsed.label != "" {
		identity, err := fileIdentityFor(app, "encrypt", parsed.label)
		if err != nil {
			fmt.Fprintln(errOut, app.Message(err))
			return exitFor(err)
		}
		own, err := identity.Recipient()
		if err != nil {
			fmt.Fprintln(errOut, app.Message(err))
			return exitFor(err)
		}
		recipients = append(recipients, own)
	}
	if len(recipients) == 0 {
		fmt.Fprintln(errOut, app.Text("cli.encrypt.usage"))
		return ExitUsage
	}
	return runFileTool(app, parsed, errOut, in, out, func(src io.Reader, dst io.Writer) error {
		return FileEncrypt(parsed.format, recipients, src, dst)
	})
}

// --- osk decrypt --------------------------------------------------------------

func cliDecrypt(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	parsed, err := parseFileArgs(args)
	if err != nil || parsed.format == "" || parsed.label == "" {
		fmt.Fprintln(errOut, app.Text("cli.decrypt.usage"))
		return ExitUsage
	}
	identity, err := fileIdentityFor(app, "decrypt", parsed.label)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	return runFileTool(app, parsed, errOut, in, out, func(src io.Reader, dst io.Writer) error {
		return FileDecrypt(parsed.format, identity, src, dst)
	})
}

// runFileTool oeffnet Ein- und Ausgabe und laesst die Arbeit laufen.
func runFileTool(app *App, parsed fileArgs, errOut io.Writer, in io.Reader, out io.Writer,
	work func(io.Reader, io.Writer) error) int {
	source, closeIn, err := fileOpenIn(parsed.in, in)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	defer closeIn()
	target, closeOut, err := fileOpenOut(parsed.out, out)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	if err := work(source, target); err != nil {
		closeOut()
		// Eine angefangene Ausgabedatei wird weggeraeumt. Ein halb
		// entschluesselter Klartext ist kein Ergebnis, und ein halbes
		// Chiffrat erst recht nicht.
		if parsed.out != "" {
			os.Remove(parsed.out)
		}
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	if err := closeOut(); err != nil {
		fmt.Fprintln(errOut, app.Text("error.file.write"))
		return ExitError
	}
	return ExitOK
}
