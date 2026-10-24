// Die Kommandozeile fuer Skripte.
//
// `osk get <name>` gab es schon. Dazu kommt hier, was ein Skript braucht:
//
//	osk keys                 oeffentliche SSH-Schluessel (authorized_keys)
//	osk env NAME=eintrag     Zuweisungen fuer die Schale, .env oder JSON
//	osk run eintrag -- prog  Programm starten, Geheimnis nur in dessen Umgebung
//	osk codes                die Ausstiegscodes, gegen die man pruefen kann
//	osk agent                den ssh-Agenten anbieten (agent_cli.go)
//	osk socket               die lokale Schnittstelle anbieten (native_socket.go)
//
// Diese Datei haelt ausserdem die Tabelle, aus der cli.go die zusaetzlichen
// Befehle nachschlaegt - so steht jeder neue Weg in seiner eigenen Datei.
//
// Zwei Regeln gelten in allen Befehlen hier:
//
//   - Ein Geheimnis steht nie in einem Aufrufparameter. Was in der
//     Befehlszeile steht, sieht jeder in der Prozessliste. Darum gibt es
//     `osk run`: dort geht das Geheimnis in die Umgebung des Kindes und
//     sonst nirgendwohin.
//   - Ein Geheimnis steht nie in einer Fehlermeldung und nie in einem
//     Protokoll. Was schiefgeht, wird mit Namen gemeldet, nicht mit Werten.
package classes

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"os/exec"
	"sort"
	"strings"
	"time"
)

// Die Ausstiegscodes. Ein Skript soll unterscheiden koennen, WARUM etwas
// nicht ging - mit einem einzigen Code waere jede Ursache gleich.
const (
	ExitOK        = 0 // alles in Ordnung
	ExitError     = 1 // Fehler, der nicht genauer unterschieden wird
	ExitUsage     = 2 // falscher Aufruf
	ExitNoDevice  = 3 // kein Geraet gefunden oder keine Verbindung
	ExitLocked    = 4 // das Geraet ist gesperrt, erst `osk unlock`
	ExitDenied    = 5 // die Freigabe am Geraet fehlt oder der Weg ist aus
	ExitNotFound  = 6 // diesen Eintrag gibt es nicht
	ExitAmbiguous = 7 // der Name passt auf mehrere Eintraege
)

// exitFor macht aus einem Fehler den Ausstiegscode. Die Zuordnung haengt am
// Schluessel des Problems, nicht an seinem Text - ein uebersetzter Text darf
// nie etwas am Verhalten aendern.
func exitFor(err error) int {
	problem, ok := err.(*Problem)
	if !ok {
		return ExitError
	}
	switch problem.Key {
	case "error.device.none", "error.sim.connect", "error.sim.address",
		"error.hid.open", "error.hid.timeout", "error.device.io":
		return ExitNoDevice
	case "error.device.locked":
		return ExitLocked
	case "error.device.denied", "error.device.unsupported":
		return ExitDenied
	case "error.device.notfound", "error.item.id":
		return ExitNotFound
	case "error.device.exists":
		return ExitAmbiguous
	}
	return ExitError
}

// cliCommand ist ein zusaetzlicher Befehl. args beginnt beim Befehlsnamen.
type cliCommand func(app *App, args []string, in io.Reader, out, errOut io.Writer) int

// extraCommands ist die Tabelle, in die cli.go nachschlaegt. Jeder Weg
// traegt sich hier ein; cli.go selbst bleibt unveraendert.
var extraCommands = map[string]cliCommand{
	"keys":   cliKeys,
	"env":    cliEnv,
	"run":    cliRun,
	"put":    cliPut,
	"log":    cliLog,
	"native": cliNativeCommand,
	"socket": cliSocket,
	"codes":  cliCodes,
	"agent":  cliAgent,
}

// ExtraCommandNames nennt die zusaetzlichen Befehle in fester Reihenfolge -
// fuer die Hilfe und fuer die Pruefungen.
func ExtraCommandNames() []string {
	out := make([]string, 0, len(extraCommands))
	for name := range extraCommands {
		out = append(out, name)
	}
	sort.Strings(out)
	return out
}

// askedByFor baut den Namen, den ein Befehl der Kommandozeile dem Geraet
// nennt. Er sagt den Weg, nicht nur das Programm: im Geraeteprotokoll soll
// stehen, ob die Oberflaeche, ein Skript oder ssh gefragt hat.
func askedByFor(app *App, route string) string {
	return CleanAskedBy(app.Config().AppName + " " + route)
}

// accessFor baut den Zugang eines Befehls.
func accessFor(app *App, route string) (*Access, error) {
	client, err := app.Client()
	if err != nil {
		return nil, err
	}
	return NewAccess(client, askedByFor(app, route)), nil
}

// --- osk codes ----------------------------------------------------------------

// cliCodes nennt die Ausstiegscodes. Ein Skript, das gegen Zahlen prueft,
// soll sie nachlesen koennen, ohne in den Quelltext zu schauen.
func cliCodes(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	if len(args) > 1 {
		fmt.Fprintln(errOut, app.Text("cli.codes.usage"))
		return ExitUsage
	}
	lines := []struct {
		code int
		key  string
	}{
		{ExitOK, "cli.code.ok"},
		{ExitError, "cli.code.error"},
		{ExitUsage, "cli.code.usage"},
		{ExitNoDevice, "cli.code.nodevice"},
		{ExitLocked, "cli.code.locked"},
		{ExitDenied, "cli.code.denied"},
		{ExitNotFound, "cli.code.notfound"},
		{ExitAmbiguous, "cli.code.ambiguous"},
	}
	for _, line := range lines {
		fmt.Fprintf(out, "%d\t%s\n", line.code, app.Text(line.key))
	}
	return ExitOK
}

// --- osk log ------------------------------------------------------------------

// cliLog liest das Geraeteprotokoll. Es ist der Ort, an dem nachzulesen ist,
// WER wann welches Geheimnis geholt hat - und damit die Gegenprobe zu jedem
// Weg, der hier gebaut wurde.
//
// Die Spalte mit dem Namen des fragenden Programms ist eine BEHAUPTUNG. Das
// steht als Zeile unter der Tabelle, nicht im Kleingedruckten: ein Programm
// kann jeden Namen angeben, und wer das Protokoll liest, muss es wissen.
func cliLog(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	limit := uint64(50)
	if len(args) == 2 {
		number, err := parseCount(args[1])
		if err != nil {
			fmt.Fprintln(errOut, app.Text("cli.log.usage"))
			return ExitUsage
		}
		limit = number
	} else if len(args) > 2 {
		fmt.Fprintln(errOut, app.Text("cli.log.usage"))
		return ExitUsage
	}
	client, err := app.Client()
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	lines, err := client.Log(limit)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	for _, line := range lines {
		when := time.Unix(line.TimeUTC, 0).UTC().Format("2006-01-02 15:04:05")
		fmt.Fprintf(out, "%s\t%s\t%s\t%d\n", when, line.Event, line.AskedBy, line.ItemID)
	}
	fmt.Fprintln(errOut, app.Text("claim.appname"))
	return ExitOK
}

// parseCount liest eine kleine Zahl. Ohne obere Grenze koennte eine Angabe
// das Geraet zu einer Antwort zwingen, die nicht mehr in eine Nachricht passt.
func parseCount(text string) (uint64, error) {
	if text == "" || len(text) > 4 {
		return 0, Fail("error.cli.flag", text)
	}
	number := uint64(0)
	for i := 0; i < len(text); i++ {
		if text[i] < '0' || text[i] > '9' {
			return 0, Fail("error.cli.flag", text)
		}
		number = number*10 + uint64(text[i]-'0')
	}
	if number == 0 {
		return 0, Fail("error.cli.flag", text)
	}
	return number, nil
}

// --- osk keys -----------------------------------------------------------------

// cliKeys gibt die oeffentlichen SSH-Schluessel aus - die Zeilen, die in
// authorized_keys gehoeren. Der oeffentliche Teil ist kein Geheimnis; um ihn
// zu errechnen, muss das Geraet den privaten aber einmal herausgeben, und
// das heisst: ein Tastendruck je Schluessel. Siehe docs/anbindungen.md.
func cliKeys(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	if len(args) > 2 {
		fmt.Fprintln(errOut, app.Text("cli.keys.usage"))
		return ExitUsage
	}
	filter := ""
	if len(args) == 2 {
		filter = args[1]
	}
	agent := NewAgent(app)
	identities, err := agent.Identities(askedByFor(app, "keys"))
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	shown := 0
	for _, id := range identities {
		if filter != "" && !strings.EqualFold(id.comment, filter) {
			continue
		}
		line, lineErr := SSHPublicLine(id.blob, id.comment)
		if lineErr != nil {
			continue
		}
		fmt.Fprintln(out, line)
		shown++
	}
	if shown == 0 {
		fmt.Fprintln(errOut, app.Text("cli.keys.none"))
		return ExitNotFound
	}
	return ExitOK
}

// --- osk put ------------------------------------------------------------------

// cliPut legt einen Eintrag an. Das Geheimnis kommt von der Standardeingabe,
// nie aus einem Aufrufparameter - was in der Befehlszeile steht, sieht jeder
// in der Prozessliste.
//
//	osk put --type sshkey --name "Mein Schluessel" --site github.com < key
//
// Ausgegeben wird nur die Nummer des neuen Eintrags, damit ein Skript damit
// weiterarbeiten kann.
func cliPut(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	item, err := parsePutFlags(args[1:])
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitUsage
	}
	// Eine Grenze beim Lesen, damit eine umgeleitete Datei den Speicher
	// nicht fuellt. Ein Byte mehr als erlaubt wird mitgelesen, damit "zu
	// lang" von "genau voll" zu unterscheiden ist.
	secret, err := io.ReadAll(io.LimitReader(in, MaxSecretLen+1))
	if err != nil {
		fmt.Fprintln(errOut, app.Text("error.item.secret"))
		return ExitUsage
	}
	secret = trimTrailingNewline(secret)
	if len(secret) == 0 || len(secret) > MaxSecretLen {
		Wipe(secret)
		fmt.Fprintln(errOut, app.Text("error.item.secret"))
		return ExitUsage
	}
	access, err := accessFor(app, "put")
	if err != nil {
		Wipe(secret)
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	id, err := access.Add(item, secret)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	fmt.Fprintln(out, id)
	return ExitOK
}

// trimTrailingNewline nimmt genau einen Zeilenumbruch am Ende weg - den
// haengt jede Schale an, und in einem Geheimnis hat er nichts zu suchen.
// Mehrere Umbrueche bleiben: bei einer Schluesseldatei gehoeren sie dazu.
func trimTrailingNewline(data []byte) []byte {
	if len(data) > 0 && data[len(data)-1] == '\n' {
		data = data[:len(data)-1]
	}
	if len(data) > 0 && data[len(data)-1] == '\r' {
		data = data[:len(data)-1]
	}
	return data
}

// parsePutFlags liest --type, --name, --site und --profile.
func parsePutFlags(args []string) (Item, error) {
	item := Item{}
	typeName := ""
	for index := 0; index < len(args); index++ {
		name, value, err := flagValue(args, &index)
		if err != nil {
			return Item{}, err
		}
		switch name {
		case "--type":
			typeName = value
		case "--name":
			item.Name = value
		case "--site":
			item.Site = value
		case "--profile":
			number := 0
			for i := 0; i < len(value); i++ {
				if value[i] < '0' || value[i] > '9' {
					return Item{}, Fail("error.item.profile")
				}
				number = number*10 + int(value[i]-'0')
				if number > 255 {
					return Item{}, Fail("error.item.profile")
				}
			}
			item.Profile = uint8(number)
		default:
			return Item{}, Fail("error.cli.flag", name)
		}
	}
	found, ok := ItemTypeByName(typeName)
	if !ok {
		return Item{}, Fail("error.item.type")
	}
	item.Type = found
	if err := CheckItem(item); err != nil {
		return Item{}, err
	}
	return item, nil
}

// flagValue liest --name wert oder --name=wert und schiebt den Zeiger weiter.
func flagValue(args []string, index *int) (string, string, error) {
	arg := args[*index]
	if !strings.HasPrefix(arg, "--") {
		return "", "", Fail("error.cli.flag", arg)
	}
	if cut := strings.IndexByte(arg, '='); cut > 0 {
		return arg[:cut], arg[cut+1:], nil
	}
	if *index+1 >= len(args) {
		return "", "", Fail("error.cli.flag", arg)
	}
	*index++
	return arg, args[*index], nil
}

// ItemTypeByName findet einen Eintragstyp an seinem kurzen Namen. Die Namen
// stammen aus den Sprachschluesseln (item.type.sshkey -> sshkey) - so gibt
// es keine zweite Liste, die auseinanderlaufen koennte.
func ItemTypeByName(name string) (ItemType, bool) {
	if name == "" {
		return 0, false
	}
	wanted := strings.ToLower(strings.TrimSpace(name))
	for _, candidate := range ItemTypes() {
		key := candidate.Key()
		short := key[strings.LastIndexByte(key, '.')+1:]
		if short == wanted {
			return candidate, true
		}
	}
	return 0, false
}

// ItemTypeNames nennt alle kurzen Namen in fester Reihenfolge.
func ItemTypeNames() []string {
	out := make([]string, 0, len(ItemTypes()))
	for _, candidate := range ItemTypes() {
		key := candidate.Key()
		out = append(out, key[strings.LastIndexByte(key, '.')+1:])
	}
	return out
}

// --- Namen von Eintraegen auf Variablennamen ----------------------------------

// wish ist ein Wunsch der Form NAME=eintrag oder nur eintrag.
type wish struct {
	variable string
	item     string
}

// parseWishes liest die Wuensche. Ohne NAME= wird der Variablenname aus dem
// Namen des Eintrags gebaut: Grossbuchstaben, alles Fremde wird zu _.
func parseWishes(args []string, prefix string) ([]wish, error) {
	out := make([]wish, 0, len(args))
	seen := map[string]bool{}
	for _, arg := range args {
		entry := wish{}
		if cut := strings.IndexByte(arg, '='); cut > 0 {
			entry.variable = arg[:cut]
			entry.item = arg[cut+1:]
		} else {
			entry.item = arg
			entry.variable = prefix + variableName(arg)
		}
		if entry.item == "" || !validVariableName(entry.variable) {
			return nil, Fail("error.cli.variable", arg)
		}
		if seen[entry.variable] {
			// Zweimal derselbe Variablenname hiesse: einer der beiden Werte
			// verschwindet, und niemand merkt welcher.
			return nil, Fail("error.cli.variable", entry.variable)
		}
		seen[entry.variable] = true
		out = append(out, entry)
	}
	return out, nil
}

// variableName macht aus einem Eintragsnamen einen Variablennamen.
func variableName(name string) string {
	out := make([]rune, 0, len(name))
	for _, r := range name {
		switch {
		case r >= 'a' && r <= 'z':
			out = append(out, r-32)
		case (r >= 'A' && r <= 'Z') || (r >= '0' && r <= '9'):
			out = append(out, r)
		default:
			out = append(out, '_')
		}
	}
	return string(out)
}

// validVariableName laesst nur zu, was jede Schale als Variablennamen
// annimmt. Alles andere waere ein Weg, Zeichen in eine Schale zu schmuggeln.
func validVariableName(name string) bool {
	if name == "" || len(name) > 128 {
		return false
	}
	if name[0] >= '0' && name[0] <= '9' {
		return false
	}
	for i := 0; i < len(name); i++ {
		c := name[i]
		ok := c == '_' || (c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
		if !ok {
			return false
		}
	}
	return true
}

// fetch holt die Geheimnisse zu den Wuenschen. Der Aufrufer muss jeden
// Puffer mit Wipe ueberschreiben.
func fetch(app *App, route string, wishes []wish) (map[string][]byte, error) {
	access, err := accessFor(app, route)
	if err != nil {
		return nil, err
	}
	items, err := access.List(0, "")
	if err != nil {
		return nil, err
	}
	out := make(map[string][]byte, len(wishes))
	clean := func() {
		for _, value := range out {
			Wipe(value)
		}
	}
	for _, want := range wishes {
		var found []Item
		for _, item := range items {
			if strings.EqualFold(item.Name, want.item) {
				found = append(found, item)
			}
		}
		switch len(found) {
		case 0:
			clean()
			return nil, Fail("error.device.notfound")
		case 1:
		default:
			// Raten waere hier das Schlimmste: das Skript bekaeme ein
			// Geheimnis, das es nicht gemeint hat.
			clean()
			return nil, Fail("error.device.exists")
		}
		secret, revealErr := access.Reveal(found[0].ID)
		if revealErr != nil {
			clean()
			return nil, revealErr
		}
		out[want.variable] = secret
	}
	return out, nil
}

// --- osk env ------------------------------------------------------------------

// cliEnv gibt Zuweisungen aus, die eine Schale oder eine .env-Datei liest.
//
//	osk env GITHUB_TOKEN=github
//	osk env --format ps github
//	osk env --format dotenv --prefix APP_ datenbank
//
// Ohne Eintragsnamen gibt der Befehl nichts aus. Das ist Absicht: "alle
// Geheimnisse ausgeben" waere der eine Aufruf, der einen ganzen Tresor in
// eine Protokolldatei schreibt.
func cliEnv(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	format, prefix, names, err := parseEnvFlags(args[1:])
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitUsage
	}
	if len(names) == 0 {
		fmt.Fprintln(errOut, app.Text("cli.env.usage"))
		return ExitUsage
	}
	wishes, err := parseWishes(names, prefix)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitUsage
	}
	values, err := fetch(app, "env", wishes)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	defer func() {
		for _, value := range values {
			Wipe(value)
		}
	}()
	text, err := formatEnv(format, wishes, values)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitUsage
	}
	defer Wipe(text)
	out.Write(text)
	return ExitOK
}

// parseEnvFlags liest --format und --prefix. Eine eigene kleine Auswertung
// statt flag aus der Standardbibliothek: flag schreibt bei einem Fehler
// selbst nach stderr und beendet das Programm - beides will die
// Kommandozeile hier selbst entscheiden.
func parseEnvFlags(args []string) (format, prefix string, names []string, err error) {
	format, prefix = "sh", "OSK_"
	for index := 0; index < len(args); index++ {
		arg := args[index]
		switch {
		case arg == "--format" || arg == "--prefix":
			if index+1 >= len(args) {
				return "", "", nil, Fail("error.cli.flag", arg)
			}
			index++
			if arg == "--format" {
				format = args[index]
			} else {
				prefix = args[index]
			}
		case strings.HasPrefix(arg, "--format="):
			format = strings.TrimPrefix(arg, "--format=")
		case strings.HasPrefix(arg, "--prefix="):
			prefix = strings.TrimPrefix(arg, "--prefix=")
		case strings.HasPrefix(arg, "-"):
			return "", "", nil, Fail("error.cli.flag", arg)
		default:
			names = append(names, arg)
		}
	}
	if prefix != "" && !validVariableName(prefix+"X") {
		return "", "", nil, Fail("error.cli.variable", prefix)
	}
	switch format {
	case "sh", "ps", "dotenv", "json":
	default:
		return "", "", nil, Fail("error.cli.format", format)
	}
	return format, prefix, names, nil
}

// formatEnv baut die Ausgabe. Die Reihenfolge ist die der Wuensche, damit
// zwei Laeufe dieselbe Datei ergeben.
func formatEnv(format string, wishes []wish, values map[string][]byte) ([]byte, error) {
	if format == "json" {
		record := make(map[string]string, len(values))
		for _, want := range wishes {
			record[want.variable] = string(values[want.variable])
		}
		data, err := json.Marshal(record)
		if err != nil {
			return nil, Fail("error.internal")
		}
		return append(data, '\n'), nil
	}
	var text []byte
	for _, want := range wishes {
		value := values[want.variable]
		switch format {
		case "sh":
			// In einfachen Anfuehrungszeichen hat kein Zeichen eine
			// Bedeutung - ausser dem Anfuehrungszeichen selbst, und das
			// wird herausgefuehrt und einzeln geschuetzt.
			text = append(text, (want.variable + "='" +
				strings.ReplaceAll(string(value), "'", `'\''`) + "'\n")...)
		case "ps":
			// PowerShell: ein einfaches Anfuehrungszeichen wird verdoppelt.
			text = append(text, ("$env:" + want.variable + " = '" +
				strings.ReplaceAll(string(value), "'", "''") + "'\n")...)
		case "dotenv":
			text = append(text, (want.variable + "=" + dotenvQuote(string(value)) + "\n")...)
		}
	}
	return text, nil
}

// dotenvQuote schreibt einen Wert so, dass die ueblichen Leser von
// .env-Dateien ihn wieder genauso herausbekommen.
func dotenvQuote(value string) string {
	out := make([]byte, 0, len(value)+2)
	out = append(out, '"')
	for i := 0; i < len(value); i++ {
		switch value[i] {
		case '\\', '"', '$', '`':
			out = append(out, '\\', value[i])
		case '\n':
			out = append(out, '\\', 'n')
		case '\r':
			out = append(out, '\\', 'r')
		default:
			out = append(out, value[i])
		}
	}
	return string(append(out, '"'))
}

// --- osk run ------------------------------------------------------------------

// cliRun startet ein Programm und legt die Geheimnisse in seine Umgebung.
//
//	osk run GITHUB_TOKEN=github -- gh pr list
//
// Das ist der sichere Weg fuer Skripte: das Geheimnis steht in keiner Datei,
// in keiner Befehlszeile und in keiner Protokollzeile - nur in der Umgebung
// des einen Kindprozesses. Was dieses Programm damit macht, liegt dann
// allerdings bei ihm; aus der Umgebung eines fremden Prozesses kann niemand
// mehr etwas ueberschreiben. Das steht auch in docs/anbindungen.md.
func cliRun(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	names, command := splitAtDashes(args[1:])
	if len(command) == 0 {
		fmt.Fprintln(errOut, app.Text("cli.run.usage"))
		return ExitUsage
	}
	_, prefix, names, err := parseEnvFlags(names)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitUsage
	}
	wishes, err := parseWishes(names, prefix)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitUsage
	}
	values, err := fetch(app, "run", wishes)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	environment := os.Environ()
	for _, want := range wishes {
		environment = append(environment, want.variable+"="+string(values[want.variable]))
	}
	for _, value := range values {
		Wipe(value)
	}
	child := exec.Command(command[0], command[1:]...)
	child.Env = environment
	child.Stdin, child.Stdout, child.Stderr = in, out, errOut
	if err := child.Run(); err != nil {
		if coded, ok := err.(*exec.ExitError); ok {
			// Der Code des Kindes ist das Ergebnis - ein Skript prueft
			// gegen ihn, nicht gegen unseren.
			return coded.ExitCode()
		}
		fmt.Fprintln(errOut, app.Text("cli.run.failed"))
		return ExitError
	}
	return ExitOK
}

// splitAtDashes teilt bei --: davor die Eintraege, danach das Programm.
func splitAtDashes(args []string) (before, after []string) {
	for index, arg := range args {
		if arg == "--" {
			return args[:index], args[index+1:]
		}
	}
	return args, nil
}
