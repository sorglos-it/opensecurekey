// Pruefungen fuer die lokale Schnittstelle.
//
// Die eine Zusicherung, um die es geht: OHNE ausdrueckliche Freigabe kommt
// kein Geheimnis heraus. Alles andere hier dient dazu, genau das messbar zu
// machen - und zu zeigen, dass die Freigabe nicht aus Versehen auch dann
// gilt, wenn niemand geantwortet hat.
package classes

import (
	"bufio"
	"bytes"
	"encoding/json"
	"io"
	"net"
	"strings"
	"testing"
	"time"
)

// socketPair baut einen Server mit vorgegebenen Antworten auf die Frage
// nach der Freigabe und gibt eine Verbindung zu ihm.
func socketPair(t *testing.T, answers string) (*socketServer, net.Conn, *App, uint32, *bytes.Buffer) {
	t.Helper()
	app := unlockedApp(t)
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	id, err := NewAccess(client, "osk put").Add(
		Item{Type: ItemLogin, Name: "Pruefzugang", Site: "example.org"},
		[]byte("geheim-pruefung"))
	if err != nil {
		t.Fatalf("Eintrag ablegen: %v", app.Message(err))
	}
	var window bytes.Buffer
	server := &socketServer{
		app:    app,
		asking: bufio.NewReader(strings.NewReader(answers)),
		out:    &window,
	}
	ours, theirs := net.Pipe()
	go server.serve(theirs, "pruefprogramm")
	t.Cleanup(func() { ours.Close() })
	return server, ours, app, id, &window
}

// say schickt eine Zeile und liest die Antwort.
func say(t *testing.T, conn net.Conn, request socketRequest) socketReply {
	t.Helper()
	if err := conn.SetDeadline(time.Now().Add(30 * time.Second)); err != nil {
		t.Fatalf("Zeitlimit: %v", err)
	}
	body, err := json.Marshal(request)
	if err != nil {
		t.Fatalf("Anfrage: %v", err)
	}
	if _, err := conn.Write(append(body, '\n')); err != nil {
		t.Fatalf("senden: %v", err)
	}
	line, err := bufio.NewReader(conn).ReadString('\n')
	if err != nil && line == "" {
		t.Fatalf("lesen: %v", err)
	}
	var reply socketReply
	if err := json.Unmarshal([]byte(line), &reply); err != nil {
		t.Fatalf("Antwort %q: %v", line, err)
	}
	return reply
}

// Mit Ja kommt das Geheimnis - und die Frage nennt Programm und Eintrag.
func TestSocketRevealNeedsYes(t *testing.T) {
	_, conn, _, id, window := socketPair(t, "j\n")
	reply := say(t, conn, socketRequest{ID: 1, Was: "reveal", Item: id})
	if !reply.OK {
		t.Fatalf("reveal: %s", reply.Error)
	}
	if reply.Value != "geheim-pruefung" {
		t.Errorf("Wert %q", reply.Value)
	}
	asked := window.String()
	// Die Frage muss BEIDES nennen: wer fragt und wonach. Eine Frage ohne
	// diese zwei Angaben ist eine Formalitaet, keine Entscheidung.
	if !strings.Contains(asked, "pruefprogramm") {
		t.Errorf("die Frage nennt das Programm nicht: %q", asked)
	}
	if !strings.Contains(asked, "Pruefzugang") {
		t.Errorf("die Frage nennt den Eintrag nicht: %q", asked)
	}
}

// Alles, was kein Ja ist, ist ein Nein - und es kommt NICHTS heraus.
func TestSocketRevealRefusedWithoutYes(t *testing.T) {
	for _, answer := range []string{"n\n", "\n", "nein\n", "vielleicht\n",
		"ja bitte nicht\n", "ok\n", "1\n", "Y E S\n"} {
		_, conn, _, id, _ := socketPair(t, answer)
		reply := say(t, conn, socketRequest{ID: 1, Was: "reveal", Item: id})
		if reply.OK {
			t.Errorf("%q wurde als Zustimmung gewertet", answer)
		}
		if reply.Value != "" {
			t.Errorf("%q: in der Antwort steht ein Wert", answer)
		}
	}
}

// Ein geschlossener Eingang ist ein Nein, kein Ja. Das ist der Fall, in dem
// `osk socket` ohne Fenster laeuft - dort darf nichts durchrutschen.
func TestSocketRefusesWithoutAnyone(t *testing.T) {
	_, conn, _, id, _ := socketPair(t, "")
	reply := say(t, conn, socketRequest{ID: 1, Was: "reveal", Item: id})
	if reply.OK || reply.Value != "" {
		t.Errorf("ohne jede Antwort kam etwas heraus: %+v", reply)
	}
}

// Die Zustimmung in allen fuenf Sprachen wird erkannt - und nur sie.
func TestSocketYesWords(t *testing.T) {
	yes := []string{"j", "ja", "J", " Ja ", "y", "yes", "YES",
		"o", "oui", "s", "si", "sí", "sim", "ja\n", "y\r\n"}
	for _, word := range yes {
		if !isYes(word) {
			t.Errorf("%q wurde nicht als Zustimmung erkannt", word)
		}
	}
	no := []string{"", " ", "n", "nein", "no", "non", "nope", "jein",
		"jawohl", "yess", "ok", "1", "true", "erlauben"}
	for _, word := range no {
		if isYes(word) {
			t.Errorf("%q wurde als Zustimmung erkannt", word)
		}
	}
}

// Die Liste nennt Angaben, nie Geheimnisse - und braucht keine Freigabe,
// weil nichts Geheimes darin steht.
func TestSocketListHasNoSecrets(t *testing.T) {
	_, conn, _, id, window := socketPair(t, "")
	reply := say(t, conn, socketRequest{ID: 1, Was: "list"})
	if !reply.OK {
		t.Fatalf("list: %s", reply.Error)
	}
	found := false
	for _, item := range reply.Items {
		if item.ID == id {
			found = true
		}
	}
	if !found {
		t.Error("der Eintrag fehlt in der Liste")
	}
	// Das Geheimnis darf in keiner Form in der Antwort stehen.
	body, err := json.Marshal(reply)
	if err != nil {
		t.Fatalf("Antwort: %v", err)
	}
	if bytes.Contains(body, []byte("geheim-pruefung")) {
		t.Error("in der Liste steht ein Geheimnis")
	}
	if window.Len() != 0 {
		t.Errorf("die Liste hat eine Frage ausgeloest: %q", window.String())
	}
}

// Eine Nummer, die es nicht gibt, loest keine Frage aus - sonst waere die
// Frage selbst ein Weg, den Tresor abzuklopfen.
func TestSocketUnknownItemAsksNothing(t *testing.T) {
	_, conn, _, _, window := socketPair(t, "j\n")
	reply := say(t, conn, socketRequest{ID: 1, Was: "reveal", Item: 9999})
	if reply.OK || reply.Value != "" {
		t.Errorf("fremde Nummer: %+v", reply)
	}
	if window.Len() != 0 {
		t.Errorf("eine fremde Nummer hat eine Frage ausgeloest: %q", window.String())
	}
}

// Kaputte Zeilen und unbekannte Wuensche werden beantwortet, nicht
// verschluckt - und sie beenden die Verbindung nicht.
func TestSocketBadLines(t *testing.T) {
	app := unlockedApp(t)
	var window bytes.Buffer
	server := &socketServer{app: app, asking: bufio.NewReader(strings.NewReader("")),
		out: &window}
	ours, theirs := net.Pipe()
	defer ours.Close()
	go server.serve(theirs, "pruefprogramm")

	if err := ours.SetDeadline(time.Now().Add(30 * time.Second)); err != nil {
		t.Fatalf("Zeitlimit: %v", err)
	}
	reader := bufio.NewReader(ours)
	for _, line := range []string{"kein json\n", "{}\n", `{"was":"zaubern"}` + "\n"} {
		if _, err := io.WriteString(ours, line); err != nil {
			t.Fatalf("senden: %v", err)
		}
		answer, err := reader.ReadString('\n')
		if err != nil {
			t.Fatalf("nach %q kam nichts: %v", line, err)
		}
		var reply socketReply
		if err := json.Unmarshal([]byte(answer), &reply); err != nil {
			t.Fatalf("Antwort %q: %v", answer, err)
		}
		if reply.OK {
			t.Errorf("%q wurde bejaht", line)
		}
		if reply.Error == "" {
			t.Errorf("%q bekam keinen Grund", line)
		}
	}
}

// Der Name fuer das Geraeteprotokoll nennt den Weg und das Programm.
func TestSocketAskedBy(t *testing.T) {
	if got := socketAskedBy("git.exe"); got != "lokal: git.exe" {
		t.Errorf("%q", got)
	}
	if got := socketAskedBy(""); got != SocketName {
		t.Errorf("%q", got)
	}
}

// Die lokale Schnittstelle und der ssh-Agent lauschen an VERSCHIEDENEN
// Plaetzen. Wer ssh erreichen darf, soll nicht nebenbei den ganzen Tresor
// erreichen duerfen.
func TestSocketAndAgentAreSeparate(t *testing.T) {
	t.Setenv(AgentEnvName, "")
	t.Setenv(SocketEnvName, "")
	if DefaultAgentAddress() == DefaultSocketAddress() {
		t.Errorf("beide lauschen auf %s", DefaultAgentAddress())
	}
}
