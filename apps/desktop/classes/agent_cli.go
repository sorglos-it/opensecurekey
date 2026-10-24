// `osk agent` - den ssh-Agenten anbieten.
//
//	osk agent                     lauscht am ueblichen Platz
//	osk agent --path <adresse>    an einem eigenen Platz
//	osk agent --print             nur sagen, wo er lauschen wuerde
//
// Der Befehl gibt zuerst die Zeile aus, die ssh braucht, und bleibt dann
// stehen. Beendet wird er mit Strg+C; der Platz wird dabei aufgeraeumt.
package classes

import (
	"fmt"
	"io"
	"os"
	"os/signal"
	"runtime"
	"strings"
)

// cliAgent startet den Agenten.
func cliAgent(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	address, printOnly, err := parseAgentFlags(args[1:])
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitUsage
	}
	if address == "" {
		address = DefaultAgentAddress()
	}
	if printOnly {
		fmt.Fprintln(out, address)
		return ExitOK
	}

	// Die Verbindung zum Geraet wird vor dem Lauschen aufgebaut. Ein Agent,
	// der lauscht und erst beim ersten `git push` merkt, dass kein Geraet
	// steckt, laesst ssh ohne Erklaerung scheitern.
	if _, err := app.Client(); err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	listener, err := ListenAgent(address)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitError
	}
	defer listener.Close()

	agent := NewAgent(app)
	fmt.Fprintf(out, app.Text("cli.agent.started")+"\n", listener.Address())
	fmt.Fprintln(out, AgentHint(listener.Address()))
	fmt.Fprintln(out, app.Text("claim.appname"))

	// Strg+C soll den Platz aufraeumen, nicht liegen lassen: ein Socket von
	// einem abgebrochenen Lauf sieht fuer ssh aus wie ein laufender Agent.
	stop := make(chan os.Signal, 1)
	signal.Notify(stop, os.Interrupt)
	defer signal.Stop(stop)
	done := make(chan struct{})
	go func() {
		select {
		case <-stop:
			listener.Close()
		case <-done:
		}
	}()
	defer close(done)

	if err := RunAgent(agent, listener); err != nil {
		// Ein geschlossener Lauscher ist das gewollte Ende, kein Fehler.
		// Alles andere wird gesagt: siehe listenerClosed.
		if listenerClosed(err) {
			return ExitOK
		}
		fmt.Fprintln(errOut, app.Message(Fail("error.agent.listen", address)))
		return ExitError
	}
	return ExitOK
}

// parseAgentFlags liest --path und --print.
func parseAgentFlags(args []string) (address string, printOnly bool, err error) {
	for index := 0; index < len(args); index++ {
		arg := args[index]
		switch {
		case arg == "--print":
			printOnly = true
		case arg == "--path":
			if index+1 >= len(args) {
				return "", false, Fail("error.cli.flag", arg)
			}
			index++
			address = args[index]
		case strings.HasPrefix(arg, "--path="):
			address = strings.TrimPrefix(arg, "--path=")
		default:
			return "", false, Fail("error.cli.flag", arg)
		}
	}
	return address, printOnly, nil
}

// AgentHint ist die Zeile, mit der ssh den Agenten findet. Sie unterscheidet
// sich je System, weil Windows eine Pipe statt eines Sockets benutzt - und
// sie ist der eigentliche Zweck des Startbefehls: ohne sie weiss niemand,
// was er eintragen soll.
func AgentHint(address string) string {
	if runtime.GOOS == "windows" {
		return "$env:SSH_AUTH_SOCK = '" + address + "'   # oder: ssh -o IdentityAgent=" + address
	}
	return "export SSH_AUTH_SOCK=" + address
}
