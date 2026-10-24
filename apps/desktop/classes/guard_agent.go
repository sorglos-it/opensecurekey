// Der lokale Dienst, an dem die Einbindungs-Bibliothek osk-guard haengt.
//
// Warum es ihn gibt: ein fremdes Programm in C, PHP, Python oder .NET soll
// den Stick in wenigen Zeilen benutzen koennen, ohne USB-HID, CBOR und
// Rahmen nachzubauen. Der Dienst spricht darum eine Sprache, die sich in
// jeder dieser Sprachen in hundert Zeilen lesen laesst: eine Zeile hin, eine
// Zeile zurueck, jeder Wert in Hex.
//
// Wer darf: nur Programme auf demselben Rechner - der Dienst lauscht allein
// auf der Rueckschleife. Mehr Riegel gibt es nicht und soll es nicht geben;
// was dahinter schuetzt, ist das Geraet selbst, das je Freigabe fragt und
// jeden Zugriff protokolliert. Was das fuer den Dongle-Schutz bedeutet,
// steht in docs/dongle.md: die Bibliothek prueft die Unterschrift SELBST
// und glaubt dem Dienst kein Ja.
package classes

import (
	"bufio"
	"encoding/hex"
	"errors"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"
)

// Grenzen des Dienstes.
const (
	GuardAgentVersion = 1
	guardAgentMaxLine = 4 << 20 // 1 MiB Nutzdaten in Hex plus Luft
	guardAgentIdle    = 5 * time.Minute
)

// GuardAgent bedient die Bibliothek auf der Rueckschleife.
type GuardAgent struct {
	guard    *Guard
	listener net.Listener
	wait     sync.WaitGroup
}

// NewGuardAgent oeffnet den Port. Port 0 nimmt einen freien; die Adresse
// steht danach in Address(). Nur 127.0.0.1 - eine andere Adresse waere ein
// Dienst im Netz, und der Stick steckt nun einmal hier.
func NewGuardAgent(guard *Guard, port int) (*GuardAgent, error) {
	if guard == nil {
		return nil, Fail("error.guard.nodevice")
	}
	if port < 0 || port > 65535 {
		return nil, Fail("error.guard.port")
	}
	listener, err := net.Listen("tcp", "127.0.0.1:"+strconv.Itoa(port))
	if err != nil {
		return nil, Fail("error.guard.listen", err.Error())
	}
	agent := &GuardAgent{guard: guard, listener: listener}
	agent.wait.Add(1)
	go agent.serve()
	return agent, nil
}

// Address nennt Rechner und Port.
func (a *GuardAgent) Address() string { return a.listener.Addr().String() }

// Close haelt an und wartet, bis keine Verbindung mehr offen ist.
func (a *GuardAgent) Close() error {
	err := a.listener.Close()
	a.wait.Wait()
	return err
}

func (a *GuardAgent) serve() {
	defer a.wait.Done()
	for {
		conn, err := a.listener.Accept()
		if err != nil {
			return
		}
		a.wait.Add(1)
		go func() {
			defer a.wait.Done()
			a.handle(conn)
		}()
	}
}

func (a *GuardAgent) handle(conn net.Conn) {
	defer conn.Close()
	reader := bufio.NewReaderSize(conn, 64*1024)
	writer := bufio.NewWriterSize(conn, 64*1024)
	for {
		if err := conn.SetReadDeadline(time.Now().Add(guardAgentIdle)); err != nil {
			return
		}
		line, err := guardAgentReadLine(reader)
		if err != nil {
			if errors.Is(err, errGuardLineTooLong) {
				writer.WriteString("ERR error.guard.size\n")
				writer.Flush()
			}
			return
		}
		if strings.TrimSpace(line) == "" {
			continue
		}
		answer := a.answer(line)
		if _, err := writer.WriteString(answer + "\n"); err != nil {
			return
		}
		if err := writer.Flush(); err != nil {
			return
		}
	}
}

var errGuardLineTooLong = errors.New("line too long")

// guardAgentReadLine liest eine Zeile mit Obergrenze. bufio.ReadString
// allein wuerde beliebig viel Speicher annehmen.
func guardAgentReadLine(reader *bufio.Reader) (string, error) {
	var out strings.Builder
	for {
		part, more, err := reader.ReadLine()
		if err != nil {
			return "", err
		}
		if out.Len()+len(part) > guardAgentMaxLine {
			return "", errGuardLineTooLong
		}
		out.Write(part)
		if !more {
			return out.String(), nil
		}
	}
}

// answer beantwortet genau eine Zeile. Jedes Argument und jeder Wert ist
// Hex in Kleinbuchstaben - ein Zeichen, das dort nicht hingehoert, fuehrt
// zur Abweisung und nicht zu einem geratenen Wert.
func (a *GuardAgent) answer(line string) string {
	fields := strings.Fields(line)
	if len(fields) == 0 {
		return guardAgentErr(Fail("error.guard.request"))
	}
	verb := strings.ToLower(fields[0])
	args := fields[1:]
	switch verb {
	case "ping":
		return "OK v=" + strconv.Itoa(GuardAgentVersion)
	case "attest":
		return a.answerAttest(args)
	case "seal":
		return a.answerSeal(args, true)
	case "unseal":
		return a.answerSeal(args, false)
	case "license":
		return a.answerLicense(args)
	case "nodepub":
		return a.answerNodePub(args)
	case "nodesign":
		return a.answerNodeSign(args)
	case "nodeagree":
		return a.answerNodeAgree(args)
	}
	return guardAgentErr(Fail("error.guard.verb"))
}

// answerAttest reicht die Frage des Aufrufers unveraendert zum Geraet
// weiter. Der Zufallswert kommt also vom geschuetzten Programm, nicht von
// hier - sonst wuerde die Bibliothek die Frische ihrer eigenen Frage nicht
// kennen, und das Pruefen waere wertlos.
func (a *GuardAgent) answerAttest(args []string) string {
	if len(args) != 2 {
		return guardAgentErr(Fail("error.guard.request"))
	}
	nonce, err := guardAgentHex(args[0], GuardNonceLen, GuardNonceLen)
	if err != nil {
		return guardAgentErr(err)
	}
	binHash, err := guardAgentHex(args[1], GuardHashLen, GuardHashLen)
	if err != nil {
		return guardAgentErr(err)
	}
	answer, err := a.guard.AskWith(nonce, binHash)
	if err != nil {
		return guardAgentErr(err)
	}
	return "OK sig=" + hex.EncodeToString(answer.Signature) +
		" pub=" + hex.EncodeToString(answer.PublicKey)
}

func (a *GuardAgent) answerSeal(args []string, closing bool) string {
	if len(args) != 3 {
		return guardAgentErr(Fail("error.guard.request"))
	}
	label, err := guardAgentText(args[0], sealMaxLabel)
	if err != nil {
		return guardAgentErr(err)
	}
	aad, err := guardAgentHex(args[1], 0, sealMaxPlain)
	if err != nil {
		return guardAgentErr(err)
	}
	body, err := guardAgentHex(args[2], 1, sealMaxPlain+sealHeaderLen+sealTagLen)
	if err != nil {
		return guardAgentErr(err)
	}
	if closing {
		blob, sealErr := a.guard.Seal(label, aad, body)
		if sealErr != nil {
			return guardAgentErr(sealErr)
		}
		return "OK blob=" + hex.EncodeToString(blob)
	}
	plain, openErr := a.guard.Unseal(label, aad, body)
	if openErr != nil {
		return guardAgentErr(openErr)
	}
	defer Wipe(plain)
	return "OK plain=" + hex.EncodeToString(plain)
}

func (a *GuardAgent) answerLicense(args []string) string {
	if len(args) != 1 {
		return guardAgentErr(Fail("error.guard.request"))
	}
	product, err := guardAgentText(args[0], MaxNameLen)
	if err != nil {
		return guardAgentErr(err)
	}
	license, err := a.guard.License(product)
	if err != nil {
		return guardAgentErr(err)
	}
	tiers := make([]string, 0, len(license.Tiers))
	for _, tier := range license.Tiers {
		tiers = append(tiers, hex.EncodeToString([]byte(tier)))
	}
	joined := strings.Join(tiers, ",")
	if joined == "" {
		joined = "-"
	}
	return "OK product=" + hex.EncodeToString([]byte(license.Product)) +
		" holder=" + guardAgentOptional(license.Holder) +
		" number=" + guardAgentOptional(license.Number) +
		" tiers=" + joined +
		" seats=" + strconv.FormatUint(uint64(license.Seats), 10) +
		" issued=" + strconv.FormatInt(license.IssuedUTC, 10) +
		" expires=" + strconv.FormatInt(license.ExpiresUTC, 10)
}

// guardAgentOptional schreibt einen Bindestrich fuer "nicht gesetzt" - ein
// leerer Wert hinter einem Gleichheitszeichen waere in einer Zeile mit
// Leerzeichen als Trenner nicht zu lesen.
func guardAgentOptional(text string) string {
	if text == "" {
		return "-"
	}
	return hex.EncodeToString([]byte(text))
}

func (a *GuardAgent) answerNodePub(args []string) string {
	if len(args) != 2 {
		return guardAgentErr(Fail("error.guard.request"))
	}
	label, err := guardAgentText(args[0], NodeKeyLabelMax)
	if err != nil {
		return guardAgentErr(err)
	}
	curve, err := guardAgentText(args[1], 16)
	if err != nil {
		return guardAgentErr(err)
	}
	key, err := a.guard.NodePublic(label, curve)
	if err != nil {
		return guardAgentErr(err)
	}
	return "OK pub=" + hex.EncodeToString(key.PublicKey) +
		" curve=" + hex.EncodeToString([]byte(key.Curve))
}

func (a *GuardAgent) answerNodeSign(args []string) string {
	if len(args) != 2 {
		return guardAgentErr(Fail("error.guard.request"))
	}
	label, err := guardAgentText(args[0], NodeKeyLabelMax)
	if err != nil {
		return guardAgentErr(err)
	}
	job, err := guardAgentHex(args[1], 1, NodeJobMax)
	if err != nil {
		return guardAgentErr(err)
	}
	signature, err := a.guard.NodeSign(label, job)
	if err != nil {
		return guardAgentErr(err)
	}
	return "OK sig=" + hex.EncodeToString(signature)
}

func (a *GuardAgent) answerNodeAgree(args []string) string {
	if len(args) != 2 {
		return guardAgentErr(Fail("error.guard.request"))
	}
	label, err := guardAgentText(args[0], NodeKeyLabelMax)
	if err != nil {
		return guardAgentErr(err)
	}
	peer, err := guardAgentHex(args[1], NodeSecretLen, NodeSecretLen)
	if err != nil {
		return guardAgentErr(err)
	}
	secret, err := a.guard.NodeAgree(label, peer)
	if err != nil {
		return guardAgentErr(err)
	}
	defer Wipe(secret)
	return "OK sec=" + hex.EncodeToString(secret)
}

// guardAgentHex liest einen Hexwert und prueft seine Laenge.
func guardAgentHex(text string, min, max int) ([]byte, error) {
	if text == "-" {
		if min > 0 {
			return nil, Fail("error.guard.request")
		}
		return nil, nil
	}
	if len(text) > 2*max || len(text)%2 != 0 {
		return nil, Fail("error.guard.request")
	}
	raw, err := hex.DecodeString(text)
	if err != nil {
		return nil, Fail("error.guard.request")
	}
	if len(raw) < min || len(raw) > max {
		return nil, Fail("error.guard.request")
	}
	return raw, nil
}

// guardAgentText liest einen Text, der als Hex ankommt. Steuerzeichen
// werden abgewiesen: sie wuerden das Geraeteprotokoll verunstalten.
func guardAgentText(text string, max int) (string, error) {
	raw, err := guardAgentHex(text, 1, max)
	if err != nil {
		return "", err
	}
	out := string(raw)
	if hasControl(out) {
		return "", Fail("error.guard.request")
	}
	return out, nil
}

// guardAgentErr macht aus einem Fehler eine Zeile. Der Schluessel geht
// unveraendert hinaus, damit die Bibliothek in jeder Sprache dieselbe
// Ursache sieht - und error.guard.noecdh bleibt error.guard.noecdh.
func guardAgentErr(err error) string {
	var problem *Problem
	if errors.As(err, &problem) {
		return "ERR " + problem.Key
	}
	return "ERR error.guard.internal"
}

// AskWith ist Ask mit vorgegebener Frage: der lokale Dienst reicht die Frage
// des Aufrufers durch, statt eine eigene zu wuerfeln.
func (g *Guard) AskWith(nonce, binHash []byte) (GuardAnswer, error) {
	if len(nonce) != GuardNonceLen || len(binHash) != GuardHashLen {
		return GuardAnswer{}, Fail("error.guard.request")
	}
	answer, err := g.client.Call(CmdAttest,
		CborMap{"ch": nonce, "bh": binHash}, readTimeout)
	if err != nil {
		return GuardAnswer{}, err
	}
	signature, ok := CborBytes(answer, "sig")
	if !ok || len(signature) != GuardSigLen {
		return GuardAnswer{}, Fail("error.guard.answer")
	}
	public, ok := CborBytes(answer, "pub")
	if !ok || len(public) != GuardKeyLen {
		return GuardAnswer{}, Fail("error.guard.answer")
	}
	return GuardAnswer{Nonce: nonce, BinHash: binHash,
		Signature: signature, PublicKey: public}, nil
}
