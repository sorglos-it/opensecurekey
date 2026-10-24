// Die lokale Schnittstelle fuer eigene Programme.
//
//	osk socket                  lauscht am ueblichen Platz
//	osk socket --path <adresse> an einem eigenen Platz
//	osk socket --print          nur sagen, wo er lauschen wuerde
//
// Gesprochen wird in Zeilen: je Zeile eine JSON-Abbildung hin, je Zeile eine
// zurueck. Das ist mit Absicht das einfachste Format, das es gibt - wer ein
// eigenes Programm anbinden will, soll das in jeder Sprache in zehn Zeilen
// koennen, ohne eine Bibliothek von uns.
//
//	{"was":"hello"}
//	{"was":"list"}
//	{"was":"reveal","item":4}
//	{"was":"lock"}
//
// JEDER ZUGRIFF WIRD EINZELN FREIGEGEBEN.
//
// Bei jedem `reveal` haelt dieses Programm an und fragt in seinem Fenster:
// WELCHES Programm will WELCHES Geheimnis. Erst nach einem ausdruecklichen
// Ja geht die Frage ans Geraet - wo der Anwender ein zweites Mal die Taste
// drueckt. Es gibt kein "merken" und kein "immer erlauben": genau diese
// zwei Knoepfe sind es, die eine lokale Schnittstelle von einem offenen
// Tresor unterscheiden.
//
// Den Namen des fragenden Programms nennt das Betriebssystem (SO_PEERCRED
// bzw. GetNamedPipeClientProcessId), nicht das Programm selbst. Das ist mehr
// als eine Behauptung - und trotzdem nur ein Name: ein Programm kann sich
// umbenennen. Die Frage sagt das dazu.
package classes

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"os"
	"os/signal"
	"strings"
	"sync"
	"time"
)

// Grenzen der Schnittstelle.
const (
	// socketMaxLine ist die laengste Zeile, die angenommen wird. Ohne
	// Grenze koennte ein Programm den Speicher fuellen.
	socketMaxLine = 64 * 1024
	// socketIdleTimeout beendet eine Verbindung, auf der nichts passiert.
	socketIdleTimeout = 30 * time.Minute
	// socketAskTimeout ist die Zeit, die auf die Freigabe gewartet wird.
	// Laeuft sie ab, gilt NEIN - nie ja.
	socketAskTimeout = 2 * time.Minute
	// SocketEnvName ist die Umgebungsvariable fuer einen eigenen Platz.
	SocketEnvName = "OSK_SOCKET"
	// SocketName ist der Name fuer das Geraeteprotokoll, wenn das System
	// kein Programm nennt.
	SocketName = "lokal"
)

// socketRequest ist eine Zeile hinein.
type socketRequest struct {
	ID    int    `json:"id"`
	Was   string `json:"was"`
	Item  uint32 `json:"item,omitempty"`
	Query string `json:"query,omitempty"`
}

// socketReply ist eine Zeile hinaus.
type socketReply struct {
	ID       int          `json:"id"`
	OK       bool         `json:"ok"`
	Error    string       `json:"error,omitempty"`
	Unlocked bool         `json:"unlocked,omitempty"`
	Version  string       `json:"version,omitempty"`
	Items    []socketItem `json:"items,omitempty"`
	Value    string       `json:"value,omitempty"`
}

// socketItem sind die Angaben eines Eintrags - nie das Geheimnis.
type socketItem struct {
	ID   uint32 `json:"id"`
	Name string `json:"name"`
	Site string `json:"site"`
	Type uint8  `json:"type"`
}

// socketServer bedient die lokale Schnittstelle.
type socketServer struct {
	app *App
	// askMu sorgt dafuer, dass immer nur EINE Frage im Fenster steht. Zwei
	// Fragen gleichzeitig waeren ein Weg, eine Antwort der falschen Frage
	// unterzuschieben.
	askMu  sync.Mutex
	asking *bufio.Reader
	out    io.Writer
}

// cliSocket startet die lokale Schnittstelle.
func cliSocket(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	address, printOnly, err := parseAgentFlags(args[1:])
	if err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return ExitUsage
	}
	if address == "" {
		address = DefaultSocketAddress()
	}
	if printOnly {
		fmt.Fprintln(out, address)
		return ExitOK
	}
	// Erst verbinden, dann lauschen: eine Schnittstelle, die erst beim
	// ersten Zugriff merkt, dass kein Geraet steckt, laesst das fremde
	// Programm ohne Erklaerung scheitern.
	if _, err := app.Client(); err != nil {
		fmt.Fprintln(errOut, app.Message(err))
		return exitFor(err)
	}
	listener, err := ListenAgent(address)
	if err != nil {
		fmt.Fprintln(errOut, app.Message(socketError(err)))
		return ExitError
	}
	defer listener.Close()

	server := &socketServer{app: app, asking: bufio.NewReader(in), out: out}
	fmt.Fprintf(out, app.Text("cli.socket.started")+"\n", listener.Address())
	fmt.Fprintln(out, app.Text("socket.claim"))

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

	for {
		conn, peer, acceptErr := listener.Accept()
		if acceptErr != nil {
			// Ein geschlossener Lauscher ist das gewollte Ende; alles
			// andere ist ein Fehler und wird gesagt.
			//
			// Gemessen am 24.10.2026: hier stand "return ExitOK" ohne
			// Unterscheidung. Ein falsch geschriebener Pipe-Name liess das
			// Programm die Startzeile ausgeben und dann WORTLOS enden - es
			// sah aus, als lausche es, und tat nichts. Ein stiller
			// Ausstiegscode 0 ist die teuerste Art zu scheitern.
			if listenerClosed(acceptErr) {
				return ExitOK
			}
			fmt.Fprintln(errOut, app.Message(socketError(acceptErr)))
			return ExitError
		}
		// Die Verbindungen werden NACHEINANDER bedient, nicht nebeneinander.
		// Die Freigabe ist ohnehin einzeln, und zwei Programme, die
		// gleichzeitig fragen, machen die Frage im Fenster unlesbar.
		server.serve(conn, peer)
	}
}

// socketError macht aus einem Fehler des Lauschers einen mit eigenem Text -
// die Meldungen des Agenten passen hier nicht.
func socketError(err error) error {
	problem, ok := err.(*Problem)
	if !ok {
		return err
	}
	switch problem.Key {
	case "error.agent.listen":
		return &Problem{Key: "error.socket.listen", Args: problem.Args}
	case "error.agent.inuse":
		return &Problem{Key: "error.socket.inuse", Args: problem.Args}
	case "error.agent.closed":
		return Fail("error.socket.closed")
	}
	return err
}

// DefaultSocketAddress nennt den Platz der lokalen Schnittstelle. Er ist ein
// anderer als der des ssh-Agenten: wer ssh erreichen darf, soll nicht
// automatisch den ganzen Tresor erreichen duerfen.
func DefaultSocketAddress() string {
	if custom := os.Getenv(SocketEnvName); custom != "" {
		return custom
	}
	return defaultSocketPath()
}

// serve bedient eine Verbindung, bis sie endet.
func (s *socketServer) serve(conn net.Conn, peer string) {
	defer conn.Close()
	name := socketAskedBy(peer)
	reader := bufio.NewReaderSize(conn, socketMaxLine)
	encoder := json.NewEncoder(conn)
	for {
		if err := conn.SetDeadline(time.Now().Add(socketIdleTimeout)); err != nil {
			return
		}
		line, err := reader.ReadString('\n')
		if err != nil {
			return
		}
		if len(line) > socketMaxLine {
			return
		}
		var request socketRequest
		if err := json.Unmarshal([]byte(strings.TrimSpace(line)), &request); err != nil {
			if err := encoder.Encode(socketReply{
				Error: s.app.Text("error.native.format"),
			}); err != nil {
				return
			}
			continue
		}
		reply := s.handle(request, name, peer)
		if err := encoder.Encode(reply); err != nil {
			return
		}
	}
}

// socketAskedBy baut den Namen fuer das Geraeteprotokoll.
func socketAskedBy(peer string) string {
	if peer == "" {
		return SocketName
	}
	return SocketName + ": " + peer
}

func (s *socketServer) handle(request socketRequest, name, peer string) socketReply {
	reply := socketReply{ID: request.ID}
	client, err := s.app.Client()
	if err != nil {
		reply.Error = s.app.Message(err)
		return reply
	}
	access := NewAccess(client, name)

	switch request.Was {
	case "hello":
		info, helloErr := client.Hello()
		if helloErr != nil {
			reply.Error = s.app.Message(helloErr)
			return reply
		}
		reply.OK = true
		reply.Unlocked = info.Unlocked
		reply.Version = Version
		return reply

	case "lock":
		if lockErr := client.Lock(); lockErr != nil {
			reply.Error = s.app.Message(lockErr)
			return reply
		}
		reply.OK = true
		return reply

	case "list":
		items, listErr := access.List(0, request.Query)
		if listErr != nil {
			reply.Error = s.app.Message(listErr)
			return reply
		}
		reply.OK = true
		reply.Items = make([]socketItem, 0, len(items))
		for _, item := range items {
			reply.Items = append(reply.Items, socketItem{
				ID: item.ID, Name: item.Name, Site: item.Site,
				Type: uint8(item.Type),
			})
		}
		return reply

	case "reveal":
		items, listErr := access.List(0, "")
		if listErr != nil {
			reply.Error = s.app.Message(listErr)
			return reply
		}
		var found *Item
		for index := range items {
			if items[index].ID == request.Item {
				found = &items[index]
				break
			}
		}
		if found == nil {
			reply.Error = s.app.Text("error.device.notfound")
			return reply
		}
		// HIER wird gefragt. Vor dem Geraet, vor allem anderen.
		if !s.ask(peer, found.Name) {
			reply.Error = s.app.Text("error.socket.denied")
			return reply
		}
		secret, revealErr := access.Reveal(found.ID)
		if revealErr != nil {
			reply.Error = s.app.Message(revealErr)
			return reply
		}
		reply.OK = true
		reply.Value = string(secret)
		Wipe(secret)
		return reply
	}

	reply.Error = s.app.Text("error.native.unknown")
	return reply
}

// ask stellt die Frage im Fenster und wartet auf die Antwort.
//
// Nein ist die Vorgabe: eine leere Zeile, ein Zeitablauf, ein geschlossener
// Eingang - alles heisst nein. Nur ein ausdrueckliches Ja ist ein Ja. Das
// ist der Unterschied zwischen einer Rueckfrage und einer Formalitaet.
func (s *socketServer) ask(peer, item string) bool {
	s.askMu.Lock()
	defer s.askMu.Unlock()

	who := peer
	if who == "" {
		who = s.app.Text("error.internal")
	}
	fmt.Fprintf(s.out, "\n"+s.app.Text("socket.ask"), who, item)

	answer := make(chan string, 1)
	go func() {
		line, err := s.asking.ReadString('\n')
		if err != nil && line == "" {
			answer <- ""
			return
		}
		answer <- line
	}()

	var said string
	select {
	case said = <-answer:
	case <-time.After(socketAskTimeout):
		fmt.Fprintln(s.out, s.app.Text("error.socket.timeout"))
		return false
	}
	if !isYes(said) {
		fmt.Fprintln(s.out, s.app.Text("socket.refused"))
		return false
	}
	fmt.Fprintln(s.out, s.app.Text("socket.granted"))
	return true
}

// isYes nimmt nur die Zustimmung, die in den fuenf Sprachen ueblich ist -
// und nichts sonst. "ja", "j", "yes", "y", "oui", "o", "si", "s".
func isYes(line string) bool {
	switch strings.ToLower(strings.TrimSpace(line)) {
	case "j", "ja", "y", "yes", "o", "oui", "s", "si", "sí", "sim":
		return true
	}
	return false
}
