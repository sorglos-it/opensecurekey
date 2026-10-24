// Dongle-Schutz, Stufe 2: ein Schluessel, der den Stick nie verlaesst.
//
// Das ist die Stufe, die wirklich etwas haelt. Stufe 1 ist eine Ja/Nein-Frage
// im Programm und laesst sich herausschneiden. Hier dagegen liegt der
// Schluessel im Chip: ohne Stick gibt es ihn nicht, und was damit verschlossen
// ist - eine Konfiguration, ein Datenmodell, nachgeladener Code - bleibt
// ohne Stick unlesbar. Kein Umgehen, weil nichts zu umgehen ist: die Daten
// sind einfach nicht da.
//
// Die ehrliche Grenze steht in docs/dongle.md: der abgeleitete Schluessel
// kommt zur Benutzung in den Arbeitsspeicher des Rechners, wie jedes
// Geheimnis aus dem Tresor. Wer zur Laufzeit mitliest, sieht ihn. Was er
// nicht sieht, ist das Grundgeheimnis im Chip - und ohne Stick kann er
// nichts aufmachen, was er nicht vorher mitgelesen hat.
package classes

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/hkdf"
	"crypto/rand"
	"crypto/sha256"
)

// Aufbau einer verschlossenen Datei. Alles Feste steht vorn, damit ein
// Leser den Rest nicht raten muss.
const (
	sealMagic     = "OSKSEAL1"
	sealSaltLen   = 16
	sealNonceLen  = 12 // AES-GCM
	sealTagLen    = 16
	SealKeyLen    = 32
	sealMaxPlain  = 1 << 20 // 1 MiB: fuer Schluessel und Konfiguration, nicht fuer Filme
	sealMaxLabel  = 64
	sealHeaderLen = len(sealMagic) + 1 + sealSaltLen + sealNonceLen
)

// sealVersion steht im Kopf, damit ein spaeteres Verfahren die alten Dateien
// noch lesen kann.
const sealVersion = 1

// DeriveKey laesst den Chip einen Schluessel aus dem Grundgeheimnis
// ableiten. Gleiches label ergibt immer denselben Schluessel; das
// Grundgeheimnis kommt dabei nicht heraus (osk_se050_hkdf).
func (g *Guard) DeriveKey(label string) ([]byte, error) {
	if label == "" || len(label) > sealMaxLabel || hasControl(label) {
		return nil, Fail("error.guard.label")
	}
	info := sealInfo(label)
	answer, err := g.client.Call(CmdDerive,
		CborMap{"info": info, "n": uint64(SealKeyLen)}, readTimeout)
	if err != nil {
		return nil, err
	}
	key, ok := CborBytes(answer, "key")
	if !ok || len(key) != SealKeyLen {
		return nil, Fail("error.guard.answer")
	}
	return key, nil
}

// sealInfo ist die Angabe, mit der der Chip den Schluessel dieser
// Bezeichnung ableitet. Der Zweck steht davor, damit dieselbe Bezeichnung
// in einem anderen Zusammenhang nie denselben Schluessel ergibt. HIER sitzt
// die Bindung der Bezeichnung - an genau einer Stelle, siehe sealCipher.
func sealInfo(label string) []byte {
	out := make([]byte, 0, len(guardSealDomain)+1+len(label))
	out = append(out, guardSealDomain...)
	out = append(out, 0)
	return append(out, label...)
}

// Seal verschliesst Daten mit einem Schluessel aus dem Chip. aad wird
// mitgerechnet, aber nicht gespeichert: wer aufmachen will, muss denselben
// Wert kennen (etwa den Dateinamen oder die Programmversion).
func (g *Guard) Seal(label string, aad, plain []byte) ([]byte, error) {
	if len(plain) == 0 || len(plain) > sealMaxPlain {
		return nil, Fail("error.guard.size")
	}
	deviceKey, err := g.DeriveKey(label)
	if err != nil {
		return nil, err
	}
	defer Wipe(deviceKey)

	salt := make([]byte, sealSaltLen)
	nonce := make([]byte, sealNonceLen)
	if _, err := rand.Read(salt); err != nil {
		return nil, Fail("error.guard.random")
	}
	if _, err := rand.Read(nonce); err != nil {
		return nil, Fail("error.guard.random")
	}
	aead, fileKey, err := sealCipher(deviceKey, salt)
	if err != nil {
		return nil, err
	}
	defer Wipe(fileKey)

	head := sealHeader(salt, nonce)
	out := aead.Seal(head, nonce, plain, sealAAD(aad))
	return out, nil
}

// Unseal macht wieder auf. Ohne Stick gibt es den Schluessel nicht, und
// dann endet das hier mit error.guard.nodevice - nicht mit halben Daten.
func (g *Guard) Unseal(label string, aad, blob []byte) ([]byte, error) {
	salt, nonce, body, err := sealSplit(blob)
	if err != nil {
		return nil, err
	}
	deviceKey, err := g.DeriveKey(label)
	if err != nil {
		return nil, err
	}
	defer Wipe(deviceKey)

	aead, fileKey, err := sealCipher(deviceKey, salt)
	if err != nil {
		return nil, err
	}
	defer Wipe(fileKey)

	plain, err := aead.Open(nil, nonce, body, sealAAD(aad))
	if err != nil {
		return nil, Fail("error.guard.sealbroken")
	}
	return plain, nil
}

// sealHeader schreibt den festen Kopf.
func sealHeader(salt, nonce []byte) []byte {
	head := make([]byte, 0, sealHeaderLen)
	head = append(head, sealMagic...)
	head = append(head, sealVersion)
	head = append(head, salt...)
	head = append(head, nonce...)
	return head
}

// sealSplit liest den Kopf und prueft jede Angabe darin, bevor etwas
// entschluesselt wird. Eine zu kurze Datei ist ein Formatfehler, kein
// falscher Schluessel - beides muss sich unterscheiden lassen.
func sealSplit(blob []byte) (salt, nonce, body []byte, err error) {
	if len(blob) < sealHeaderLen+sealTagLen {
		return nil, nil, nil, Fail("error.guard.sealformat")
	}
	if string(blob[:len(sealMagic)]) != sealMagic {
		return nil, nil, nil, Fail("error.guard.sealformat")
	}
	if blob[len(sealMagic)] != sealVersion {
		return nil, nil, nil, Fail("error.guard.sealversion")
	}
	at := len(sealMagic) + 1
	salt = blob[at : at+sealSaltLen]
	at += sealSaltLen
	nonce = blob[at : at+sealNonceLen]
	at += sealNonceLen
	return salt, nonce, blob[at:], nil
}

// sealCipher leitet den Schluessel dieser einen Datei ab. Der Chip gibt je
// Bezeichnung denselben Schluessel; das Salz der Datei macht daraus je Datei
// einen eigenen, damit zwei Dateien nie denselben Schluessel und dasselbe
// Nonce treffen koennen.
//
// Die Bezeichnung steht hier nicht mehr in der HKDF-Angabe, und das ist
// gemessen: ein Mutant, der sie wegnahm, ueberlebte jeden Fall. Gebunden
// ist sie eine Ebene tiefer, in DeriveKey - der Chip gibt fuer zwei
// Bezeichnungen zwei verschiedene Schluessel heraus. Dieselbe Bindung
// dreimal hinzuschreiben hat nicht mehr geschuetzt, sondern nur verdeckt,
// WO sie sitzt.
func sealCipher(deviceKey, salt []byte) (cipher.AEAD, []byte, error) {
	fileKey, err := hkdf.Key(sha256.New, deviceKey, salt, sealMagic, SealKeyLen)
	if err != nil {
		return nil, nil, Fail("error.guard.internal")
	}
	block, err := aes.NewCipher(fileKey)
	if err != nil {
		Wipe(fileKey)
		return nil, nil, Fail("error.guard.internal")
	}
	aead, err := cipher.NewGCM(block)
	if err != nil {
		Wipe(fileKey)
		return nil, nil, Fail("error.guard.internal")
	}
	return aead, fileKey, nil
}

// sealAAD ist, was zusaetzlich mitgerechnet wird: der Wert des Aufrufers,
// etwa der Dateiname oder die Programmversion.
//
// Die BEZEICHNUNG steht hier bewusst nicht drin, obwohl sie zuerst hier
// stand. Sie ist schon ueber die Schluesselableitung gebunden
// (sealCipher nimmt sie in die HKDF-Angabe), und eine zweite Bindung war
// nicht nur unnoetig, sondern gefaehrlich: ein Mutant, der sie hier
// wegnahm, ueberlebte jeden Fall - die Pruefung "unter anderem Namen geht
// sie nicht auf" lief gegen die frueheren Wand, nicht gegen diese Zeile.
// Code, den kein Fall toeten kann, ist kein Schutz, sondern eine
// Behauptung.
func sealAAD(extra []byte) []byte {
	return extra
}
