// `osk native` - die Gegenstelle der Erweiterung und ihre Anmeldung.
//
//	osk native                      vom Browser gestartet, spricht ueber stdin
//	osk native --register <browser> <kennung> [<ordner>]
//
// Der zweite Aufruf legt die zwei Dateien an, die der Browser braucht, und
// sagt, wohin sie gehoeren. Er schreibt NICHT selbst in die Ordner des
// Browsers: das ist die Einstellung eines fremden Programms, und die nimmt
// dieses Programm niemandem ab. Es sagt, was zu tun ist, und der Anwender
// tut es - oder eben nicht.
//
// Warum ueberhaupt eine zweite Datei: der Browser startet den Wirt direkt
// und gibt ihm die Kennung der Erweiterung als ersten Aufrufparameter. Er
// kann ihm aber keinen Unterbefehl mitgeben. Also zeigt die Anmeldedatei auf
// ein winziges Startskript, und das ruft `osk native` auf.
package classes

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

// cliNativeCommand ist der Einstieg fuer `osk native`.
func cliNativeCommand(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	rest := args[1:]
	if len(rest) > 0 && rest[0] == "--register" {
		return cliNativeRegister(app, rest[1:], out, errOut)
	}
	return cliNative(app, args, in, out, errOut)
}

// cliNativeRegister legt Anmeldedatei und Startskript an.
func cliNativeRegister(app *App, args []string, out, errOut io.Writer) int {
	if len(args) < 2 {
		fmt.Fprintln(errOut, app.Text("cli.native.usage"))
		return ExitUsage
	}
	browser := strings.ToLower(args[0])
	if browser != "chrome" && browser != "firefox" {
		fmt.Fprintln(errOut, app.Message(Fail("error.native.browser", args[0])))
		return ExitUsage
	}
	extension := strings.TrimSpace(args[1])
	if !validExtensionID(browser, extension) {
		fmt.Fprintln(errOut, app.Message(Fail("error.native.extension", args[1])))
		return ExitUsage
	}
	folder := "."
	if len(args) > 2 {
		folder = args[2]
	}
	if err := os.MkdirAll(folder, 0o700); err != nil {
		fmt.Fprintln(errOut, app.Message(Fail("error.native.manifest")))
		return ExitError
	}

	wrapper, err := writeWrapper(folder)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitError
	}
	manifest, err := nativeManifestFor(browser, extension, wrapper)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitError
	}
	target := filepath.Join(folder, NativeHostName+".json")
	if err := writePrivateFile(target, manifest, 0o600); err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitError
	}
	fmt.Fprintf(out, app.Text("cli.native.written")+"\n", target, wrapper)
	for _, line := range nativeManifestPlaces(browser) {
		fmt.Fprintln(out, "  "+line)
	}
	return ExitOK
}

// validExtensionID prueft die Kennung. Chrome vergibt 32 Kleinbuchstaben a
// bis p; Firefox benutzt eine Kennung in geschweiften Klammern oder eine
// Adresse mit @. Alles andere ist nicht die Kennung einer Erweiterung, und
// eine falsche Kennung in der Anmeldedatei heisst: jede Erweiterung darf
// oder keine.
func validExtensionID(browser, id string) bool {
	if id == "" || len(id) > 128 || hasControl(id) {
		return false
	}
	if browser == "chrome" {
		if len(id) != 32 {
			return false
		}
		for index := 0; index < len(id); index++ {
			if id[index] < 'a' || id[index] > 'p' {
				return false
			}
		}
		return true
	}
	// Firefox: {uuid} oder name@beispiel.org
	if strings.HasPrefix(id, "{") && strings.HasSuffix(id, "}") {
		return len(id) == 38
	}
	at := strings.IndexByte(id, '@')
	return at > 0 && at < len(id)-1 && !strings.ContainsAny(id, " /\\\"")
}

// writeWrapper legt das Startskript an, das der Browser aufruft.
func writeWrapper(folder string) (string, error) {
	exe, err := os.Executable()
	if err != nil {
		return "", Fail("error.native.manifest")
	}
	if runtime.GOOS == "windows" {
		path := filepath.Join(folder, "osk-native.bat")
		// %* gibt die Kennung der Erweiterung weiter, die der Browser
		// uebergibt. Ohne sie stuende im Geraeteprotokoll nur "browser".
		body := "@echo off\r\n\"" + exe + "\" native %*\r\n"
		if err := writePrivateFile(path, []byte(body), 0o700); err != nil {
			return "", err
		}
		return path, nil
	}
	path := filepath.Join(folder, "osk-native.sh")
	body := "#!/bin/sh\nexec \"" + exe + "\" native \"$@\"\n"
	if err := writePrivateFile(path, []byte(body), 0o700); err != nil {
		return "", err
	}
	return path, nil
}

// nativeManifestFor baut die Anmeldedatei mit dem Pfad des Startskripts.
func nativeManifestFor(browser, extension, wrapper string) ([]byte, error) {
	absolute, err := filepath.Abs(wrapper)
	if err != nil {
		return nil, Fail("error.native.manifest")
	}
	return nativeManifestJSON(browser, absolute, []string{extension})
}

// nativeManifestPlaces nennt die Orte, an denen der Browser die
// Anmeldedatei sucht. Kein Ort braucht Verwalterrechte - das ist der Grund,
// warum dieser Weg ueberhaupt auf einem fremden Rechner taugt.
func nativeManifestPlaces(browser string) []string {
	if browser == "chrome" {
		switch runtime.GOOS {
		case "windows":
			return []string{
				`HKCU\Software\Google\Chrome\NativeMessagingHosts\` + NativeHostName,
				`(Standardwert = vollstaendiger Pfad der JSON-Datei)`,
			}
		case "darwin":
			return []string{
				"~/Library/Application Support/Google/Chrome/NativeMessagingHosts/",
			}
		}
		return []string{"~/.config/google-chrome/NativeMessagingHosts/"}
	}
	switch runtime.GOOS {
	case "windows":
		return []string{
			`HKCU\Software\Mozilla\NativeMessagingHosts\` + NativeHostName,
			`(Standardwert = vollstaendiger Pfad der JSON-Datei)`,
		}
	case "darwin":
		return []string{"~/Library/Application Support/Mozilla/NativeMessagingHosts/"}
	}
	return []string{"~/.mozilla/native-messaging-hosts/"}
}

// writePrivateFile schreibt atomar und nur fuer den Benutzer lesbar: erst
// daneben, dann umbenennen. Ein Stromausfall darf keine halbe Datei
// hinterlassen, die der Browser dann liest.
func writePrivateFile(path string, data []byte, mode os.FileMode) error {
	temp := path + ".new"
	if err := os.WriteFile(temp, data, mode); err != nil {
		return Fail("error.native.manifest")
	}
	if err := os.Rename(temp, path); err != nil {
		os.Remove(temp)
		return Fail("error.native.manifest")
	}
	return nil
}
