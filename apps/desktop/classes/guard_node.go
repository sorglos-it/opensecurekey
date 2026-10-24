// Server-Betrieb: der Knotenschluessel liegt im Chip, nicht als Datei auf
// der Platte. Ein Knoten unterschreibt damit seine Auftraege (Ed25519) und
// handelt Sitzungsschluessel aus (X25519). Wer den Server kopiert, kopiert
// keine Kennung mit - das ist der ganze Zweck.
//
// Zwei Dinge gehoeren dazu gesagt, und sie stehen nebeneinander in
// docs/dongle.md:
//   - Ohne die Freischaltung stuende der Server bei jeder Unterschrift still
//     und wartete auf einen Tastendruck, den niemand gibt.
//   - Mit ihr kann Schadsoftware auf demselben Server unterschreiben, solange
//     die Freischaltung gilt. Darum ist sie ausdruecklich einzuschalten, auf
//     einzelne Schluessel begrenzt, zeitlich befristet, und jeder Zugriff
//     steht im Geraeteprotokoll.
//
// Und der Fall, der alles davon betrifft: ein geliefertes SE051 kann im Werk
// so eingestellt sein, dass es ECDH verweigert (FIPS-Modus, AN12413 4.10.3.1).
// Dann gibt es keinen Schluesselaustausch im Chip. Rechnen wir ihn hilfsweise
// in der Firmware, liegt der Schluessel im Speicher des nRF52840 - und damit
// ist genau die Zusage weg, um die es hier geht. Also wird nicht gerechnet,
// sondern gesagt, was wegfaellt: NodeAgree meldet error.guard.noecdh.
package classes

import (
	"crypto/ecdh"
	"crypto/ed25519"
	"crypto/sha256"
	"errors"
	"time"
)

// Grenzen des Knotenbetriebs.
const (
	NodeKeyLabelMax = 64
	NodeJobMax      = 1 << 20 // 1 MiB Auftrag; unterschrieben wird der Hash
	NodeSecretLen   = 32      // X25519
	SessionMaxTTL   = 24 * time.Hour
)

// NodeKey ist ein Schluessel des Knotens, so wie das Geraet ihn meldet.
type NodeKey struct {
	Label     string `json:"label"`
	Curve     string `json:"curve"` // "ed25519" oder "x25519"
	PublicKey []byte `json:"publicKey"`
}

// checkLabel prueft eine Schluesselbezeichnung. Sie geht in die Unterschrift
// ein und steht im Geraeteprotokoll - Steuerzeichen haben darin nichts zu
// suchen.
func checkLabel(label string) error {
	if label == "" || len(label) > NodeKeyLabelMax || hasControl(label) {
		return Fail("error.guard.label")
	}
	return nil
}

// NodePublic holt den oeffentlichen Teil eines Knotenschluessels. Der
// private Teil kommt nie heraus; danach zu fragen gibt es keinen Befehl.
func (g *Guard) NodePublic(label, curve string) (NodeKey, error) {
	if err := checkLabel(label); err != nil {
		return NodeKey{}, err
	}
	if curve != "ed25519" && curve != "x25519" {
		return NodeKey{}, Fail("error.guard.curve")
	}
	answer, err := g.client.Call(CmdSign,
		CborMap{"k": label, "cv": curve, "pub": true}, readTimeout)
	if err != nil {
		return NodeKey{}, guardMapECDH(curve, err)
	}
	public, ok := CborBytes(answer, "pub")
	if !ok || len(public) != GuardKeyLen {
		return NodeKey{}, Fail("error.guard.answer")
	}
	return NodeKey{Label: label, Curve: curve, PublicKey: public}, nil
}

// NodeJobHash ist, was unterschrieben wird: der Hash des Auftrags. Beide
// Seiten bilden ihn mit derselben Funktion, sonst passt keine Unterschrift.
func NodeJobHash(job []byte) ([]byte, error) {
	if len(job) == 0 || len(job) > NodeJobMax {
		return nil, Fail("error.guard.size")
	}
	sum := sha256.Sum256(job)
	return sum[:], nil
}

// NodeSigned ist die Bytefolge, die das Geraet wirklich unterschreibt. Der
// Zweck steht davor, und das Geraet setzt ihn selbst - darum kann ein
// Auftrag nie als Praesenz-Antwort durchgehen und umgekehrt.
func NodeSigned(label string, jobHash []byte) []byte {
	out := make([]byte, 0, len(guardSignDomain)+2+len(label)+len(jobHash))
	out = append(out, guardSignDomain...)
	out = append(out, 0)
	out = append(out, label...)
	out = append(out, 0)
	return append(out, jobHash...)
}

// NodeSign laesst den Knoten einen Auftrag unterschreiben. Ohne
// Freischaltung verlangt das Geraet je Unterschrift einen Tastendruck.
func (g *Guard) NodeSign(label string, job []byte) ([]byte, error) {
	if err := checkLabel(label); err != nil {
		return nil, err
	}
	jobHash, err := NodeJobHash(job)
	if err != nil {
		return nil, err
	}
	answer, err := g.client.Call(CmdSign,
		CborMap{"k": label, "h": jobHash}, readTimeout)
	if err != nil {
		return nil, err
	}
	signature, ok := CborBytes(answer, "sig")
	if !ok || len(signature) != GuardSigLen {
		return nil, Fail("error.guard.answer")
	}
	return signature, nil
}

// NodeVerify prueft eine Unterschrift des anderen Knotens. Das braucht kein
// Geraet - nur den oeffentlichen Schluessel, den man vorher ausgetauscht hat.
func NodeVerify(publicKey []byte, label string, job, signature []byte) error {
	if len(publicKey) != GuardKeyLen {
		return Fail("error.guard.key")
	}
	if err := checkLabel(label); err != nil {
		return err
	}
	if len(signature) != GuardSigLen {
		return Fail("error.guard.signature")
	}
	jobHash, err := NodeJobHash(job)
	if err != nil {
		return err
	}
	if !ed25519.Verify(ed25519.PublicKey(publicKey), NodeSigned(label, jobHash), signature) {
		return Fail("error.guard.signature")
	}
	return nil
}

// NodeAgree handelt ein gemeinsames Geheimnis mit der Gegenseite aus. Kann
// der gelieferte Chip kein ECDH, kommt error.guard.noecdh - und das Programm
// sagt dann, welcher Anwendungsfall damit wegfaellt. Es wird nichts
// hilfsweise in der Firmware gerechnet; das waere die Zusage, die wir nicht
// halten koennen.
func (g *Guard) NodeAgree(label string, peerPublic []byte) ([]byte, error) {
	if err := checkLabel(label); err != nil {
		return nil, err
	}
	if len(peerPublic) != NodeSecretLen {
		return nil, Fail("error.guard.peerkey")
	}
	// Ein Punkt, den die Kurve nicht kennt, wird hier abgewiesen und nicht
	// erst vom Chip - so steht die Ursache im Programm und nicht in einem
	// Fehlercode aus der Ferne.
	if _, err := ecdh.X25519().NewPublicKey(peerPublic); err != nil {
		return nil, Fail("error.guard.peerkey")
	}
	answer, err := g.client.Call(CmdAgree,
		CborMap{"k": label, "pub": peerPublic}, readTimeout)
	if err != nil {
		return nil, guardMapECDH("x25519", err)
	}
	secret, ok := CborBytes(answer, "sec")
	if !ok || len(secret) != NodeSecretLen {
		return nil, Fail("error.guard.answer")
	}
	return secret, nil
}

// guardMapECDH macht aus dem nackten "kann ich nicht" des Geraets die
// Aussage, die der Betreiber braucht: welcher Anwendungsfall wegfaellt.
// Nur fuer den Schluesselaustausch - beim Unterschreiben heisst
// OSK_ERR_UNSUPPORTED etwas anderes.
func guardMapECDH(curve string, err error) error {
	if curve != "x25519" {
		return err
	}
	var problem *Problem
	if errors.As(err, &problem) && problem.Key == "error.device.unsupported" {
		return Fail("error.guard.noecdh")
	}
	return err
}

// GuardNoECDH sagt, ob ein Fehler genau dieser Fall ist. Damit kann ein
// Programm den fehlenden Anwendungsfall abschalten statt abzubrechen.
func GuardNoECDH(err error) bool {
	var problem *Problem
	return errors.As(err, &problem) && problem.Key == "error.guard.noecdh"
}

// SessionGrant ist die Freischaltung "einmal frei, dann ohne Tastendruck".
// Sie wird ausdruecklich gesetzt: es gibt keine Vorgabe, die sie einschaltet.
type SessionGrant struct {
	// Keys sind die Bezeichnungen der Schluessel, fuer die sie gilt. Leer
	// ist ein Fehler - eine Freischaltung fuer alles gibt es nicht.
	Keys []string
	// TTL ist die Dauer. Hoechstens SessionMaxTTL.
	TTL time.Duration
}

// SessionAllow schaltet die Freischaltung am Geraet ein. Das Geraet verlangt
// dafuer einen Tastendruck - einmal, und genau das ist der Sinn. Jeder
// spaetere Zugriff steht im Geraeteprotokoll.
func (g *Guard) SessionAllow(grant SessionGrant) error {
	if len(grant.Keys) == 0 {
		return Fail("error.guard.sessionkeys")
	}
	if grant.TTL <= 0 || grant.TTL > SessionMaxTTL {
		return Fail("error.guard.sessionttl")
	}
	keys := make([]any, 0, len(grant.Keys))
	for _, label := range grant.Keys {
		if err := checkLabel(label); err != nil {
			return err
		}
		keys = append(keys, label)
	}
	_, err := g.client.Call(CmdSession, CborMap{
		"on":  true,
		"k":   keys,
		"ttl": uint64(grant.TTL / time.Second),
	}, readTimeout)
	return err
}

// SessionEnd nimmt die Freischaltung zurueck. Das braucht keinen
// Tastendruck: weniger Rechte darf ein Programm immer verlangen.
func (g *Guard) SessionEnd() error {
	_, err := g.client.Call(CmdSession, CborMap{"on": false}, shortTimeout)
	return err
}

// SessionState ist, was das Geraet ueber die Freischaltung meldet.
type SessionState struct {
	Active     bool     `json:"active"`
	Keys       []string `json:"keys"`
	SecondsTTL uint64   `json:"secondsLeft"`
}

// SessionStatus fragt, ob eine Freischaltung gilt - damit die Oberflaeche
// es anzeigen kann, statt dass der Betreiber es glauben muss.
func (g *Guard) SessionStatus() (SessionState, error) {
	answer, err := g.client.Call(CmdSession, CborMap{"ask": true}, shortTimeout)
	if err != nil {
		return SessionState{}, err
	}
	out := SessionState{}
	if active, ok := answer["on"].(bool); ok {
		out.Active = active
	}
	out.SecondsTTL, _ = CborUint(answer, "ttl")
	if list, ok := CborList(answer, "k"); ok {
		for _, entry := range list {
			if label, isText := entry.(string); isText {
				out.Keys = append(out.Keys, label)
			}
		}
	}
	return out, nil
}
