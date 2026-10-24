// Das Token hinter PKCS#11: Sitzungen, Objekte, Unterschriften.
//
// Diese Datei kennt kein C. Sie bildet den Tresor auf das Weltbild von
// PKCS#11 ab - Steckplatz, Token, Sitzung, Objekt, Merkmal - und ist darum
// ohne C-Uebersetzer pruefbar. Die C-Seite in classes/pkcs11/ ist nur noch
// Uebersetzung von Zeigern auf diese Aufrufe.
//
// Was Firefox, Thunderbird, OpenSSL, Java und ssh von uns sehen:
//
//	ein Steckplatz, ein Token
//	je Schluessel im Tresor ein privates und ein oeffentliches Objekt
//	je Zertifikat im Tresor ein Zertifikatsobjekt
//	Unterschreiben - und sonst nichts
//
// Nicht angeboten wird alles, was Geheimnisse aus dem Geraet holen oder in
// es schreiben wuerde: kein C_GetAttributeValue auf CKA_VALUE eines privaten
// Schluessels (CKA_SENSITIVE ist wahr, CKA_EXTRACTABLE falsch), kein
// Entschluesseln, kein Erzeugen, kein Loeschen. Das Geraet entscheidet, nie
// das Programm.
package classes

import (
	"crypto"
	"crypto/ecdsa"
	"crypto/ed25519"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha1"
	"crypto/sha256"
	"crypto/sha512"
	"crypto/x509"
	"encoding/asn1"
	"encoding/pem"
	"math/big"
	"sort"
	"sync"
)

// Pkcs11Library ist der Name, den fremde Programme in ihrer Liste sehen.
const (
	Pkcs11Library      = "OpenSecureKey PKCS#11"
	Pkcs11Manufacturer = "Sorglos Thomas Weirich"
	// Pkcs11AskedBy ist der Name, den das Geraet protokolliert. Welches
	// Programm die Bibliothek geladen hat, sagt das Betriebssystem hier
	// nicht - eine Bibliothek laeuft IM fremden Programm. Darum steht hier
	// der Weg, und der Name des Programms wird dazugesetzt, wenn er zu
	// bekommen ist (siehe Pkcs11Token.SetHost).
	Pkcs11AskedBy = "pkcs11"
)

// Pkcs11ValueKind sagt, wie ein Merkmalswert auf der C-Seite zu schreiben
// ist. Die Groessen von CK_ULONG und CK_BBOOL kennt nur der C-Uebersetzer,
// darum wird hier nicht in Bytes gerechnet.
type Pkcs11ValueKind uint8

const (
	Pkcs11Bytes Pkcs11ValueKind = iota
	Pkcs11Bool
	Pkcs11Ulong
)

// Pkcs11Value ist ein Merkmalswert.
type Pkcs11Value struct {
	Kind  Pkcs11ValueKind
	Bytes []byte
	Bool  bool
	Ulong uint64
}

func pkcs11Bytes(data []byte) Pkcs11Value {
	return Pkcs11Value{Kind: Pkcs11Bytes, Bytes: data}
}

func pkcs11Text(text string) Pkcs11Value {
	return Pkcs11Value{Kind: Pkcs11Bytes, Bytes: []byte(text)}
}

func pkcs11Bool(value bool) Pkcs11Value {
	return Pkcs11Value{Kind: Pkcs11Bool, Bool: value}
}

func pkcs11Ulong(value uint64) Pkcs11Value {
	return Pkcs11Value{Kind: Pkcs11Ulong, Ulong: value}
}

// Pkcs11BytesValue, Pkcs11BoolValue und Pkcs11UlongValue bauen Merkmalswerte.
// Sie sind fuer die C-Seite da: die liest sie aus fremden Zeigern und darf
// dafuer nicht in das Innere dieser Datei greifen.
func Pkcs11BytesValue(data []byte) Pkcs11Value { return pkcs11Bytes(data) }

// Pkcs11BoolValue baut einen Wahrheitswert.
func Pkcs11BoolValue(value bool) Pkcs11Value { return pkcs11Bool(value) }

// Pkcs11UlongValue baut eine Zahl.
func Pkcs11UlongValue(value uint64) Pkcs11Value { return pkcs11Ulong(value) }

// Pkcs11ErrorFor baut einen Fehler mit einem Rueckgabewert von PKCS#11.
func Pkcs11ErrorFor(rv uint64) error { return pkcs11Fail(rv, "error.pkcs11.argument") }

// Pkcs11Attribute ist ein Merkmal in einer Suchvorlage.
type Pkcs11Attribute struct {
	Type  uint64
	Value Pkcs11Value
}

// pkcs11Object ist ein Objekt des Tokens. Der private Schluessel steht hier
// NICHT drin - nur die Nummer des Eintrags, aus dem er beim Unterschreiben
// geholt wird.
type pkcs11Object struct {
	handle     uint64
	itemID     uint32
	class      uint64
	attributes map[uint64]Pkcs11Value
	// signKind sagt, womit unterschrieben wird. Nur private Objekte haben
	// es gesetzt.
	signKind uint64
}

// Pkcs11Token ist das Token. Eine Bibliothek, ein Token, beliebig viele
// Sitzungen - PKCS#11 erlaubt mehrere, und Firefox oeffnet auch mehrere.
type Pkcs11Token struct {
	app *App

	mu       sync.Mutex
	host     string
	nextHdl  uint64
	sessions map[uint64]*pkcs11Session
	objects  []pkcs11Object
	listed   bool
	loggedIn bool
	// revealFallbacks zaehlt, wie oft ein Eintrag doch per `reveal` geholt
	// werden musste. Erwartet ist null.
	revealFallbacks int
}

// pkcs11Session ist eine Sitzung.
type pkcs11Session struct {
	handle uint64
	// search haelt das Ergebnis von C_FindObjectsInit, bis
	// C_FindObjectsFinal es wegnimmt.
	search    []uint64
	searching bool
	// signObject und signMechanism halten den Zustand zwischen C_SignInit
	// und C_Sign.
	signObject    uint64
	signMechanism uint64
	signing       bool
	// signBuffer sammelt bei C_SignUpdate. Das Geraet unterschreibt erst,
	// wenn alles da ist.
	signBuffer []byte
}

// NewPkcs11Token baut das Token auf dem Kern des Programms auf.
func NewPkcs11Token(app *App) *Pkcs11Token {
	return &Pkcs11Token{app: app, nextHdl: 1, sessions: map[uint64]*pkcs11Session{}}
}

// SetHost nennt das Programm, das die Bibliothek geladen hat. Es ist eine
// BEHAUPTUNG dieses Programms - eine Bibliothek laeuft in ihm, sie kann
// nicht nachsehen, wer sie geladen hat, ohne ihm zu glauben. Genau so steht
// es auch im Geraeteprotokoll.
func (t *Pkcs11Token) SetHost(name string) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.host = CleanAskedBy(name)
}

// askedBy ist der Name fuer das Geraeteprotokoll.
func (t *Pkcs11Token) askedBy() string {
	if t.host == "" {
		return Pkcs11AskedBy
	}
	return Pkcs11AskedBy + ": " + t.host
}

// access baut einen Zugang. Der Aufrufer haelt die Sperre.
func (t *Pkcs11Token) access() (*Access, error) {
	client, err := t.app.Client()
	if err != nil {
		return nil, err
	}
	return NewAccess(client, t.askedBy()), nil
}

// --- Steckplatz und Token ------------------------------------------------------

// Pkcs11SlotPresent sagt, ob ein Geraet steckt. PKCS#11 will das wissen,
// bevor jemand eine Sitzung oeffnet.
func (t *Pkcs11Token) SlotPresent() bool {
	return len(t.app.Devices()) > 0
}

// TokenLabel ist der Name, den der Steckplatz zeigt. Er kommt vom Geraet,
// nicht aus dem Code - ohne Geraet bleibt er leer.
func (t *Pkcs11Token) TokenLabel() (label, serial, firmware string) {
	client, err := t.app.Client()
	if err != nil {
		return "", "", ""
	}
	info, err := client.Hello()
	if err != nil {
		return "", "", ""
	}
	return Pkcs11Library, info.Serial, info.Firmware
}

// Mechanisms nennt die Verfahren, die dieses Token kann - in fester
// Reihenfolge, damit zwei Abfragen dieselbe Liste ergeben.
func (t *Pkcs11Token) Mechanisms() []uint64 {
	return []uint64{
		CkmRSAPKCS, CkmSHA1RSAPKCS, CkmSHA256RSAPKCS, CkmSHA384RSAPKCS,
		CkmSHA512RSAPKCS, CkmECDSA, CkmECDSASHA256, CkmECDSASHA384,
		CkmECDSASHA512, CkmEDDSA,
	}
}

// MechanismInfo nennt die Schluessellaengen eines Verfahrens.
func (t *Pkcs11Token) MechanismInfo(mechanism uint64) (min, max, flags uint64, err error) {
	switch mechanism {
	case CkmRSAPKCS, CkmSHA1RSAPKCS, CkmSHA256RSAPKCS, CkmSHA384RSAPKCS, CkmSHA512RSAPKCS:
		return 2048, 4096, CkfSign | CkfHW, nil
	case CkmECDSA, CkmECDSASHA256, CkmECDSASHA384, CkmECDSASHA512:
		return 256, 521, CkfSign | CkfHW, nil
	case CkmEDDSA:
		return 255, 255, CkfSign | CkfHW, nil
	}
	return 0, 0, 0, pkcs11Fail(CkrMechanismInvalid, "error.pkcs11.mechanism")
}

// --- Sitzungen -----------------------------------------------------------------

// OpenSession oeffnet eine Sitzung.
//
// Hier steht eine bewusste Abweichung vom Buchstaben der Spezifikation, und
// sie ist gemessen, nicht vermutet (24.10.2026):
//
// Die Spezifikation erlaubt einem schreibgeschuetzten Token, eine schreibende
// Sitzung abzulehnen. Genau das tat diese Umsetzung zuerst - und ssh war
// damit ausgesperrt: OpenSSH fragt in ssh-pkcs11.c IMMER mit CKF_RW_SESSION,
// ohne Rueckfall, und brach mit "C_OpenSession failed: 181" ab. Dasselbe
// trifft jedes Programm, das seine Sitzung einmal oeffnet und nicht zweimal
// fragt.
//
// Darum wird das Kennzeichen angenommen. Das kostet nichts: in der Tabelle
// der Funktionen (classes/pkcs11/pkcs11_list.c) steht bei JEDER schreibenden
// Funktion NULL - C_CreateObject, C_DestroyObject, C_SetAttributeValue,
// C_GenerateKey. Es gibt also keinen Weg, auf dem fremde Software etwas
// schreiben koennte, und CKF_WRITE_PROTECTED sagt ihr das vorher. Die
// Sitzung meldet sich weiter als nur lesend (SessionState).
func (t *Pkcs11Token) OpenSession(flags uint64) (uint64, error) {
	if flags&CkfSerialSession == 0 {
		// PKCS#11 verlangt dieses Kennzeichen; ohne es ist der Aufruf falsch.
		return 0, pkcs11Fail(CkrSessionParallelNotSupported, "error.pkcs11.session")
	}
	if !t.SlotPresent() {
		return 0, pkcs11Fail(CkrTokenNotPresent, "error.device.none")
	}
	t.mu.Lock()
	defer t.mu.Unlock()
	handle := t.nextHdl
	t.nextHdl++
	t.sessions[handle] = &pkcs11Session{handle: handle}
	return handle, nil
}

// CloseSession schliesst eine Sitzung.
func (t *Pkcs11Token) CloseSession(handle uint64) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	session, found := t.sessions[handle]
	if !found {
		return pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	Wipe(session.signBuffer)
	delete(t.sessions, handle)
	if len(t.sessions) == 0 {
		// Ohne Sitzung ist nichts mehr offen: die Objektliste gilt nicht
		// weiter, und die Anmeldung auch nicht.
		t.objects = nil
		t.listed = false
		t.loggedIn = false
	}
	return nil
}

// CloseAllSessions schliesst alle Sitzungen.
func (t *Pkcs11Token) CloseAllSessions() error {
	t.mu.Lock()
	defer t.mu.Unlock()
	for _, session := range t.sessions {
		Wipe(session.signBuffer)
	}
	t.sessions = map[uint64]*pkcs11Session{}
	t.objects = nil
	t.listed = false
	t.loggedIn = false
	return nil
}

// SessionState sagt, ob die Sitzung angemeldet ist.
func (t *Pkcs11Token) SessionState(handle uint64) (uint64, error) {
	t.mu.Lock()
	defer t.mu.Unlock()
	if _, found := t.sessions[handle]; !found {
		return 0, pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	if t.loggedIn {
		return CksRoUserFunctions, nil
	}
	return CksRoPublicSession, nil
}

// LoggedIn sagt, ob der Tresor offen ist.
func (t *Pkcs11Token) LoggedIn() bool {
	t.mu.Lock()
	defer t.mu.Unlock()
	return t.loggedIn
}

// Login oeffnet den Tresor. Der Puffer mit der PIN wird dabei
// ueberschrieben; der Aufrufer braucht ihn danach nicht mehr.
func (t *Pkcs11Token) Login(handle uint64, user uint64, pin []byte) error {
	defer Wipe(pin)
	if user != CkuUser && user != CkuContextSpecific {
		// Ein Sicherheitsbeauftragter gibt es hier nicht: das Geraet wird
		// am Geraet eingerichtet, nicht durch fremde Software.
		return pkcs11Fail(CkrUserTypeInvalid, "error.pkcs11.user")
	}
	t.mu.Lock()
	_, found := t.sessions[handle]
	already := t.loggedIn
	t.mu.Unlock()
	if !found {
		return pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	if already {
		return pkcs11Fail(CkrUserAlreadyLoggedIn, "error.pkcs11.loggedin")
	}
	client, err := t.app.Client()
	if err != nil {
		return err
	}
	copied := append([]byte(nil), pin...)
	if err := client.Unlock(copied); err != nil {
		return err
	}
	t.mu.Lock()
	t.loggedIn = true
	// Die Liste wird neu geholt: vorher war der Tresor zu und hat nichts
	// genannt.
	t.objects = nil
	t.listed = false
	t.mu.Unlock()
	return nil
}

// Logout sperrt den Tresor wieder.
func (t *Pkcs11Token) Logout(handle uint64) error {
	t.mu.Lock()
	_, found := t.sessions[handle]
	t.mu.Unlock()
	if !found {
		return pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	client, err := t.app.Client()
	if err != nil {
		return err
	}
	if err := client.Lock(); err != nil {
		return err
	}
	t.mu.Lock()
	t.loggedIn = false
	t.objects = nil
	t.listed = false
	t.mu.Unlock()
	return nil
}

// --- Objekte -------------------------------------------------------------------

// loadObjects holt die Objekte aus dem Tresor. Der Aufrufer haelt die Sperre.
//
// Der oeffentliche Teil kommt mit der Liste - ohne Freigabe, weil er kein
// Geheimnis ist (Feld `pub`, shared/protocol.md Befehl 4). Frueher holte
// diese Schleife jeden Schluessel einzeln per `reveal`, nur um Modulus,
// Kurve und Punkt zu erfahren; das kostete einen Tastendruck je Schluessel,
// der nichts schuetzte. Wer lernt, Tastendruecke wegzudruecken, drueckt auch
// den weg, der zaehlt - und das ist der beim Unterschreiben. Der bleibt:
// signWith holt das Geheimnis dort, und zwar jedes Mal.
func (t *Pkcs11Token) loadObjects() error {
	if t.listed {
		return nil
	}
	if !t.loggedIn {
		return pkcs11Fail(CkrUserNotLoggedIn, "error.device.locked")
	}
	access, err := t.access()
	if err != nil {
		return err
	}
	items, err := access.ItemsOfType(ItemSSHKey, ItemCert)
	if err != nil {
		return err
	}
	var objects []pkcs11Object
	handle := uint64(1)
	for _, item := range items {
		if len(item.Pub) > 0 {
			objects = append(objects, publicOnlyObjects(item, &handle)...)
			continue
		}
		// Rueckfall fuer ein Geraet mit aelterem Protokoll: der kostet die
		// Freigabe wie frueher. Gezaehlt, damit er nicht stillschweigend
		// greift - RevealFallbacks() macht ihn nachpruefbar.
		t.revealFallbacks++
		secret, revealErr := access.Reveal(item.ID)
		if revealErr != nil {
			// Ein Eintrag, den das Geraet nicht herausgibt, fehlt eben.
			// Die anderen bleiben brauchbar.
			continue
		}
		var built []pkcs11Object
		switch item.Type {
		case ItemSSHKey:
			built = keyObjects(item, secret, &handle)
		case ItemCert:
			built = certObjects(item, secret, &handle)
		}
		Wipe(secret)
		objects = append(objects, built...)
	}
	t.objects = objects
	t.listed = true
	return nil
}

// RevealFallbacks sagt, wie oft ein Eintrag doch noch per `reveal` geholt
// werden musste, weil das Geraet den oeffentlichen Teil nicht mitschickte.
// Erwartet ist null; die Zahl ist der Nachweis, nicht die Absicht.
func (t *Pkcs11Token) RevealFallbacks() int {
	t.mu.Lock()
	defer t.mu.Unlock()
	return t.revealFallbacks
}

// publicOnlyObjects baut die Objekte eines Eintrags allein aus dem
// oeffentlichen Teil, den die Liste mitgebracht hat.
func publicOnlyObjects(item Item, handle *uint64) []pkcs11Object {
	switch item.Type {
	case ItemSSHKey:
		pub, err := ParseSSHPublicBlob(item.Pub)
		if err != nil {
			// Lieber kein Objekt als eines, dessen Merkmale geraten sind.
			return nil
		}
		keyType, public := publicAttrsOf(pub)
		return keyObjectsFrom(item, keyType, public, handle)
	case ItemCert:
		// Ein Zertifikat ist in Gaenze oeffentlich; die Liste schickt es
		// genau so, wie es im Tresor liegt.
		return certObjects(item, item.Pub, handle)
	}
	return nil
}

// publicAttrsOf macht aus einem gelesenen oeffentlichen Schluessel die
// Merkmale, die PKCS#11 nennt: die Art des Schluessels und seine
// oeffentlichen Groessen. Ein leeres Ergebnis heisst "nicht darstellbar".
func publicAttrsOf(pub *SSHPublicKey) (uint64, map[uint64]Pkcs11Value) {
	out := map[uint64]Pkcs11Value{}
	switch {
	case pub.Ed25519 != nil:
		point, err := ecPointDER(pub.Ed25519)
		if err != nil {
			return 0, nil
		}
		out[CkaECParams] = pkcs11Bytes(curveParamsEd25519())
		out[CkaECPoint] = pkcs11Bytes(point)
		return CkkECEdwards, out
	case pub.Ecdsa != nil:
		params, paramsErr := curveParamsDER(pub.Ecdsa.Curve)
		if paramsErr != nil {
			return 0, nil
		}
		raw, rawErr := markPoint(pub.Ecdsa.Curve, pub.Ecdsa.X, pub.Ecdsa.Y)
		if rawErr != nil {
			return 0, nil
		}
		point, pointErr := ecPointDER(raw)
		if pointErr != nil {
			return 0, nil
		}
		out[CkaECParams] = pkcs11Bytes(params)
		out[CkaECPoint] = pkcs11Bytes(point)
		return CkkEC, out
	case pub.Rsa != nil:
		out[CkaModulus] = pkcs11Bytes(pub.Rsa.Modulus.Bytes())
		out[CkaModulusBits] = pkcs11Ulong(uint64(pub.Rsa.Modulus.BitLen()))
		out[CkaPublicExponent] = pkcs11Bytes(pub.Rsa.Exponent.Bytes())
		return CkkRSA, out
	}
	return 0, nil
}

// keyObjects ist der Rueckfallweg: er hat das Geheimnis und schreibt den
// oeffentlichen Teil daraus. Gelesen wird er dann wie der aus der Liste -
// zwei Wege, eine Rechnung, sonst liefen sie auseinander.
func keyObjects(item Item, secret []byte, handle *uint64) []pkcs11Object {
	key, err := ParseSSHKey(secret)
	if err != nil {
		return nil
	}
	defer key.Wipe()
	pub, pubErr := ParseSSHPublicBlob(key.Blob)
	if pubErr != nil {
		return nil
	}
	keyType, public := publicAttrsOf(pub)
	return keyObjectsFrom(item, keyType, public, handle)
}

// keyObjectsFrom baut aus Schluesselart und oeffentlichen Groessen das
// private und das oeffentliche Objekt. Der private Teil steckt in keinem von
// beiden - nur die Nummer des Eintrags, aus dem beim Unterschreiben geholt
// wird.
func keyObjectsFrom(item Item, keyType uint64, public map[uint64]Pkcs11Value,
	handle *uint64) []pkcs11Object {
	if public == nil {
		return nil
	}
	id := itemIDBytes(item.ID)
	common := map[uint64]Pkcs11Value{
		CkaToken:       pkcs11Bool(true),
		CkaLabel:       pkcs11Text(item.Name),
		CkaID:          pkcs11Bytes(id),
		CkaModifiable:  pkcs11Bool(false),
		CkaCopyable:    pkcs11Bool(false),
		CkaDestroyable: pkcs11Bool(false),
		CkaLocal:       pkcs11Bool(true),
	}
	privateAttrs := map[uint64]Pkcs11Value{
		CkaClass:   pkcs11Ulong(CkoPrivateKey),
		CkaKeyType: pkcs11Ulong(keyType),
		CkaPrivate: pkcs11Bool(true),
		CkaSign:    pkcs11Bool(true),
		// Das Entscheidende: der private Teil verlaesst das Geraet nie, und
		// das muss die fremde Software ERFAHREN, nicht erraten.
		CkaSensitive:        pkcs11Bool(true),
		CkaExtractable:      pkcs11Bool(false),
		CkaAlwaysSensitive:  pkcs11Bool(true),
		CkaNeverExtractable: pkcs11Bool(true),
		CkaDecrypt:          pkcs11Bool(false),
		CkaUnwrap:           pkcs11Bool(false),
		CkaSignRecover:      pkcs11Bool(false),
		CkaDerive:           pkcs11Bool(false),
		// Jede Unterschrift braucht die Freigabe am Geraet. Wer das weiss,
		// erklaert dem Anwender, warum er gleich eine Taste druecken soll.
		CkaAlwaysAuthenticate: pkcs11Bool(true),
	}
	publicAttrs := map[uint64]Pkcs11Value{
		CkaClass:       pkcs11Ulong(CkoPublicKey),
		CkaKeyType:     pkcs11Ulong(keyType),
		CkaPrivate:     pkcs11Bool(false),
		CkaVerify:      pkcs11Bool(true),
		CkaEncrypt:     pkcs11Bool(false),
		CkaWrap:        pkcs11Bool(false),
		CkaDerive:      pkcs11Bool(false),
		CkaSensitive:   pkcs11Bool(false),
		CkaExtractable: pkcs11Bool(true),
	}
	for key, value := range common {
		privateAttrs[key] = value
		publicAttrs[key] = value
	}
	for key, value := range public {
		// Der oeffentliche Teil gehoert an BEIDE Objekte: OpenSSL und ssh
		// fragen ihn am privaten ab.
		privateAttrs[key] = value
		publicAttrs[key] = value
	}

	privateHandle := *handle
	*handle++
	publicHandle := *handle
	*handle++
	return []pkcs11Object{
		{handle: privateHandle, itemID: item.ID, class: CkoPrivateKey,
			attributes: privateAttrs, signKind: keyType},
		{handle: publicHandle, itemID: item.ID, class: CkoPublicKey,
			attributes: publicAttrs},
	}
}

// certObjects baut das Objekt eines Zertifikats. Ein Eintrag, der kein
// lesbares Zertifikat enthaelt, wird uebergangen - lieber kein Objekt als
// eines, dessen Merkmale geraten sind.
func certObjects(item Item, secret []byte, handle *uint64) []pkcs11Object {
	der := secret
	if block, _ := pem.Decode(secret); block != nil {
		if block.Type != "CERTIFICATE" {
			return nil
		}
		der = block.Bytes
	}
	certificate, err := x509.ParseCertificate(der)
	if err != nil {
		return nil
	}
	own := *handle
	*handle++
	return []pkcs11Object{{
		handle: own, itemID: item.ID, class: CkoCertificate,
		attributes: map[uint64]Pkcs11Value{
			CkaClass:           pkcs11Ulong(CkoCertificate),
			CkaCertificateType: pkcs11Ulong(CkcX509),
			CkaToken:           pkcs11Bool(true),
			CkaPrivate:         pkcs11Bool(false),
			CkaLabel:           pkcs11Text(item.Name),
			CkaID:              pkcs11Bytes(itemIDBytes(item.ID)),
			CkaValue:           pkcs11Bytes(append([]byte(nil), certificate.Raw...)),
			CkaSubject:         pkcs11Bytes(append([]byte(nil), certificate.RawSubject...)),
			CkaIssuer:          pkcs11Bytes(append([]byte(nil), certificate.RawIssuer...)),
			CkaSerialNumber:    pkcs11Bytes(serialDER(certificate.SerialNumber)),
			CkaModifiable:      pkcs11Bool(false),
			CkaDestroyable:     pkcs11Bool(false),
		},
	}}
}

// itemIDBytes macht aus der Nummer eines Eintrags die Kennung CKA_ID. Vier
// Bytes, grosses Ende voran: so steht an Schluessel und Zertifikat eines
// Eintrags dieselbe Kennung, und daran findet Firefox das Paar.
func itemIDBytes(id uint32) []byte {
	return []byte{byte(id >> 24), byte(id >> 16), byte(id >> 8), byte(id)}
}

// serialDER verpackt die Seriennummer als ganze Zahl in DER - so verlangt
// PKCS#11 CKA_SERIAL_NUMBER.
func serialDER(serial *big.Int) []byte {
	data, err := asn1.Marshal(serial)
	if err != nil {
		return nil
	}
	return data
}

// curveParamsDER ist die Kennung der Kurve als DER - CKA_EC_PARAMS.
func curveParamsDER(curve elliptic.Curve) ([]byte, error) {
	var oid asn1.ObjectIdentifier
	switch curve {
	case elliptic.P256():
		oid = asn1.ObjectIdentifier{1, 2, 840, 10045, 3, 1, 7}
	case elliptic.P384():
		oid = asn1.ObjectIdentifier{1, 3, 132, 0, 34}
	case elliptic.P521():
		oid = asn1.ObjectIdentifier{1, 3, 132, 0, 35}
	default:
		return nil, pkcs11Fail(CkrKeyTypeInconsistent, "error.pkcs11.curve")
	}
	return asn1.Marshal(oid)
}

// curveParamsEd25519 ist die Kennung von Ed25519 als DER. PKCS#11 v3.0
// nennt dafuer die Kennung 1.3.101.112 aus RFC 8410.
func curveParamsEd25519() []byte {
	data, err := asn1.Marshal(asn1.ObjectIdentifier{1, 3, 101, 112})
	if err != nil {
		return nil
	}
	return data
}

// ecPointDER verpackt den Punkt als Oktettfolge in DER - so verlangt
// PKCS#11 CKA_EC_POINT. Der nackte Punkt waere das haeufigste Missverstaendnis
// an dieser Stelle.
func ecPointDER(point []byte) ([]byte, error) {
	data, err := asn1.Marshal(point)
	if err != nil {
		return nil, pkcs11Fail(CkrFunctionFailed, "error.internal")
	}
	return data, nil
}

// FindObjects sucht Objekte nach einer Vorlage. Eine leere Vorlage trifft
// alle - so will es PKCS#11.
func (t *Pkcs11Token) FindObjects(handle uint64, template []Pkcs11Attribute) ([]uint64, error) {
	t.mu.Lock()
	defer t.mu.Unlock()
	session, found := t.sessions[handle]
	if !found {
		return nil, pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	if session.searching {
		return nil, pkcs11Fail(CkrOperationActive, "error.pkcs11.active")
	}
	if err := t.loadObjects(); err != nil {
		return nil, err
	}
	var hits []uint64
	for _, object := range t.objects {
		if matchesTemplate(object, template) {
			hits = append(hits, object.handle)
		}
	}
	sort.Slice(hits, func(a, b int) bool { return hits[a] < hits[b] })
	session.search = hits
	session.searching = true
	return hits, nil
}

// TakeFound gibt die naechsten Treffer heraus - C_FindObjects darf in
// Haeppchen abgeholt werden.
func (t *Pkcs11Token) TakeFound(handle uint64, count int) ([]uint64, error) {
	t.mu.Lock()
	defer t.mu.Unlock()
	session, found := t.sessions[handle]
	if !found {
		return nil, pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	if !session.searching {
		return nil, pkcs11Fail(CkrOperationNotInitialized, "error.pkcs11.notinit")
	}
	if count <= 0 || len(session.search) == 0 {
		return nil, nil
	}
	if count > len(session.search) {
		count = len(session.search)
	}
	out := session.search[:count]
	session.search = session.search[count:]
	return out, nil
}

// FindObjectsFinal beendet die Suche.
func (t *Pkcs11Token) FindObjectsFinal(handle uint64) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	session, found := t.sessions[handle]
	if !found {
		return pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	if !session.searching {
		return pkcs11Fail(CkrOperationNotInitialized, "error.pkcs11.notinit")
	}
	session.search = nil
	session.searching = false
	return nil
}

// matchesTemplate sagt, ob ein Objekt zu einer Vorlage passt. Ein Merkmal,
// das das Objekt nicht hat, ist ein Nichttreffer - nie ein Treffer.
func matchesTemplate(object pkcs11Object, template []Pkcs11Attribute) bool {
	for _, wanted := range template {
		have, found := object.attributes[wanted.Type]
		if !found || !sameValue(have, wanted.Value) {
			return false
		}
	}
	return true
}

// sameValue vergleicht zwei Merkmalswerte. Verschiedene Arten sind nie
// gleich - eine Null als Zahl ist nicht dasselbe wie ein leeres Feld.
func sameValue(have, wanted Pkcs11Value) bool {
	if have.Kind != wanted.Kind {
		return false
	}
	switch have.Kind {
	case Pkcs11Bool:
		return have.Bool == wanted.Bool
	case Pkcs11Ulong:
		return have.Ulong == wanted.Ulong
	}
	return constantEqual(have.Bytes, wanted.Bytes)
}

// Attribute gibt einen Merkmalswert. Ein Merkmal, das es nicht gibt, wird
// als solches gemeldet; ein Merkmal, das ein Geheimnis waere, als
// empfindlich. Beides sind eigene Rueckgabewerte in PKCS#11, und die fremde
// Software unterscheidet sie.
func (t *Pkcs11Token) Attribute(object, kind uint64) (Pkcs11Value, error) {
	t.mu.Lock()
	defer t.mu.Unlock()
	for _, candidate := range t.objects {
		if candidate.handle != object {
			continue
		}
		if candidate.class == CkoPrivateKey && isSecretAttribute(kind) {
			return Pkcs11Value{}, pkcs11Fail(CkrAttributeSensitive,
				"error.pkcs11.sensitive")
		}
		value, found := candidate.attributes[kind]
		if !found {
			return Pkcs11Value{}, pkcs11Fail(CkrAttributeTypeInvalid,
				"error.pkcs11.attribute")
		}
		return value, nil
	}
	return Pkcs11Value{}, pkcs11Fail(CkrObjectHandleInvalid, "error.pkcs11.object")
}

// isSecretAttribute nennt die Merkmale, die den privaten Teil preisgeben
// wuerden. Sie werden nicht weggelassen, sondern als empfindlich gemeldet -
// weglassen wuerde heissen "gibt es nicht", und das waere eine Luege.
func isSecretAttribute(kind uint64) bool {
	switch kind {
	case CkaValue, 0x00000123, 0x00000124, 0x00000125, 0x00000126, 0x00000127,
		0x00000128: // CKA_PRIVATE_EXPONENT bis CKA_COEFFICIENT
		return true
	}
	return false
}

// --- Unterschreiben ------------------------------------------------------------

// SignInit merkt sich, womit unterschrieben werden soll.
func (t *Pkcs11Token) SignInit(handle, object, mechanism uint64) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	session, found := t.sessions[handle]
	if !found {
		return pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	if session.signing {
		return pkcs11Fail(CkrOperationActive, "error.pkcs11.active")
	}
	if !t.loggedIn {
		return pkcs11Fail(CkrUserNotLoggedIn, "error.device.locked")
	}
	var target *pkcs11Object
	for index := range t.objects {
		if t.objects[index].handle == object {
			target = &t.objects[index]
			break
		}
	}
	if target == nil {
		return pkcs11Fail(CkrKeyHandleInvalid, "error.pkcs11.object")
	}
	if target.class != CkoPrivateKey {
		return pkcs11Fail(CkrKeyFunctionNotPermitted, "error.pkcs11.notprivate")
	}
	if !mechanismFitsKey(mechanism, target.signKind) {
		return pkcs11Fail(CkrKeyTypeInconsistent, "error.pkcs11.mechanism")
	}
	session.signObject = object
	session.signMechanism = mechanism
	session.signing = true
	Wipe(session.signBuffer)
	session.signBuffer = nil
	return nil
}

// mechanismFitsKey prueft, dass Verfahren und Schluesselart zusammenpassen.
// Ohne diese Pruefung wuerde eine RSA-Anfrage an einem Ed25519-Schluessel
// erst tief unten scheitern, und die Meldung sagte nichts.
func mechanismFitsKey(mechanism, keyType uint64) bool {
	switch mechanism {
	case CkmRSAPKCS, CkmSHA1RSAPKCS, CkmSHA256RSAPKCS, CkmSHA384RSAPKCS, CkmSHA512RSAPKCS:
		return keyType == CkkRSA
	case CkmECDSA, CkmECDSASHA256, CkmECDSASHA384, CkmECDSASHA512:
		return keyType == CkkEC
	case CkmEDDSA:
		return keyType == CkkECEdwards
	}
	return false
}

// SignUpdate sammelt Daten. Unterschrieben wird erst am Ende: das Geraet
// bekommt eine Anfrage, nicht viele.
func (t *Pkcs11Token) SignUpdate(handle uint64, part []byte) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	session, found := t.sessions[handle]
	if !found {
		return pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	if !session.signing {
		return pkcs11Fail(CkrOperationNotInitialized, "error.pkcs11.notinit")
	}
	if len(session.signBuffer)+len(part) > Pkcs11MaxSignData {
		session.signing = false
		Wipe(session.signBuffer)
		session.signBuffer = nil
		return pkcs11Fail(CkrDataLenRange, "error.pkcs11.toolong")
	}
	session.signBuffer = append(session.signBuffer, part...)
	return nil
}

// Pkcs11MaxSignData ist die groesste Menge, die in einem Zug unterschrieben
// wird. Ohne Grenze koennte ein fremdes Programm den Speicher fuellen.
const Pkcs11MaxSignData = 8 * 1024 * 1024

// SignFinal unterschreibt das Gesammelte.
func (t *Pkcs11Token) SignFinal(handle uint64) ([]byte, error) {
	t.mu.Lock()
	session, found := t.sessions[handle]
	if !found {
		t.mu.Unlock()
		return nil, pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	if !session.signing {
		t.mu.Unlock()
		return nil, pkcs11Fail(CkrOperationNotInitialized, "error.pkcs11.notinit")
	}
	data := session.signBuffer
	session.signBuffer = nil
	session.signing = false
	t.mu.Unlock()
	defer Wipe(data)
	return t.signWith(handle, session.signObject, session.signMechanism, data)
}

// Sign unterschreibt in einem Zug - der uebliche Weg.
func (t *Pkcs11Token) Sign(handle uint64, data []byte) ([]byte, error) {
	t.mu.Lock()
	session, found := t.sessions[handle]
	if !found {
		t.mu.Unlock()
		return nil, pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	if !session.signing {
		t.mu.Unlock()
		return nil, pkcs11Fail(CkrOperationNotInitialized, "error.pkcs11.notinit")
	}
	if len(data) > Pkcs11MaxSignData {
		session.signing = false
		t.mu.Unlock()
		return nil, pkcs11Fail(CkrDataLenRange, "error.pkcs11.toolong")
	}
	object, mechanism := session.signObject, session.signMechanism
	// PKCS#11 beendet den Vorgang mit C_Sign, auch wenn er scheitert.
	session.signing = false
	Wipe(session.signBuffer)
	session.signBuffer = nil
	t.mu.Unlock()
	return t.signWith(handle, object, mechanism, data)
}

// signWith holt das Geheimnis, unterschreibt und ueberschreibt es wieder.
// Genau wie beim ssh-Agenten: kein privater Schluessel bleibt liegen.
func (t *Pkcs11Token) signWith(session, object, mechanism uint64, data []byte) ([]byte, error) {
	t.mu.Lock()
	var itemID uint32
	var signKind uint64
	ok := false
	for _, candidate := range t.objects {
		if candidate.handle == object {
			itemID, signKind, ok = candidate.itemID, candidate.signKind, true
			break
		}
	}
	access, err := t.access()
	t.mu.Unlock()
	if !ok {
		return nil, pkcs11Fail(CkrKeyHandleInvalid, "error.pkcs11.object")
	}
	if err != nil {
		return nil, err
	}
	secret, err := access.Reveal(itemID)
	if err != nil {
		return nil, err
	}
	key, err := ParseSSHKey(secret)
	Wipe(secret)
	if err != nil {
		return nil, err
	}
	defer key.Wipe()
	return pkcs11SignWithKey(key, mechanism, signKind, data)
}

// pkcs11SignWithKey rechnet die Unterschrift im Format von PKCS#11.
//
// Der Unterschied zu SSH steckt hier: PKCS#11 gibt bei ECDSA die zwei Zahlen
// NACKT hintereinander, jede auf die Laenge der Kurve aufgefuellt - nicht in
// DER und nicht als mpint. Wer das verwechselt, bekommt eine Unterschrift,
// die nirgends haelt.
func pkcs11SignWithKey(key *SSHKey, mechanism, keyType uint64, data []byte) ([]byte, error) {
	switch keyType {
	case CkkECEdwards:
		if key.ed25519Key == nil {
			return nil, pkcs11Fail(CkrKeyTypeInconsistent, "error.pkcs11.mechanism")
		}
		return ed25519.Sign(key.ed25519Key, data), nil

	case CkkEC:
		if key.ecdsaKey == nil {
			return nil, pkcs11Fail(CkrKeyTypeInconsistent, "error.pkcs11.mechanism")
		}
		digest, err := pkcs11Digest(mechanism, data, key.ecdsaKey.Curve)
		if err != nil {
			return nil, err
		}
		r, s, signErr := ecdsa.Sign(rand.Reader, key.ecdsaKey, digest)
		if signErr != nil {
			return nil, pkcs11Fail(CkrFunctionFailed, "error.agent.sign")
		}
		size := (key.ecdsaKey.Curve.Params().BitSize + 7) / 8
		return ecdsaRawSignature(r, s, size)

	case CkkRSA:
		if key.rsaKey == nil {
			return nil, pkcs11Fail(CkrKeyTypeInconsistent, "error.pkcs11.mechanism")
		}
		hash, digest, err := rsaDigest(mechanism, data)
		if err != nil {
			return nil, err
		}
		signature, signErr := rsa.SignPKCS1v15(rand.Reader, key.rsaKey, hash, digest)
		if signErr != nil {
			return nil, pkcs11Fail(CkrFunctionFailed, "error.agent.sign")
		}
		return signature, nil
	}
	return nil, pkcs11Fail(CkrMechanismInvalid, "error.pkcs11.mechanism")
}

// ecdsaRawSignature baut die Unterschrift, die PKCS#11 bei ECDSA erwartet:
// die zwei Zahlen NACKT hintereinander, jede auf die Laenge der Kurve
// AUFGEFUELLT - kein DER, kein mpint, keine fuehrende Null weggelassen.
//
// Die Auffuellung ist der Punkt, und sie ist eine eigene Funktion, weil man
// sie sonst nicht pruefen kann: r und s kommen aus dem Zufall und sind fast
// immer schon lang genug. Wer hier nur kopiert statt auffuellt, baut einen
// Fehler, der in 255 von 256 Faellen nicht auffaellt - und beim 256. eine
// Unterschrift liefert, die nirgends haelt. Gemessen am 24.10.2026: genau
// dieser Mutant hat einen Prueflauf ueberlebt, weil er fast immer dasselbe
// tut.
func ecdsaRawSignature(r, s *big.Int, size int) ([]byte, error) {
	if size <= 0 || r == nil || s == nil || r.Sign() < 0 || s.Sign() < 0 ||
		len(r.Bytes()) > size || len(s.Bytes()) > size {
		return nil, pkcs11Fail(CkrFunctionFailed, "error.agent.sign")
	}
	out := make([]byte, 2*size)
	r.FillBytes(out[:size])
	s.FillBytes(out[size:])
	return out, nil
}

// pkcs11Digest streut fuer ECDSA. Bei CKM_ECDSA bringt der Aufrufer den
// Streuwert schon mit; dann wird er nur auf die Laenge der Kurve gekuerzt,
// wie es der Standard verlangt.
func pkcs11Digest(mechanism uint64, data []byte, curve elliptic.Curve) ([]byte, error) {
	switch mechanism {
	case CkmECDSA:
		size := (curve.Params().BitSize + 7) / 8
		if len(data) == 0 {
			return nil, pkcs11Fail(CkrDataLenRange, "error.pkcs11.toolong")
		}
		if len(data) > size {
			return data[:size], nil
		}
		return data, nil
	case CkmECDSASHA256:
		sum := sha256.Sum256(data)
		return sum[:], nil
	case CkmECDSASHA384:
		sum := sha512.Sum384(data)
		return sum[:], nil
	case CkmECDSASHA512:
		sum := sha512.Sum512(data)
		return sum[:], nil
	}
	return nil, pkcs11Fail(CkrMechanismInvalid, "error.pkcs11.mechanism")
}

// rsaDigest nennt Streuwertfunktion und Streuwert. Bei CKM_RSA_PKCS bringt
// der Aufrufer die fertige DigestInfo mit - dann wird nichts gestreut, und
// crypto.Hash(0) sagt der Standardbibliothek genau das.
func rsaDigest(mechanism uint64, data []byte) (crypto.Hash, []byte, error) {
	switch mechanism {
	case CkmRSAPKCS:
		return crypto.Hash(0), data, nil
	case CkmSHA1RSAPKCS:
		sum := sha1.Sum(data)
		return crypto.SHA1, sum[:], nil
	case CkmSHA256RSAPKCS:
		sum := sha256.Sum256(data)
		return crypto.SHA256, sum[:], nil
	case CkmSHA384RSAPKCS:
		sum := sha512.Sum384(data)
		return crypto.SHA384, sum[:], nil
	case CkmSHA512RSAPKCS:
		sum := sha512.Sum512(data)
		return crypto.SHA512, sum[:], nil
	}
	return 0, nil, pkcs11Fail(CkrMechanismInvalid, "error.pkcs11.mechanism")
}

// Pkcs11SignatureSize nennt die Laenge, die eine Unterschrift haben wird.
// PKCS#11 fragt danach, bevor es einen Puffer bereitstellt.
func (t *Pkcs11Token) Pkcs11SignatureSize(handle uint64) (int, error) {
	t.mu.Lock()
	defer t.mu.Unlock()
	session, found := t.sessions[handle]
	if !found {
		return 0, pkcs11Fail(CkrSessionHandleInvalid, "error.pkcs11.session")
	}
	if !session.signing {
		return 0, pkcs11Fail(CkrOperationNotInitialized, "error.pkcs11.notinit")
	}
	for _, candidate := range t.objects {
		if candidate.handle != session.signObject {
			continue
		}
		switch candidate.signKind {
		case CkkECEdwards:
			return ed25519.SignatureSize, nil
		case CkkEC:
			point, found := candidate.attributes[CkaECParams]
			if !found {
				return 0, pkcs11Fail(CkrFunctionFailed, "error.internal")
			}
			return 2 * curveSizeFromParams(point.Bytes), nil
		case CkkRSA:
			bits, found := candidate.attributes[CkaModulusBits]
			if !found {
				return 0, pkcs11Fail(CkrFunctionFailed, "error.internal")
			}
			return int((bits.Ulong + 7) / 8), nil
		}
	}
	return 0, pkcs11Fail(CkrKeyHandleInvalid, "error.pkcs11.object")
}

// curveSizeFromParams liest die Groesse der Kurve aus ihrer Kennung.
func curveSizeFromParams(params []byte) int {
	var oid asn1.ObjectIdentifier
	if _, err := asn1.Unmarshal(params, &oid); err != nil {
		return 0
	}
	switch {
	case oid.Equal(asn1.ObjectIdentifier{1, 2, 840, 10045, 3, 1, 7}):
		return 32
	case oid.Equal(asn1.ObjectIdentifier{1, 3, 132, 0, 34}):
		return 48
	case oid.Equal(asn1.ObjectIdentifier{1, 3, 132, 0, 35}):
		return 66
	}
	return 0
}
