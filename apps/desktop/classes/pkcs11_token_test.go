// Pruefungen fuer das Token hinter PKCS#11.
//
// Geprueft wird gegen die Attrappe und gegen die Standardbibliothek: jede
// Unterschrift wird mit crypto/* nachgerechnet, also mit Code, der unseren
// nicht kennt. Zwei Dinge sind dabei die eigentlichen Zusicherungen:
//
//   - Der private Teil kommt NIE heraus. CKA_VALUE an einem privaten
//     Schluessel muss als empfindlich abgewiesen werden, nicht als
//     "gibt es nicht" - der Unterschied ist in PKCS#11 ein eigener
//     Rueckgabewert, und fremde Software unterscheidet ihn.
//   - Das Format der Unterschrift ist das von PKCS#11, nicht das von SSH.
//     Bei ECDSA sind das die zwei Zahlen NACKT hintereinander, jede auf die
//     Laenge der Kurve aufgefuellt. Wer hier DER oder mpint liefert, gibt
//     eine Unterschrift heraus, die nirgends haelt.
package classes

import (
	"crypto"
	"crypto/ecdsa"
	"crypto/ed25519"
	"crypto/rsa"
	"crypto/sha256"
	"encoding/asn1"
	"math/big"
	"os"
	"testing"
)

// tokenWithKeys baut ein Token mit den drei Schluesselarten im Tresor.
func tokenWithKeys(t *testing.T) (*Pkcs11Token, *App, map[string]uint32) {
	t.Helper()
	app := unlockedApp(t)
	ids := map[string]uint32{
		"ed25519": putTestKey(t, app, "Pruefung Ed25519", goldenEd25519Private),
		"ecdsa":   putTestKey(t, app, "Pruefung ECDSA", goldenEcdsaP256Private),
		"rsa":     putTestKey(t, app, "Pruefung RSA", goldenRsa2048Private),
	}
	token := NewPkcs11Token(app)
	token.SetHost("pruefung")
	return token, app, ids
}

// openLoggedIn oeffnet eine angemeldete Sitzung.
func openLoggedIn(t *testing.T, token *Pkcs11Token) uint64 {
	t.Helper()
	session, err := token.OpenSession(CkfSerialSession)
	if err != nil {
		t.Fatalf("Sitzung oeffnen: %v", err)
	}
	// Das Geraet ist durch unlockedApp schon offen; PKCS#11 verlangt
	// trotzdem die Anmeldung, und sie muss gelingen.
	if err := token.Login(session, CkuUser, []byte(testPin)); err != nil {
		t.Fatalf("anmelden: %v", err)
	}
	return session
}

// Je Schluessel ein privates und ein oeffentliches Objekt, je Zertifikat
// eines - und die Kennung verbindet die Objekte eines Eintrags.
func TestPkcs11ObjectsFromVault(t *testing.T) {
	token, _, ids := tokenWithKeys(t)
	session := openLoggedIn(t, token)

	all, err := token.FindObjects(session, nil)
	if err != nil {
		t.Fatalf("suchen: %v", err)
	}
	// Drei Schluessel, je zwei Objekte. Das Zertifikat der Attrappe ist
	// kein lesbares Zertifikat und wird darum uebergangen - genau so soll
	// es sein, lieber kein Objekt als eines mit geratenen Merkmalen.
	if len(all) != 6 {
		t.Fatalf("%d Objekte, erwartet 6", len(all))
	}
	if err := token.FindObjectsFinal(session); err != nil {
		t.Fatalf("Suche beenden: %v", err)
	}

	for name, id := range ids {
		want := itemIDBytes(id)
		found, findErr := token.FindObjects(session, []Pkcs11Attribute{
			{Type: CkaID, Value: pkcs11Bytes(want)},
		})
		if findErr != nil {
			t.Fatalf("%s suchen: %v", name, findErr)
		}
		if len(found) != 2 {
			t.Errorf("%s: %d Objekte zur Kennung, erwartet 2 (privat und oeffentlich)",
				name, len(found))
		}
		token.FindObjectsFinal(session)
	}
}

// Der private Teil darf nie herauskommen - und die Merkmale muessen das
// vorher sagen.
func TestPkcs11PrivateKeyStaysInside(t *testing.T) {
	token, _, _ := tokenWithKeys(t)
	session := openLoggedIn(t, token)

	privates, err := token.FindObjects(session, []Pkcs11Attribute{
		{Type: CkaClass, Value: pkcs11Ulong(CkoPrivateKey)},
	})
	if err != nil {
		t.Fatalf("suchen: %v", err)
	}
	if len(privates) != 3 {
		t.Fatalf("%d private Schluessel, erwartet 3", len(privates))
	}
	token.FindObjectsFinal(session)

	for _, object := range privates {
		// Die Merkmale muessen sagen, dass hier nichts herauskommt.
		for kind, want := range map[uint64]bool{
			CkaSensitive:          true,
			CkaExtractable:        false,
			CkaAlwaysSensitive:    true,
			CkaNeverExtractable:   true,
			CkaSign:               true,
			CkaDecrypt:            false,
			CkaDerive:             false,
			CkaAlwaysAuthenticate: true,
		} {
			value, attrErr := token.Attribute(object, kind)
			if attrErr != nil {
				t.Errorf("Objekt %d, Merkmal 0x%x: %v", object, kind, attrErr)
				continue
			}
			if value.Kind != Pkcs11Bool || value.Bool != want {
				t.Errorf("Objekt %d, Merkmal 0x%x: %v, erwartet %v",
					object, kind, value, want)
			}
		}
		// Und der Wert selbst muss als EMPFINDLICH abgewiesen werden, nicht
		// als unbekannt. PKCS#11 hat dafuer einen eigenen Rueckgabewert.
		_, valueErr := token.Attribute(object, CkaValue)
		coded, ok := valueErr.(*Pkcs11Error)
		if !ok || coded.RV != CkrAttributeSensitive {
			t.Errorf("Objekt %d: CKA_VALUE gab %v, erwartet CKR_ATTRIBUTE_SENSITIVE",
				object, valueErr)
		}
	}
}

// Der oeffentliche Teil muss herauskommen - sonst kann Firefox nichts
// anfangen -, und zwar in der Verpackung, die PKCS#11 verlangt.
func TestPkcs11PublicPartsHaveTheRightShape(t *testing.T) {
	token, _, ids := tokenWithKeys(t)
	session := openLoggedIn(t, token)

	// RSA: Modulus und Exponent als nackte Zahlen.
	rsaObjects, err := token.FindObjects(session, []Pkcs11Attribute{
		{Type: CkaID, Value: pkcs11Bytes(itemIDBytes(ids["rsa"]))},
		{Type: CkaClass, Value: pkcs11Ulong(CkoPublicKey)},
	})
	if err != nil || len(rsaObjects) != 1 {
		t.Fatalf("RSA suchen: %v, %d Treffer", err, len(rsaObjects))
	}
	token.FindObjectsFinal(session)

	reference, err := ParseSSHKey([]byte(goldenRsa2048Private))
	if err != nil {
		t.Fatalf("Vergleichsschluessel: %v", err)
	}
	defer reference.Wipe()

	modulus, err := token.Attribute(rsaObjects[0], CkaModulus)
	if err != nil {
		t.Fatalf("Modulus: %v", err)
	}
	if new(big.Int).SetBytes(modulus.Bytes).Cmp(reference.rsaKey.N) != 0 {
		t.Error("der genannte Modulus ist nicht der des Schluessels")
	}
	bits, err := token.Attribute(rsaObjects[0], CkaModulusBits)
	if err != nil || bits.Ulong != uint64(reference.rsaKey.N.BitLen()) {
		t.Errorf("CKA_MODULUS_BITS: %v (%v)", bits, err)
	}

	// ECDSA: die Kurve als Kennung in DER, der Punkt als Oktettfolge in DER.
	ecObjects, err := token.FindObjects(session, []Pkcs11Attribute{
		{Type: CkaID, Value: pkcs11Bytes(itemIDBytes(ids["ecdsa"]))},
		{Type: CkaClass, Value: pkcs11Ulong(CkoPublicKey)},
	})
	if err != nil || len(ecObjects) != 1 {
		t.Fatalf("ECDSA suchen: %v, %d Treffer", err, len(ecObjects))
	}
	token.FindObjectsFinal(session)

	params, err := token.Attribute(ecObjects[0], CkaECParams)
	if err != nil {
		t.Fatalf("CKA_EC_PARAMS: %v", err)
	}
	var oid asn1.ObjectIdentifier
	if _, unmarshalErr := asn1.Unmarshal(params.Bytes, &oid); unmarshalErr != nil {
		t.Fatalf("CKA_EC_PARAMS ist kein DER: %v", unmarshalErr)
	}
	if !oid.Equal(asn1.ObjectIdentifier{1, 2, 840, 10045, 3, 1, 7}) {
		t.Errorf("Kurve %v, erwartet prime256v1", oid)
	}
	point, err := token.Attribute(ecObjects[0], CkaECPoint)
	if err != nil {
		t.Fatalf("CKA_EC_POINT: %v", err)
	}
	// Die Verpackung ist der haeufigste Fehler an dieser Stelle: der Punkt
	// gehoert in eine Oktettfolge, nicht nackt hinein.
	var raw []byte
	if _, unmarshalErr := asn1.Unmarshal(point.Bytes, &raw); unmarshalErr != nil {
		t.Fatalf("CKA_EC_POINT ist keine Oktettfolge in DER: %v", unmarshalErr)
	}
	if len(raw) != 65 || raw[0] != 4 {
		t.Errorf("Punkt hat %d Bytes und beginnt mit 0x%x", len(raw), raw[0])
	}
	reference2, err := ParseSSHKey([]byte(goldenEcdsaP256Private))
	if err != nil {
		t.Fatalf("Vergleichsschluessel: %v", err)
	}
	defer reference2.Wipe()
	expected, err := markPoint(reference2.ecdsaKey.Curve,
		reference2.ecdsaKey.X, reference2.ecdsaKey.Y)
	if err != nil {
		t.Fatalf("Punkt bauen: %v", err)
	}
	if !constantEqual(raw, expected) {
		t.Error("der genannte Punkt ist nicht der des Schluessels")
	}

	// Ed25519: die Kennung aus RFC 8410.
	edObjects, err := token.FindObjects(session, []Pkcs11Attribute{
		{Type: CkaID, Value: pkcs11Bytes(itemIDBytes(ids["ed25519"]))},
		{Type: CkaClass, Value: pkcs11Ulong(CkoPublicKey)},
	})
	if err != nil || len(edObjects) != 1 {
		t.Fatalf("Ed25519 suchen: %v, %d Treffer", err, len(edObjects))
	}
	token.FindObjectsFinal(session)
	edParams, err := token.Attribute(edObjects[0], CkaECParams)
	if err != nil {
		t.Fatalf("CKA_EC_PARAMS: %v", err)
	}
	if _, unmarshalErr := asn1.Unmarshal(edParams.Bytes, &oid); unmarshalErr != nil {
		t.Fatalf("CKA_EC_PARAMS ist kein DER: %v", unmarshalErr)
	}
	if !oid.Equal(asn1.ObjectIdentifier{1, 3, 101, 112}) {
		t.Errorf("Kennung %v, erwartet 1.3.101.112", oid)
	}
	edType, err := token.Attribute(edObjects[0], CkaKeyType)
	if err != nil || edType.Ulong != CkkECEdwards {
		t.Errorf("CKA_KEY_TYPE: %v (%v), erwartet CKK_EC_EDWARDS", edType, err)
	}
}

// Jede Unterschrift wird mit der Standardbibliothek nachgerechnet, und das
// FORMAT ist das von PKCS#11.
func TestPkcs11SignaturesVerify(t *testing.T) {
	token, _, ids := tokenWithKeys(t)
	session := openLoggedIn(t, token)
	data := []byte("was ein fremdes Programm unterschreiben laesst")

	cases := []struct {
		name      string
		key       string
		private   string
		mechanism uint64
	}{
		{"rsa-sha256", "rsa", goldenRsa2048Private, CkmSHA256RSAPKCS},
		{"rsa-roh", "rsa", goldenRsa2048Private, CkmRSAPKCS},
		{"ecdsa-sha256", "ecdsa", goldenEcdsaP256Private, CkmECDSASHA256},
		{"ecdsa-roh", "ecdsa", goldenEcdsaP256Private, CkmECDSA},
		{"ed25519", "ed25519", goldenEd25519Private, CkmEDDSA},
	}
	for _, one := range cases {
		t.Run(one.name, func(t *testing.T) {
			objects, err := token.FindObjects(session, []Pkcs11Attribute{
				{Type: CkaID, Value: pkcs11Bytes(itemIDBytes(ids[one.key]))},
				{Type: CkaClass, Value: pkcs11Ulong(CkoPrivateKey)},
			})
			if err != nil || len(objects) != 1 {
				t.Fatalf("suchen: %v, %d Treffer", err, len(objects))
			}
			token.FindObjectsFinal(session)

			if err := token.SignInit(session, objects[0], one.mechanism); err != nil {
				t.Fatalf("SignInit: %v", err)
			}
			// Die angekuendigte Laenge muss stimmen - jeder Aufrufer fragt
			// sie zuerst ab und stellt genau so viel Platz bereit.
			announced, err := token.Pkcs11SignatureSize(session)
			if err != nil {
				t.Fatalf("Laenge: %v", err)
			}

			// Bei den rohen Verfahren bringt der Aufrufer den Streuwert
			// bzw. die fertige DigestInfo mit.
			input := data
			digest := sha256.Sum256(data)
			switch one.mechanism {
			case CkmECDSA:
				input = digest[:]
			case CkmRSAPKCS:
				info, infoErr := digestInfoSHA256(digest[:])
				if infoErr != nil {
					t.Fatalf("DigestInfo: %v", infoErr)
				}
				input = info
			}

			signature, err := token.Sign(session, input)
			if err != nil {
				t.Fatalf("Sign: %v", err)
			}
			if len(signature) != announced {
				t.Errorf("Unterschrift ist %d Bytes, angekuendigt waren %d",
					len(signature), announced)
			}

			reference, err := ParseSSHKey([]byte(one.private))
			if err != nil {
				t.Fatalf("Vergleichsschluessel: %v", err)
			}
			defer reference.Wipe()

			switch one.key {
			case "rsa":
				if err := rsa.VerifyPKCS1v15(&reference.rsaKey.PublicKey,
					crypto.SHA256, digest[:], signature); err != nil {
					t.Errorf("die Unterschrift haelt nicht: %v", err)
				}
			case "ecdsa":
				// Das Format: zwei nackte Zahlen, je 32 Bytes. Keine
				// Verpackung, kein DER.
				if len(signature) != 64 {
					t.Fatalf("ECDSA-Unterschrift hat %d Bytes, erwartet 64",
						len(signature))
				}
				r := new(big.Int).SetBytes(signature[:32])
				s := new(big.Int).SetBytes(signature[32:])
				if !ecdsa.Verify(&reference.ecdsaKey.PublicKey, digest[:], r, s) {
					t.Error("die Unterschrift haelt nicht")
				}
			case "ed25519":
				public := reference.ed25519Key.Public().(ed25519.PublicKey)
				if !ed25519.Verify(public, data, signature) {
					t.Error("die Unterschrift haelt nicht")
				}
			}
		})
	}
}

// Die Auffuellung der zwei Zahlen bei ECDSA - geprueft mit KURZEN Zahlen.
//
// Mit Zahlen aus dem Zufall ist das nicht pruefbar: sie sind fast immer
// schon lang genug, und ein Code, der nur kopiert statt auffuellt, sieht
// dann richtig aus. Gemessen am 24.10.2026: der Mutant "Zahlen nicht
// aufgefuellt" ueberlebte den Prueflauf ueber echte Unterschriften. Darum
// wird die Formatierung hier einzeln und mit festen Zahlen geprueft.
func TestEcdsaRawSignaturePads(t *testing.T) {
	cases := []struct {
		name string
		r, s *big.Int
		size int
		want []byte
	}{
		{"beide kurz", big.NewInt(1), big.NewInt(2), 4,
			[]byte{0, 0, 0, 1, 0, 0, 0, 2}},
		{"r kurz, s voll", big.NewInt(0x7f), big.NewInt(0x01020304), 4,
			[]byte{0, 0, 0, 0x7f, 0x01, 0x02, 0x03, 0x04}},
		{"beide null", big.NewInt(0), big.NewInt(0), 2,
			[]byte{0, 0, 0, 0}},
		{"beide voll", big.NewInt(0xaabb), big.NewInt(0xccdd), 2,
			[]byte{0xaa, 0xbb, 0xcc, 0xdd}},
	}
	for _, one := range cases {
		got, err := ecdsaRawSignature(one.r, one.s, one.size)
		if err != nil {
			t.Errorf("%s: %v", one.name, err)
			continue
		}
		if len(got) != 2*one.size {
			t.Errorf("%s: %d Bytes, erwartet %d", one.name, len(got), 2*one.size)
			continue
		}
		if !constantEqual(got, one.want) {
			t.Errorf("%s: %x, erwartet %x", one.name, got, one.want)
		}
	}
	// Eine Zahl, die nicht in die Kurve passt, ist ein Fehler und kein
	// stilles Abschneiden.
	if _, err := ecdsaRawSignature(big.NewInt(0x010203), big.NewInt(1), 2); err == nil {
		t.Error("eine zu grosse Zahl wurde angenommen")
	}
	if _, err := ecdsaRawSignature(big.NewInt(1), big.NewInt(1), 0); err == nil {
		t.Error("Laenge null wurde angenommen")
	}
}

// Verschiedene Arten von Merkmalswerten sind NIE gleich.
//
// Das ist die Zusicherung von sameValue, und sie wird hier einzeln geprueft:
// ueber FindObjects ist sie nicht messbar, weil kein Objekt im Tresor ein
// leeres Byte-Merkmal hat, an dem der Unterschied sichtbar wuerde. Gemessen
// am 24.10.2026 - der Mutant ohne diese Pruefung ueberlebte den Prueflauf
// ueber die Suchvorlagen.
func TestPkcs11SameValueKinds(t *testing.T) {
	gleich := []struct {
		name string
		a, b Pkcs11Value
	}{
		{"zwei Zahlen", pkcs11Ulong(3), pkcs11Ulong(3)},
		{"zwei Wahrheitswerte", pkcs11Bool(true), pkcs11Bool(true)},
		{"zwei Byte-Folgen", pkcs11Bytes([]byte{1, 2}), pkcs11Bytes([]byte{1, 2})},
		{"zwei leere Folgen", pkcs11Bytes(nil), pkcs11Bytes([]byte{})},
	}
	for _, one := range gleich {
		if !sameValue(one.a, one.b) {
			t.Errorf("%s: gelten als verschieden", one.name)
		}
	}
	verschieden := []struct {
		name string
		a, b Pkcs11Value
	}{
		{"Zahl gegen Wahrheitswert", pkcs11Ulong(0), pkcs11Bool(false)},
		{"Zahl gegen leere Folge", pkcs11Ulong(0), pkcs11Bytes(nil)},
		{"Wahrheitswert gegen leere Folge", pkcs11Bool(false), pkcs11Bytes(nil)},
		{"leere Folge gegen Wahrheitswert", pkcs11Bytes(nil), pkcs11Bool(false)},
		{"leere Folge gegen Zahl", pkcs11Bytes(nil), pkcs11Ulong(0)},
		{"Wahrheitswert gegen Zahl", pkcs11Bool(true), pkcs11Ulong(1)},
		{"verschiedene Zahlen", pkcs11Ulong(3), pkcs11Ulong(4)},
		{"verschiedene Folgen", pkcs11Bytes([]byte{1}), pkcs11Bytes([]byte{2})},
	}
	for _, one := range verschieden {
		if sameValue(one.a, one.b) {
			t.Errorf("%s: gelten als gleich", one.name)
		}
	}
}

// digestInfoSHA256 baut die DigestInfo, die CKM_RSA_PKCS erwartet.
func digestInfoSHA256(digest []byte) ([]byte, error) {
	type algorithm struct {
		Algorithm  asn1.ObjectIdentifier
		Parameters asn1.RawValue
	}
	type info struct {
		Algorithm algorithm
		Digest    []byte
	}
	return asn1.Marshal(info{
		Algorithm: algorithm{
			Algorithm:  asn1.ObjectIdentifier{2, 16, 840, 1, 101, 3, 4, 2, 1},
			Parameters: asn1.RawValue{Tag: 5}, // NULL
		},
		Digest: digest,
	})
}

// Ein Verfahren, das nicht zur Schluesselart passt, muss frueh abgewiesen
// werden - nicht erst tief unten mit einer Meldung, die nichts sagt.
func TestPkcs11MechanismMustFitKey(t *testing.T) {
	token, _, ids := tokenWithKeys(t)
	session := openLoggedIn(t, token)

	cases := []struct {
		key       string
		mechanism uint64
	}{
		{"ed25519", CkmSHA256RSAPKCS},
		{"ed25519", CkmECDSASHA256},
		{"rsa", CkmEDDSA},
		{"rsa", CkmECDSA},
		{"ecdsa", CkmRSAPKCS},
		{"ecdsa", CkmEDDSA},
	}
	for _, one := range cases {
		objects, err := token.FindObjects(session, []Pkcs11Attribute{
			{Type: CkaID, Value: pkcs11Bytes(itemIDBytes(ids[one.key]))},
			{Type: CkaClass, Value: pkcs11Ulong(CkoPrivateKey)},
		})
		if err != nil || len(objects) != 1 {
			t.Fatalf("suchen: %v", err)
		}
		token.FindObjectsFinal(session)
		err = token.SignInit(session, objects[0], one.mechanism)
		coded, ok := err.(*Pkcs11Error)
		if !ok || coded.RV != CkrKeyTypeInconsistent {
			t.Errorf("%s mit 0x%x: %v, erwartet CKR_KEY_TYPE_INCONSISTENT",
				one.key, one.mechanism, err)
		}
	}
}

// Mit einem oeffentlichen Objekt darf nicht unterschrieben werden.
func TestPkcs11PublicKeyCannotSign(t *testing.T) {
	token, _, ids := tokenWithKeys(t)
	session := openLoggedIn(t, token)
	objects, err := token.FindObjects(session, []Pkcs11Attribute{
		{Type: CkaID, Value: pkcs11Bytes(itemIDBytes(ids["rsa"]))},
		{Type: CkaClass, Value: pkcs11Ulong(CkoPublicKey)},
	})
	if err != nil || len(objects) != 1 {
		t.Fatalf("suchen: %v", err)
	}
	token.FindObjectsFinal(session)
	err = token.SignInit(session, objects[0], CkmSHA256RSAPKCS)
	coded, ok := err.(*Pkcs11Error)
	if !ok || coded.RV != CkrKeyFunctionNotPermitted {
		t.Errorf("%v, erwartet CKR_KEY_FUNCTION_NOT_PERMITTED", err)
	}
}

// Ohne Anmeldung nennt das Token keine Objekte und unterschreibt nicht.
func TestPkcs11NothingWithoutLogin(t *testing.T) {
	cfg := DefaultConfig()
	cfg.SimAddress = startSimulator(t)
	cfg.Lang = "de"
	app := NewApp(os.DirFS(".."), cfg)
	t.Cleanup(app.Disconnect)
	token := NewPkcs11Token(app)

	session, err := token.OpenSession(CkfSerialSession)
	if err != nil {
		t.Fatalf("Sitzung oeffnen: %v", err)
	}
	_, err = token.FindObjects(session, nil)
	coded, ok := err.(*Pkcs11Error)
	if !ok || coded.RV != CkrUserNotLoggedIn {
		t.Errorf("suchen ohne Anmeldung: %v, erwartet CKR_USER_NOT_LOGGED_IN", err)
	}
	err = token.SignInit(session, 1, CkmEDDSA)
	coded, ok = err.(*Pkcs11Error)
	if !ok || coded.RV != CkrUserNotLoggedIn {
		t.Errorf("SignInit ohne Anmeldung: %v, erwartet CKR_USER_NOT_LOGGED_IN", err)
	}
	// Eine falsche PIN darf nicht anmelden.
	if err := token.Login(session, CkuUser, []byte("000000")); err == nil {
		t.Error("die Anmeldung gelang mit falscher PIN")
	}
	if token.LoggedIn() {
		t.Error("das Token haelt sich nach fehlgeschlagener Anmeldung fuer offen")
	}
}

// Nach dem Abmelden ist wieder nichts zu sehen - und die Liste wird nicht
// aus dem Gedaechtnis weitergegeben.
func TestPkcs11LogoutForgets(t *testing.T) {
	token, _, _ := tokenWithKeys(t)
	session := openLoggedIn(t, token)
	first, err := token.FindObjects(session, nil)
	if err != nil || len(first) == 0 {
		t.Fatalf("suchen: %v, %d Treffer", err, len(first))
	}
	token.FindObjectsFinal(session)

	if err := token.Logout(session); err != nil {
		t.Fatalf("abmelden: %v", err)
	}
	_, err = token.FindObjects(session, nil)
	coded, ok := err.(*Pkcs11Error)
	if !ok || coded.RV != CkrUserNotLoggedIn {
		t.Errorf("nach dem Abmelden: %v, erwartet CKR_USER_NOT_LOGGED_IN", err)
	}
}

// Die Reihenfolge der Aufrufe ist Teil der Schnittstelle: ohne SignInit
// keine Unterschrift, ohne FindObjectsInit kein Abholen, und zwei Suchen
// gleichzeitig gibt es nicht.
func TestPkcs11OperationOrder(t *testing.T) {
	token, _, _ := tokenWithKeys(t)
	session := openLoggedIn(t, token)

	if _, err := token.TakeFound(session, 10); err == nil {
		t.Error("Abholen ohne Suche war erlaubt")
	}
	if err := token.FindObjectsFinal(session); err == nil {
		t.Error("Beenden ohne Suche war erlaubt")
	}
	if _, err := token.Sign(session, []byte("x")); err == nil {
		t.Error("Unterschreiben ohne SignInit war erlaubt")
	}
	if _, err := token.SignFinal(session); err == nil {
		t.Error("SignFinal ohne SignInit war erlaubt")
	}
	if _, err := token.FindObjects(session, nil); err != nil {
		t.Fatalf("suchen: %v", err)
	}
	if _, err := token.FindObjects(session, nil); err == nil {
		t.Error("zwei Suchen gleichzeitig waren erlaubt")
	}
	token.FindObjectsFinal(session)

	// Eine ungueltige Sitzungsnummer muss als solche gemeldet werden.
	if _, err := token.FindObjects(9999, nil); err != nil {
		coded, ok := err.(*Pkcs11Error)
		if !ok || coded.RV != CkrSessionHandleInvalid {
			t.Errorf("fremde Sitzung: %v, erwartet CKR_SESSION_HANDLE_INVALID", err)
		}
	} else {
		t.Error("eine fremde Sitzungsnummer wurde angenommen")
	}
}

// Stueckweise unterschreiben muss dasselbe ergeben wie in einem Zug.
func TestPkcs11SignInPieces(t *testing.T) {
	token, _, ids := tokenWithKeys(t)
	session := openLoggedIn(t, token)
	objects, err := token.FindObjects(session, []Pkcs11Attribute{
		{Type: CkaID, Value: pkcs11Bytes(itemIDBytes(ids["rsa"]))},
		{Type: CkaClass, Value: pkcs11Ulong(CkoPrivateKey)},
	})
	if err != nil || len(objects) != 1 {
		t.Fatalf("suchen: %v", err)
	}
	token.FindObjectsFinal(session)

	data := []byte("erst der eine Teil, dann der andere")
	if err := token.SignInit(session, objects[0], CkmSHA256RSAPKCS); err != nil {
		t.Fatalf("SignInit: %v", err)
	}
	if err := token.SignUpdate(session, data[:10]); err != nil {
		t.Fatalf("SignUpdate: %v", err)
	}
	if err := token.SignUpdate(session, data[10:]); err != nil {
		t.Fatalf("SignUpdate: %v", err)
	}
	signature, err := token.SignFinal(session)
	if err != nil {
		t.Fatalf("SignFinal: %v", err)
	}
	reference, err := ParseSSHKey([]byte(goldenRsa2048Private))
	if err != nil {
		t.Fatalf("Vergleichsschluessel: %v", err)
	}
	defer reference.Wipe()
	digest := sha256.Sum256(data)
	if err := rsa.VerifyPKCS1v15(&reference.rsaKey.PublicKey, crypto.SHA256,
		digest[:], signature); err != nil {
		t.Errorf("die stueckweise Unterschrift haelt nicht: %v", err)
	}
}

// Eine Suchvorlage trifft nur, was WIRKLICH passt. Ein Merkmal, das das
// Objekt nicht hat, ist ein Nichttreffer - nie ein Treffer.
func TestPkcs11TemplateMatching(t *testing.T) {
	token, _, ids := tokenWithKeys(t)
	session := openLoggedIn(t, token)

	cases := []struct {
		name     string
		template []Pkcs11Attribute
		want     int
	}{
		{"alles", nil, 6},
		{"nur Zertifikate", []Pkcs11Attribute{
			{Type: CkaClass, Value: pkcs11Ulong(CkoCertificate)}}, 0},
		{"RSA-Schluessel", []Pkcs11Attribute{
			{Type: CkaKeyType, Value: pkcs11Ulong(CkkRSA)}}, 2},
		{"Kennung die es nicht gibt", []Pkcs11Attribute{
			{Type: CkaID, Value: pkcs11Bytes([]byte{9, 9, 9, 9})}}, 0},
		{"Merkmal das keines hat", []Pkcs11Attribute{
			{Type: CkaIssuer, Value: pkcs11Bytes([]byte("x"))}}, 0},
		{"Zahl gegen Wahrheitswert", []Pkcs11Attribute{
			{Type: CkaToken, Value: pkcs11Ulong(1)}}, 0},
		{"privat und RSA", []Pkcs11Attribute{
			{Type: CkaClass, Value: pkcs11Ulong(CkoPrivateKey)},
			{Type: CkaKeyType, Value: pkcs11Ulong(CkkRSA)}}, 1},
		{"Name", []Pkcs11Attribute{
			{Type: CkaLabel, Value: pkcs11Text("Pruefung RSA")}}, 2},
	}
	for _, one := range cases {
		found, err := token.FindObjects(session, one.template)
		if err != nil {
			t.Fatalf("%s: %v", one.name, err)
		}
		if len(found) != one.want {
			t.Errorf("%s: %d Treffer, erwartet %d", one.name, len(found), one.want)
		}
		token.FindObjectsFinal(session)
	}
	_ = ids
}

// Treffer duerfen in Haeppchen abgeholt werden, und keiner darf doppelt
// oder gar nicht kommen.
func TestPkcs11TakeFoundInPieces(t *testing.T) {
	token, _, _ := tokenWithKeys(t)
	session := openLoggedIn(t, token)
	all, err := token.FindObjects(session, nil)
	if err != nil {
		t.Fatalf("suchen: %v", err)
	}
	seen := map[uint64]int{}
	total := 0
	for {
		batch, takeErr := token.TakeFound(session, 2)
		if takeErr != nil {
			t.Fatalf("abholen: %v", takeErr)
		}
		if len(batch) == 0 {
			break
		}
		if len(batch) > 2 {
			t.Fatalf("%d Treffer auf einmal, erlaubt waren 2", len(batch))
		}
		for _, handle := range batch {
			seen[handle]++
		}
		total += len(batch)
	}
	if total != len(all) {
		t.Errorf("%d Treffer abgeholt, %d gefunden", total, len(all))
	}
	for handle, count := range seen {
		if count != 1 {
			t.Errorf("Objekt %d kam %dmal", handle, count)
		}
	}
	token.FindObjectsFinal(session)
}

// Die Zuordnung Geraetefehler -> Rueckgabewert von PKCS#11 haengt am
// Schluessel, nicht am uebersetzten Text.
func TestPkcs11ReturnValues(t *testing.T) {
	cases := map[string]uint64{
		"error.device.none":        CkrTokenNotPresent,
		"error.device.locked":      CkrUserNotLoggedIn,
		"error.device.denied":      CkrFunctionRejected,
		"error.device.notfound":    CkrObjectHandleInvalid,
		"error.device.unsupported": CkrFunctionNotSupported,
		"error.device.io":          CkrDeviceError,
		"error.item.secret":        CkrArgumentsBad,
		"error.protocol.answer":    CkrFunctionFailed,
	}
	for key, want := range cases {
		if got := Pkcs11RV(Fail(key)); got != want {
			t.Errorf("%s gab 0x%x, erwartet 0x%x", key, got, want)
		}
	}
	if got := Pkcs11RV(nil); got != CkrOK {
		t.Errorf("kein Fehler gab 0x%x", got)
	}
	if got := Pkcs11RV(pkcs11Fail(CkrPinLocked, "x")); got != CkrPinLocked {
		t.Errorf("eigener Wert gab 0x%x", got)
	}
}

// Jedes genannte Verfahren muss auch Angaben haben, und ein unbekanntes
// muss abgewiesen werden.
func TestPkcs11MechanismInfo(t *testing.T) {
	token, _, _ := tokenWithKeys(t)
	for _, mechanism := range token.Mechanisms() {
		min, max, flags, err := token.MechanismInfo(mechanism)
		if err != nil {
			t.Errorf("0x%x: %v", mechanism, err)
			continue
		}
		if min == 0 || max < min || flags&CkfSign == 0 {
			t.Errorf("0x%x: min %d, max %d, Kennzeichen 0x%x", mechanism, min, max, flags)
		}
	}
	if _, _, _, err := token.MechanismInfo(0x4711); err == nil {
		t.Error("ein unbekanntes Verfahren wurde beschrieben")
	}
}
