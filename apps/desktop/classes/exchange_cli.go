// Die Kommandozeile fuer die Wege hinein und hinaus.
//
//	osk export --to <ziel> [--out <datei>] [--profile <n>]
//	    schreibt den Tresor als CXF. Es FRAGT: das Ziel wird angezeigt,
//	    und ohne getippte Zustimmung geht nichts hinaus. Es gibt keinen
//	    Parameter, der das ueberspringt - auch keinen fuer Pruefungen.
//
//	osk import --format <cxf|csv|keepass> --in <datei> [--profile <n>]
//	    liest eine fremde Datei. Es zeigt zuerst, WER sie geschrieben hat
//	    und was aus ihr nicht in den Tresor passt, und fragt dann.
//
//	osk leaks [--all] [--url <adresse>]
//	    prueft Kennwoerter gegen eine Sammlung gestohlener Zugangsdaten.
//	    Es FRAGT, weil dafuer eine Anfrage nach draussen geht - das ist
//	    die einzige Stelle in diesem Programm, an der eine hinausgeht.
//
// Warum das Fragen hier und nicht nur in der Oberflaeche steht: ein Weg
// hinaus, den ein Skript ohne Zustimmung nehmen kann, ist ein Weg hinaus
// ohne Zustimmung. Die Bedingung sitzt darum in CxfExport selbst (siehe
// exchange_out.go); hier wird sie nur erfuellt.
package classes

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
)

func init() {
	extraCommands["export"] = cliExport
	extraCommands["import"] = cliImport
	extraCommands["leaks"] = cliLeaks
}

// exchangeArgs sind die gelesenen Aufrufparameter.
type exchangeArgs struct {
	format  string
	in      string
	out     string
	to      string
	url     string
	profile uint8
	all     bool
}

// parseExchangeArgs liest die Parameter. Ein unbekannter ist ein Fehler -
// sonst wirkt ein Tippfehler wie eine Einstellung, die niemand gesetzt hat.
// Besonders hier: ein vertipptes --to waere ein Export an ein Ziel, das
// niemand genannt hat.
func parseExchangeArgs(args []string) (exchangeArgs, error) {
	var parsed exchangeArgs
	for i := 1; i < len(args); i++ {
		name := args[i]
		value := ""
		if i+1 < len(args) {
			value = args[i+1]
		}
		switch name {
		case "--format":
			parsed.format = value
			i++
		case "--in":
			parsed.in = value
			i++
		case "--out":
			parsed.out = value
			i++
		case "--to":
			parsed.to = value
			i++
		case "--url":
			parsed.url = value
			i++
		case "--profile":
			number, err := strconv.ParseUint(value, 10, 8)
			if err != nil {
				return parsed, Fail("error.cli.flag", name)
			}
			parsed.profile = uint8(number)
			i++
		case "--all":
			parsed.all = true
		default:
			return parsed, Fail("error.cli.flag", name)
		}
	}
	return parsed, nil
}

// askYes zeigt eine Frage und liest die Antwort.
//
// Zugestimmt ist nur, wer das Wort aus der Sprachdatei tippt. Kein "alles
// ausser nein gilt als ja": eine abgebrochene Leitung, eine leere Zeile und
// ein Druck auf die Eingabetaste sind alle NEIN.
func askYes(app *App, in io.Reader, out io.Writer, question, hint string) bool {
	fmt.Fprintln(out, question)
	if hint != "" {
		fmt.Fprintln(out, hint)
	}
	fmt.Fprint(out, app.Text("cli.confirm.prompt"))
	reader := bufio.NewReaderSize(in, 64)
	line, err := reader.ReadString('\n')
	if err != nil && line == "" {
		return false
	}
	answer := strings.ToLower(strings.TrimSpace(line))
	return answer != "" && answer == strings.ToLower(app.Text("cli.confirm.word"))
}

// printReport schreibt den Bericht. Die zurueckgebliebenen Zeilen stehen
// ZULETZT und nicht zuerst: was man zuletzt liest, bleibt haengen, und das
// soll hier die Liste sein, die man nicht uebersehen darf.
func printReport(app *App, out io.Writer, report CxfReport) {
	fmt.Fprintln(out, app.Text(report.SummaryKey()))
	for _, line := range report.Taken {
		fmt.Fprintf(out, "+ %s\t%s\n", line.Name, app.Text(line.Type.Key()))
	}
	for _, line := range report.Flattened {
		fmt.Fprintf(out, "~ %s\t%s\t%s\n", line.Name, app.Text(line.Type.Key()),
			app.Text(line.ReasonKey))
	}
	if len(report.Left) == 0 {
		return
	}
	fmt.Fprintln(out, app.Text("cxf.report.leftheader"))
	for _, line := range report.Left {
		fmt.Fprintf(out, "- %s\t%s\t%s\n", line.Name, app.Text(line.Type.Key()),
			app.Text(line.ReasonKey))
	}
}

// --- osk export ---------------------------------------------------------------

func cliExport(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	parsed, err := parseExchangeArgs(args)
	if err != nil || strings.TrimSpace(parsed.to) == "" {
		fmt.Fprintln(errOut, app.Text("cli.export.usage"))
		return ExitUsage
	}
	client, err := app.Client()
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}

	// DIE ANZEIGE, DIE SAGT WOHIN. Sie steht vor allem anderen - vor dem
	// Auflisten, vor dem ersten reveal, vor dem ersten Tastendruck.
	question := fmt.Sprintf(app.Text("cxf.export.where"), parsed.to)
	if !askYes(app, in, out, question, app.Text("cxf.export.warning")) {
		fmt.Fprintln(errOut, app.Text("cxf.export.cancelled"))
		return ExitDenied
	}

	header, report, err := CxfExport(client, parsed.profile,
		CxfConsent{Destination: parsed.to, Confirmed: true})
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}

	target, closeOut, err := fileOpenOut(parsed.out, out)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	if err := CxfWrite(header, target); err != nil {
		closeOut()
		if parsed.out != "" {
			os.Remove(parsed.out)
		}
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	if err := closeOut(); err != nil {
		fmt.Fprintln(errOut, app.Text("error.cxf.write"))
		return ExitError
	}
	// Der Bericht geht nach errOut, nicht nach out: mit --out leer laeuft
	// die Datei durch die Leitung, und ein Bericht darin machte sie
	// unlesbar.
	printReport(app, errOut, report)
	return ExitOK
}

// --- osk import ---------------------------------------------------------------

func cliImport(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	parsed, err := parseExchangeArgs(args)
	if err != nil || parsed.in == "" {
		fmt.Fprintln(errOut, app.Text("cli.import.usage"))
		return ExitUsage
	}
	data, err := os.ReadFile(parsed.in)
	if err != nil {
		fmt.Fprintln(errOut, app.Text("error.cxf.read"))
		return ExitError
	}
	defer Wipe(data)

	var plan *CxfPlan
	switch parsed.format {
	case "cxf":
		header, err := CxfParse(data)
		if err != nil {
			fmt.Fprintln(errOut, app.Message(err))
			return exitFor(err)
		}
		plan, err = CxfImport(header, parsed.profile)
		if err != nil {
			fmt.Fprintln(errOut, app.Message(err))
			return exitFor(err)
		}
	case "csv":
		flat, err := CsvImport(bytes.NewReader(data), parsed.profile)
		if err != nil {
			fmt.Fprintln(errOut, app.Message(err))
			return exitFor(err)
		}
		plan = flatPlan(flat, parsed.in)
	case "keepass":
		// Das Kennwort der KeePass-Datei wird von der Standardeingabe
		// gelesen und steht nie in einem Aufrufparameter - der waere in
		// der Prozessliste zu sehen.
		fmt.Fprint(out, app.Text("cli.kdbx.pin"))
		reader := bufio.NewReaderSize(in, 256)
		line, _ := reader.ReadString('\n')
		password := []byte(strings.TrimRight(line, "\r\n"))
		line = ""
		flat, err := KdbxImport(bytes.NewReader(data), password, parsed.profile)
		if err != nil {
			fmt.Fprintln(errOut, app.Message(err))
			return exitFor(err)
		}
		plan = flatPlan(flat, parsed.in)
	default:
		fmt.Fprintln(errOut, app.Text("cli.import.usage"))
		return ExitUsage
	}
	defer plan.Wipe()

	// Erst zeigen, was kommt UND was nicht kommt, dann fragen. Die Liste
	// der zurueckgebliebenen Zeilen ist vor der Frage zu sehen und nicht
	// erst danach: sie kann die Antwort aendern.
	fmt.Fprintf(out, app.Text("cxf.import.from")+"\n", plan.From)
	printReport(app, out, CxfReport{Taken: plan.Taken, Left: plan.Left})
	question := fmt.Sprintf(app.Text("cxf.import.ask"), len(plan.Candidates))
	if !askYes(app, in, out, question, "") {
		fmt.Fprintln(errOut, app.Text("cxf.import.cancelled"))
		return ExitDenied
	}

	client, err := app.Client()
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	report, err := CxfApply(client, plan)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	printReport(app, out, report)
	return ExitOK
}

// flatPlan macht aus einem Tabellen- oder KeePass-Plan einen CXF-Plan. Die
// beiden Wege unterscheiden sich nur darin, woher die Zeilen kommen - von
// hier an ist alles gleich, und darum gibt es nur eine CxfApply.
func flatPlan(flat *CsvPlan, from string) *CxfPlan {
	plan := &CxfPlan{
		From:       from,
		Candidates: flat.Candidates,
		Taken:      flat.Taken,
		Left:       flat.Left,
		Warnings:   flat.Warnings,
	}
	// Spalten und Felder, die nicht mitkommen, gehoeren in dieselbe Liste
	// wie alles andere, was nicht mitkommt.
	seen := map[string]bool{}
	for _, name := range flat.UnusedColumns {
		if seen[name] {
			continue
		}
		seen[name] = true
		plan.Left = append(plan.Left, CxfLine{Name: name, ReasonKey: "csv.left.column"})
	}
	return plan
}

// --- osk leaks ----------------------------------------------------------------

func cliLeaks(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	parsed, err := parseExchangeArgs(args)
	if err != nil {
		fmt.Fprintln(errOut, app.Text("cli.leaks.usage"))
		return ExitUsage
	}
	checker := &LeakChecker{BaseURL: parsed.url}

	// DIE ZUSTIMMUNG. Hier geht eine Anfrage nach draussen, und der Plan
	// sagt: kein Server, keine Wolke, keine Telemetrie. Darum steht vor
	// der Frage, WAS hinausgeht - nicht "wir pruefen sicher", sondern die
	// Zahl der Zeichen und die Adresse.
	target := checker.BaseURL
	if target == "" {
		target = LeakDefaultURL
	}
	question := fmt.Sprintf(app.Text("leak.ask"), LeakPrefixLen, target)
	if !askYes(app, in, out, question, app.Text("leak.explain")) {
		fmt.Fprintln(errOut, app.Text("leak.cancelled"))
		return ExitDenied
	}
	consent := LeakConsent{Allowed: true}

	if !parsed.all {
		// Ein einzelnes Kennwort von der Standardeingabe. Es steht nie
		// in einem Aufrufparameter.
		fmt.Fprint(out, app.Text("leak.prompt"))
		reader := bufio.NewReaderSize(in, 512)
		line, _ := reader.ReadString('\n')
		password := []byte(strings.TrimRight(line, "\r\n"))
		line = ""
		result, err := checker.Check(password, consent)
		if err != nil {
			fmt.Fprintln(errOut, app.Message(err))
			return exitFor(err)
		}
		fmt.Fprintf(out, app.Text("leak.sent")+"\n", result.Sent, result.GroupSize)
		if result.Count == 0 {
			fmt.Fprintln(out, app.Text("leak.clean"))
			return ExitOK
		}
		fmt.Fprintf(out, app.Text("leak.found")+"\n", result.Count)
		return ExitOK
	}

	client, err := app.Client()
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	findings, err := LeakScan(client, checker, parsed.profile, consent)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	hits := 0
	for _, finding := range findings {
		switch {
		case finding.ReasonKey != "":
			fmt.Fprintf(out, "? %s\t%s\n", finding.Name, app.Text(finding.ReasonKey))
		case finding.Count > 0:
			hits++
			fmt.Fprintf(out, "! %s\t%d\n", finding.Name, finding.Count)
		default:
			fmt.Fprintf(out, "+ %s\n", finding.Name)
		}
	}
	fmt.Fprintf(out, app.Text("leak.summary")+"\n", len(findings), hits)
	return ExitOK
}
