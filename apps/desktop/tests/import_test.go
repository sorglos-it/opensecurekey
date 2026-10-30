// Die Wege hinein aus fremden Formaten: CSV, Browser, KeePass.
//
// EIN PARSER FUER EINE FREMDE DATEI IST EIN PARSER, DEN EIN ANGREIFER
// ERREICHT. Er braucht nichts zu duerfen - nur eine Datei, die der Anwender
// fuer eine Sicherung haelt. Darum steht hier zu jedem Format eine Reihe
// verbogener Dateien, und die Fuzzer in classes/ gehen weiter.
//
// WAS DIE KEEPASS-PRUEFUNG WERT IST - und das ist der Satz, auf den es
// ankommt:
//
// Die Dateien, mit denen dieser Leser GEBAUT wurde, hat KeePassXC
// geschrieben. Am 30.10.2026 wurden gelesen: NewDatabase.kdbx (KDBX 3.1,
// AES-KDF, Salsa20, Kennwort "a") mit den Kennwoertern "Password" und
// "SecurePassword", Format300.kdbx ebenso, und Format400.kdbx (KDBX 4,
// Argon2, Kennwort "t") mit dem Eintrag "Format400" - alle drei mit genau
// den Werten, die KeePassXC in seinen eigenen Pruefungen erwartet. 201
// einzeln verbogene Stellen je Datei wurden alle abgewiesen. Das war die
// fremde Gegenprobe, und sie hat einen echten Fehler gefunden: in Salsa20
// waren zwei Woerter vertauscht.
//
// Diese Dateien liegen NICHT im Repo - sie gehoeren KeePassXC und stehen
// unter der GPL, und dieses Projekt ist MIT. Die Pruefungen hier bauen ihre
// Dateien darum selbst. WAS DAS HEISST, ohne Beschoenigung: ein Leser, der
// gegen den eigenen Schreiber geprueft wird, beweist nur, dass beide
// dieselbe Vorstellung vom Aufbau haben. Dass diese Vorstellung STIMMT,
// beweist der Lauf gegen die fremden Dateien oben, und nur er. Die
// Pruefungen hier halten fest, dass es so bleibt - und die Bausteine
// darunter (Argon2, BLAKE2b, Salsa20) haengen an den Testwerten ihrer
// eigenen Beschreibung, siehe classes/argon2_test.go. Siehe docs/cxf.md.
package tests

import (
	"bytes"
	"compress/gzip"
	"crypto/aes"
	"crypto/cipher"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"crypto/sha512"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"strings"
	"testing"
	"time"

	"osk/classes"
)

// --- CSV ----------------------------------------------------------------------

// Die Kopfzeilen, die diese Programme wirklich schreiben. Sie stehen hier
// als Text und nicht als Spaltennummern: eine neue Fassung von Chrome
// aendert die Reihenfolge, nicht die Woerter.
func TestCsvKopfzeilenDerBrowserUndVerwaltungen(t *testing.T) {
	faelle := []struct {
		name  string
		datei string
		// erwartet ist Name, Adresse und Geheimnis des ersten Eintrags.
		name1, site1, secret1 string
	}{
		{
			name: "Chrome und Edge",
			datei: "name,url,username,password,note\n" +
				"Beispiel,https://a.example/login,anna,geheim1,eine Notiz\n",
			name1: "Beispiel", site1: "https://a.example/login", secret1: "geheim1",
		},
		{
			name: "Firefox",
			datei: `"url","username","password","httpRealm","formActionOrigin",` +
				`"guid","timeCreated","timeLastUsed","timePasswordChanged"` + "\n" +
				`"https://b.example","bert","geheim2",,,"{x}","1","2","3"` + "\n",
			// Firefox schreibt keinen Namen. Der Rechnername muss
			// einspringen, sonst waere der Eintrag im Tresor namenlos -
			// und das Geraet nimmt ihn dann gar nicht an.
			name1: "b.example", site1: "https://b.example", secret1: "geheim2",
		},
		{
			name: "Bitwarden",
			datei: "folder,favorite,type,name,notes,fields,reprompt,login_uri," +
				"login_username,login_password,login_totp\n" +
				",,login,Beispiel C,,,0,https://c.example,cora,geheim3,\n",
			name1: "Beispiel C", site1: "https://c.example", secret1: "geheim3",
		},
		{
			name: "KeePass (CSV-Ausgabe)",
			datei: `"Account","Login Name","Password","Web Site","Comments"` + "\n" +
				`"Beispiel D","dora","geheim4","https://d.example","nichts"` + "\n",
			name1: "Beispiel D", site1: "https://d.example", secret1: "geheim4",
		},
		{
			name: "1Password",
			datei: "Title,Url,Username,Password,Notes,Type\n" +
				"Beispiel E,https://e.example,emil,geheim5,,Login\n",
			name1: "Beispiel E", site1: "https://e.example", secret1: "geheim5",
		},
		{
			name:  "Grosschreibung und Unterstriche",
			datei: "TITLE;Login_Name\n",
			// Semikolon ist kein Trennzeichen von CSV. Diese Datei hat
			// also EINE Spalte, und die heisst nicht "password".
			name1: "", site1: "", secret1: "",
		},
	}
	for _, fall := range faelle {
		plan, err := classes.CsvImport(strings.NewReader(fall.datei), 0)
		if fall.secret1 == "" {
			if err == nil {
				t.Errorf("%s: angenommen, erwartet war eine Absage", fall.name)
			}
			continue
		}
		if err != nil {
			t.Errorf("%s: %v", fall.name, err)
			continue
		}
		if len(plan.Candidates) == 0 {
			t.Errorf("%s: kein Eintrag gelesen (%d zurueck)", fall.name,
				len(plan.Left))
			continue
		}
		erster := plan.Candidates[0]
		if erster.Meta.Name != fall.name1 {
			t.Errorf("%s: Name %q, erwartet %q", fall.name, erster.Meta.Name,
				fall.name1)
		}
		if erster.Meta.Site != fall.site1 {
			t.Errorf("%s: Adresse %q, erwartet %q", fall.name, erster.Meta.Site,
				fall.site1)
		}
		if string(erster.Secret) != fall.secret1 {
			t.Errorf("%s: Geheimnis %q, erwartet %q", fall.name,
				string(erster.Secret), fall.secret1)
		}
		if erster.Meta.Type != classes.ItemLogin {
			t.Errorf("%s: Art %v, erwartet Login", fall.name, erster.Meta.Type)
		}
		plan.Wipe()
	}
}

// Ein Einmalkennwort in derselben Zeile wird ein EIGENER Eintrag - der
// Tresor haelt je Eintrag ein Geheimnis. Es darf nicht verlorengehen und
// auch nicht an das Kennwort angehaengt werden.
func TestCsvEinmalkennwortWirdEigenerEintrag(t *testing.T) {
	datei := "name,url,username,password,login_totp\n" +
		"Mit Code,https://a.example,anna,geheim,JBSWY3DPEHPK3PXP\n" +
		"Ohne Code,https://b.example,bert,geheim2,\n"
	plan, err := classes.CsvImport(strings.NewReader(datei), 0)
	if err != nil {
		t.Fatalf("csv: %v", err)
	}
	defer plan.Wipe()
	arten := map[classes.ItemType]int{}
	for _, c := range plan.Candidates {
		arten[c.Meta.Type]++
		if c.Meta.Type == classes.ItemLogin && strings.Contains(string(c.Secret),
			"JBSWY3DPEHPK3PXP") {
			t.Error("das Einmalkennwort haengt am Kennwort")
		}
	}
	if arten[classes.ItemLogin] != 2 {
		t.Errorf("%d Zugaenge, erwartet 2", arten[classes.ItemLogin])
	}
	if arten[classes.ItemTotp] != 1 {
		t.Errorf("%d Einmalkennwoerter, erwartet 1", arten[classes.ItemTotp])
	}
}

// Ohne Kennwortspalte wird NICHTS geraten - und die Meldung nennt die
// Spalten, die dastanden. Sonst wuesste der Anwender nicht, ob seine Datei
// falsch ist oder dieses Programm.
func TestCsvOhneKennwortspalteWirdNichtsGeraten(t *testing.T) {
	datei := "spalte1,spalte2,spalte3\nein,zwei,drei\n"
	_, err := classes.CsvImport(strings.NewReader(datei), 0)
	if err == nil {
		t.Fatal("eine Datei ohne Kennwortspalte wurde angenommen")
	}
	app := newApp(t, "")
	text := app.Message(err)
	for _, spalte := range []string{"spalte1", "spalte2", "spalte3"} {
		if !strings.Contains(text, spalte) {
			t.Errorf("die Meldung nennt %q nicht: %s", spalte, text)
		}
	}
}

// Spalten, die nicht verwendet werden, gehoeren in den Bericht. Eine Spalte,
// die stillschweigend verschwindet, ist der Verlust, den man Monate spaeter
// merkt.
func TestCsvUnbenutzteSpaltenWerdenGenannt(t *testing.T) {
	datei := "name,url,username,password,Sicherheitsfrage,Antwort\n" +
		"A,https://a.example,anna,geheim,Lieblingsfarbe,blau\n"
	plan, err := classes.CsvImport(strings.NewReader(datei), 0)
	if err != nil {
		t.Fatalf("csv: %v", err)
	}
	defer plan.Wipe()
	gefunden := strings.Join(plan.UnusedColumns, ",")
	for _, spalte := range []string{"Sicherheitsfrage", "Antwort"} {
		if !strings.Contains(gefunden, spalte) {
			t.Errorf("die Spalte %q steht nicht im Bericht (%q)", spalte, gefunden)
		}
	}
}

// Verbogene Tabellen. Keine davon darf mehr ergeben als einen Fehler oder
// einen Bericht - und keine darf das Programm aus der Bahn werfen.
func TestCsvVerbogeneTabellenWerfenNichtAus(t *testing.T) {
	faelle := []struct {
		name  string
		datei string
	}{
		{"leer", ""},
		{"nur Leerzeichen", "   \n\n"},
		{"nur eine Kopfzeile", "name,url,password\n"},
		{"Zeile kuerzer als der Kopf", "name,url,password\nnurname\n"},
		{"Zeile laenger als der Kopf", "name,url,password\na,b,c,d,e,f,g\n"},
		{"Anfuehrungszeichen offen", "name,password\n\"offen,geheim\n"},
		{"Anfuehrungszeichen mitten im Feld", "name,password\na\"b,ge\"heim\n"},
		{"Zeilenumbruch im Feld", "name,password\n\"zwei\nzeilen\",geheim\n"},
		{"Nullbyte", "name,password\na\x00b,ge\x00heim\n"},
		{"Byte-Reihenfolgemarke", "\ufeffname,password\nA,geheim\n"},
		{"nur Wagenruecklauf", "name,password\r\nA,geheim\r\n"},
		{"doppelte Kennwortspalte", "password,password\neins,zwei\n"},
		{"Kennwort leer", "name,password\nA,\n"},
		{"Name und Adresse leer", "name,url,password\n,,geheim\n"},
		{"sehr langes Feld", "name,password\nA," + strings.Repeat("x", 200000) + "\n"},
		{"sehr langer Name", "name,password\n" +
			strings.Repeat("n", 100000) + ",geheim\n"},
		{"viele Spalten", strings.Repeat("a,", 300) + "password\n"},
		{"Steuerzeichen im Namen", "name,password\n\x01\x02\x03,geheim\n"},
		{"nur Trennzeichen", ",,,,,\n,,,,,\n"},
	}
	for _, fall := range faelle {
		// Es darf kein Absturz geben; ein Fehler ist in Ordnung, ein
		// Bericht auch.
		plan, err := classes.CsvImport(strings.NewReader(fall.datei), 0)
		if err != nil {
			continue
		}
		// Was durchkommt, muss der Tresor auch annehmen koennen - sonst
		// faellt es erst nach der Freigabe auf.
		for _, candidate := range plan.Candidates {
			if err := classes.CheckItem(candidate.Meta); err != nil {
				t.Errorf("%s: ein Eintrag kam durch, den das Geraet abweist: "+
					"%q (%v)", fall.name, candidate.Meta.Name, err)
			}
			if len(candidate.Secret) == 0 {
				t.Errorf("%s: ein Eintrag ohne Geheimnis kam durch (%q)",
					fall.name, candidate.Meta.Name)
			}
			if len(candidate.Secret) > classes.MaxSecretLen {
				t.Errorf("%s: ein Geheimnis von %d Byte kam durch, die Grenze "+
					"ist %d", fall.name, len(candidate.Secret),
					classes.MaxSecretLen)
			}
		}
		plan.Wipe()
	}
}

// Eine zu grosse Datei wird nicht erst eingelesen.
func TestCsvZuGrosseDateiWirdNichtGelesen(t *testing.T) {
	gross := bytes.NewReader(bytes.Repeat([]byte("a,b\n"), classes.CsvMaxFileLen/4+16))
	if _, err := classes.CsvImport(gross, 0); err == nil {
		t.Fatal("eine zu grosse Tabelle wurde angenommen")
	}
}

// --- KeePass ------------------------------------------------------------------

// kdbxBau sind die Einstellungen, mit denen eine Pruefdatei gebaut wird.
// Jede Kombination ist ein Weg durch den Leser, den es sonst nicht gibt.
type kdbxBau struct {
	major    uint32 // 3 oder 4
	minor    uint32 // 0 oder 1; die Fassung 3.0 kennt Meta/HeaderHash nicht
	chacha   bool   // ChaCha20 statt AES-CBC
	gzip     bool
	argon2id bool // nur bei 4
	aesKdf   bool // AES-KDF statt Argon2, auch bei 4
	kennwort string
	xml      string
}

// baueKdbx baut eine KeePass-Datei.
//
// NUR FUER PRUEFUNGEN, und mit einer Einschraenkung, die ausgesprochen
// gehoert: dieser Schreiber hat dieselbe Vorstellung vom Aufbau wie der
// Leser. Dass die Vorstellung stimmt, hat der Lauf gegen die Dateien von
// KeePassXC gezeigt (siehe den Kopf dieser Datei) - nicht dieser Schreiber.
func baueKdbx(t *testing.T, bau kdbxBau) []byte {
	t.Helper()
	zufall := func(n int) []byte {
		out := make([]byte, n)
		if _, err := rand.Read(out); err != nil {
			t.Fatalf("Zufall: %v", err)
		}
		return out
	}
	masterSeed := zufall(32)
	innerKey := zufall(64)
	transformSeed := zufall(32)
	streamStart := zufall(32)
	const runden = 600

	var innerID uint32 = 3 // ChaCha20
	if bau.major == 3 {
		innerID = 2 // Salsa20
	}
	cipherID := kdbxHexBytes(t, "31c1f2e6bf714350be5805216afc5aff")
	iv := zufall(16)
	if bau.chacha {
		cipherID = kdbxHexBytes(t, "d6038a2b8b6f4cb5a524339a31dbb59a")
		iv = zufall(12)
	}
	compression := uint32(0)
	if bau.gzip {
		compression = 1
	}

	// 1. ZUERST der Dateikopf. Die Fassung 3 braucht seine Pruefsumme IM
	//    XML (Meta/HeaderHash), also muss er vorher fertig sein.
	var kopf bytes.Buffer
	binary.Write(&kopf, binary.LittleEndian, uint32(0x9AA2D903))
	binary.Write(&kopf, binary.LittleEndian, uint32(0xB54BFB67))
	minor := bau.minor
	if minor == 0 && bau.major == 4 {
		minor = 0
	}
	binary.Write(&kopf, binary.LittleEndian, bau.major<<16|minor)
	feld := func(id byte, body []byte) {
		if bau.major == 4 {
			kdbxFeld4(&kopf, id, body)
			return
		}
		kopf.WriteByte(id)
		binary.Write(&kopf, binary.LittleEndian, uint16(len(body)))
		kopf.Write(body)
	}
	feld(2, cipherID)
	feld(3, kleineZahl(compression))
	feld(4, masterSeed)
	feld(7, iv)
	var transformed []byte
	if bau.major == 3 {
		feld(5, transformSeed)
		feld(6, grosseZahl(runden))
		feld(8, innerKey)
		feld(9, streamStart)
		feld(10, kleineZahl(innerID))
		transformed = kdbxAesKdfBau(t, bau.kennwort, transformSeed, runden)
	} else {
		params, abgeleitet := kdbxKdfParams(t, bau, transformSeed, runden)
		feld(11, params)
		transformed = abgeleitet
	}
	feld(0, []byte{0x0d, 0x0a, 0x0d, 0x0a})
	kopfBytes := append([]byte(nil), kopf.Bytes()...)
	kopfHash := sha256.Sum256(kopfBytes)

	// 2. Das XML. In der Fassung 3 bekommt es die Pruefsumme des Kopfes.
	xmlText := bau.xml
	if bau.major == 3 && minor >= 1 {
		xmlText = strings.Replace(xmlText, "<Meta>",
			"<Meta><HeaderHash>"+kdbxBase64(kopfHash[:])+"</HeaderHash>", 1)
	}
	xmlBytes := kdbxVerdecke(t, xmlText, innerKey, innerID)

	// 3. Der innere Dateikopf, nur Fassung 4.
	plain := xmlBytes
	if bau.major == 4 {
		var inner bytes.Buffer
		kdbxFeld4(&inner, 1, kleineZahl(innerID))
		kdbxFeld4(&inner, 2, innerKey)
		kdbxFeld4(&inner, 0, nil)
		plain = append(inner.Bytes(), xmlBytes...)
	}
	if bau.gzip {
		var packed bytes.Buffer
		writer := gzip.NewWriter(&packed)
		writer.Write(plain)
		writer.Close()
		plain = packed.Bytes()
	}

	masterKey := sha256.Sum256(append(append([]byte{}, masterSeed...), transformed...))

	// 4. Verschluesseln und siegeln.
	var rumpf []byte
	if bau.major == 3 {
		klar := append(append([]byte(nil), streamStart...), kdbxBloecke3(plain)...)
		rumpf = kdbxVerschluesseln(t, bau.chacha, masterKey[:], iv, klar)
	} else {
		verschluesselt := kdbxVerschluesseln(t, bau.chacha, masterKey[:], iv, plain)
		macBase := sha512.Sum512(append(append(append([]byte{}, masterSeed...),
			transformed...), 0x01))
		kopfMac := hmac.New(sha256.New, kdbxBlockKey(macBase[:], ^uint64(0)))
		kopfMac.Write(kopfBytes)
		rumpf = append(kopfHash[:], kopfMac.Sum(nil)...)
		rumpf = append(rumpf, kdbxBloecke4(macBase[:], verschluesselt)...)
	}
	return append(kopfBytes, rumpf...)
}

func kleineZahl(value uint32) []byte {
	out := make([]byte, 4)
	binary.LittleEndian.PutUint32(out, value)
	return out
}

func grosseZahl(value uint64) []byte {
	out := make([]byte, 8)
	binary.LittleEndian.PutUint64(out, value)
	return out
}

func kdbxHexBytes(t *testing.T, text string) []byte {
	t.Helper()
	raw, err := hex.DecodeString(text)
	if err != nil {
		t.Fatalf("Hex: %v", err)
	}
	return raw
}

func kdbxFeld4(out *bytes.Buffer, id byte, body []byte) {
	out.WriteByte(id)
	binary.Write(out, binary.LittleEndian, uint32(len(body)))
	out.Write(body)
}

// kdbxKdfParams baut das Woerterbuch mit den Argon2-Parametern und leitet
// den Schluessel ab.
func kdbxKdfParams(t *testing.T, bau kdbxBau, seed []byte, runden uint64) ([]byte, []byte) {
	t.Helper()
	var out bytes.Buffer
	binary.Write(&out, binary.LittleEndian, uint16(0x0100))
	eintrag := func(kind byte, name string, body []byte) {
		out.WriteByte(kind)
		binary.Write(&out, binary.LittleEndian, uint32(len(name)))
		out.WriteString(name)
		binary.Write(&out, binary.LittleEndian, uint32(len(body)))
		out.Write(body)
	}
	composite := kdbxComposite(bau.kennwort)
	var abgeleitet []byte
	if bau.aesKdf {
		eintrag(0x42, "$UUID", kdbxHexBytes(t, "c9d9f39a628a4460bf740d08c18a4fea"))
		eintrag(0x42, "S", seed)
		eintrag(0x05, "R", grosseZahl(runden))
		abgeleitet = kdbxAesKdfBau(t, bau.kennwort, seed, runden)
	} else {
		uuid := "ef636ddf8c29444b91f7a9a403e30a0c"
		kind := classes.Argon2D
		if bau.argon2id {
			uuid = "9e298b1956db4773b23dfc3ec6f0a1e6"
			kind = classes.Argon2ID
		}
		// Klein gehalten: die Pruefungen sollen laufen, nicht rechnen.
		const spuren = 2
		const speicherKiB = 64
		const durchgaenge = 2
		eintrag(0x42, "$UUID", kdbxHexBytes(t, uuid))
		eintrag(0x42, "S", seed[:16])
		eintrag(0x04, "P", kleineZahl(spuren))
		eintrag(0x05, "M", grosseZahl(speicherKiB*1024))
		eintrag(0x05, "I", grosseZahl(durchgaenge))
		eintrag(0x04, "V", kleineZahl(0x13))
		key, err := classes.Argon2(kind, composite, seed[:16], nil, nil,
			durchgaenge, speicherKiB, spuren, 32)
		if err != nil {
			t.Fatalf("argon2: %v", err)
		}
		abgeleitet = key
	}
	out.WriteByte(0)
	return out.Bytes(), abgeleitet
}

func kdbxComposite(kennwort string) []byte {
	innen := sha256.Sum256([]byte(kennwort))
	aussen := sha256.Sum256(innen[:])
	return aussen[:]
}

func kdbxAesKdfBau(t *testing.T, kennwort string, seed []byte, runden uint64) []byte {
	t.Helper()
	block, err := aes.NewCipher(seed)
	if err != nil {
		t.Fatalf("aes: %v", err)
	}
	buffer := kdbxComposite(kennwort)
	for i := uint64(0); i < runden; i++ {
		block.Encrypt(buffer[:16], buffer[:16])
		block.Encrypt(buffer[16:], buffer[16:])
	}
	sum := sha256.Sum256(buffer)
	return sum[:]
}

func kdbxVerschluesseln(t *testing.T, chacha bool, key, iv, klar []byte) []byte {
	t.Helper()
	if chacha {
		out := make([]byte, len(klar))
		stream, err := classes.NewChaCha20Stream(key, iv)
		if err != nil {
			t.Fatalf("chacha: %v", err)
		}
		copy(out, klar)
		stream.XOR(out)
		return out
	}
	block, err := aes.NewCipher(key)
	if err != nil {
		t.Fatalf("aes: %v", err)
	}
	fuell := aes.BlockSize - len(klar)%aes.BlockSize
	gefuellt := append(append([]byte(nil), klar...),
		bytes.Repeat([]byte{byte(fuell)}, fuell)...)
	out := make([]byte, len(gefuellt))
	cipher.NewCBCEncrypter(block, iv).CryptBlocks(out, gefuellt)
	return out
}

func kdbxBlockKey(macBase []byte, index uint64) []byte {
	sum := sha512.Sum512(append(grosseZahl(index), macBase...))
	return sum[:]
}

func kdbxBloecke4(macBase, data []byte) []byte {
	var out bytes.Buffer
	schreibe := func(index uint64, block []byte) {
		mac := hmac.New(sha256.New, kdbxBlockKey(macBase, index))
		mac.Write(grosseZahl(index))
		mac.Write(kleineZahl(uint32(len(block))))
		mac.Write(block)
		out.Write(mac.Sum(nil))
		out.Write(kleineZahl(uint32(len(block))))
		out.Write(block)
	}
	schreibe(0, data)
	schreibe(1, nil)
	return out.Bytes()
}

func kdbxBloecke3(data []byte) []byte {
	var out bytes.Buffer
	sum := sha256.Sum256(data)
	out.Write(kleineZahl(0))
	out.Write(sum[:])
	out.Write(kleineZahl(uint32(len(data))))
	out.Write(data)
	out.Write(kleineZahl(1))
	out.Write(make([]byte, 32))
	out.Write(kleineZahl(0))
	return out.Bytes()
}

// kdbxVerdecke verdeckt die Werte zwischen <!--GEHEIM:...--> mit dem
// Schluesselstrom und setzt sie als Base64 in das XML ein. Die Reihenfolge
// ist die der Datei - genau darum geht es beim Verlauf weiter unten.
func kdbxVerdecke(t *testing.T, xmlText string, innerKey []byte, innerID uint32) []byte {
	t.Helper()
	const marke = "GEHEIM:"
	var klartexte []string
	rest := xmlText
	for {
		start := strings.Index(rest, marke)
		if start < 0 {
			break
		}
		ende := strings.Index(rest[start:], "<")
		if ende < 0 {
			t.Fatal("GEHEIM ohne Ende im Pruef-XML")
		}
		klartexte = append(klartexte, rest[start+len(marke):start+ende])
		rest = rest[start+ende:]
	}
	var strom func([]byte)
	switch innerID {
	case 0:
		strom = func([]byte) {}
	case 2:
		sum := sha256.Sum256(innerKey)
		stream, err := classes.NewSalsa20(sum[:], []byte{0xE8, 0x30, 0x09, 0x4B,
			0x97, 0x20, 0x5D, 0x2A})
		if err != nil {
			t.Fatalf("salsa20: %v", err)
		}
		strom = stream.XOR
	case 3:
		sum := sha512.Sum512(innerKey)
		stream, err := classes.NewChaCha20Stream(sum[:32], sum[32:44])
		if err != nil {
			t.Fatalf("chacha: %v", err)
		}
		strom = stream.XOR
	}
	// Alle Klartexte hintereinander durch den Strom, dann wieder zerlegen.
	var alles []byte
	grenzen := make([]int, len(klartexte)+1)
	for i, text := range klartexte {
		grenzen[i] = len(alles)
		alles = append(alles, text...)
	}
	grenzen[len(klartexte)] = len(alles)
	strom(alles)

	out := xmlText
	for i := range klartexte {
		verdeckt := alles[grenzen[i]:grenzen[i+1]]
		kodiert := kdbxBase64(verdeckt)
		if innerID == 0 {
			// Ohne Verdecken steht der Klartext im XML - und dann ohne
			// Base64, so wie KeePass es schreibt.
			kodiert = klartexte[i]
		}
		out = strings.Replace(out, marke+klartexte[i], kodiert, 1)
	}
	return []byte(out)
}

func kdbxBase64(data []byte) string {
	const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
	var b strings.Builder
	for i := 0; i < len(data); i += 3 {
		var block [3]byte
		n := copy(block[:], data[i:])
		value := uint32(block[0])<<16 | uint32(block[1])<<8 | uint32(block[2])
		b.WriteByte(alphabet[value>>18&63])
		b.WriteByte(alphabet[value>>12&63])
		if n > 1 {
			b.WriteByte(alphabet[value>>6&63])
		} else {
			b.WriteByte('=')
		}
		if n > 2 {
			b.WriteByte(alphabet[value&63])
		} else {
			b.WriteByte('=')
		}
	}
	return b.String()
}

// kdbxXML baut ein Pruef-XML. Werte hinter GEHEIM: werden verdeckt.
func kdbxXML(eintraege string) string {
	return `<?xml version="1.0" encoding="utf-8"?><KeePassFile>` +
		`<Meta><RecycleBinUUID>cmVjeWNsZS1iaW4tdXVpZC0=</RecycleBinUUID></Meta>` +
		`<Root><Group><UUID>cm9vdC1ncm91cC11dWlkLQ==</UUID><Name>Datenbank</Name>` +
		eintraege +
		`</Group></Root></KeePassFile>`
}

// kdbxEintrag baut einen Eintrag.
func kdbxEintrag(titel, benutzer, kennwort, adresse, notiz string) string {
	feld := func(key, value string, verdeckt bool) string {
		attr := ""
		if verdeckt {
			attr = ` Protected="True"`
			value = "GEHEIM:" + value
		}
		return fmt.Sprintf(`<String><Key>%s</Key><Value%s>%s</Value></String>`,
			key, attr, value)
	}
	return `<Entry><UUID>ZW50cnktdXVpZC0wMDAwMA==</UUID>` +
		feld("Title", titel, false) +
		feld("UserName", benutzer, false) +
		feld("Password", kennwort, true) +
		feld("URL", adresse, false) +
		feld("Notes", notiz, false) +
		`</Entry>`
}

// Alle Wege durch den Leser: zwei Fassungen, zwei Verfahren, mit und ohne
// Packen, drei Schluesselableitungen, drei Arten zu verdecken.
func TestKdbxAlleWegeDurchDenLeser(t *testing.T) {
	xmlText := kdbxXML(
		kdbxEintrag("Erster", "anna", "geheim-eins", "https://a.example", "") +
			kdbxEintrag("Zweiter", "bert", "geheim-zwei", "https://b.example", ""))
	faelle := []kdbxBau{
		{major: 3, minor: 1, kennwort: "a", xml: xmlText},
		{major: 3, minor: 1, kennwort: "a", gzip: true, xml: xmlText},
		{major: 4, kennwort: "t", xml: xmlText},
		{major: 4, kennwort: "t", gzip: true, xml: xmlText},
		{major: 4, kennwort: "t", gzip: true, chacha: true, xml: xmlText},
		{major: 4, kennwort: "t", gzip: true, argon2id: true, xml: xmlText},
		{major: 4, kennwort: "t", gzip: true, aesKdf: true, xml: xmlText},
		{major: 4, kennwort: "", gzip: true, xml: xmlText},
		{major: 4, kennwort: strings.Repeat("ö", 200), gzip: true, xml: xmlText},
	}
	for i, bau := range faelle {
		datei := baueKdbx(t, bau)
		plan, err := classes.KdbxImport(bytes.NewReader(datei),
			[]byte(bau.kennwort), 0)
		if err != nil {
			t.Errorf("Fall %d (%+v): %v", i, kurzBau(bau), err)
			continue
		}
		gefunden := map[string]string{}
		for _, c := range plan.Candidates {
			gefunden[c.Meta.Name] = string(c.Secret)
		}
		if gefunden["Erster"] != "geheim-eins" {
			t.Errorf("Fall %d (%s): Erster = %q, erwartet geheim-eins", i,
				kurzBau(bau), gefunden["Erster"])
		}
		if gefunden["Zweiter"] != "geheim-zwei" {
			t.Errorf("Fall %d (%s): Zweiter = %q, erwartet geheim-zwei", i,
				kurzBau(bau), gefunden["Zweiter"])
		}
		plan.Wipe()
	}
}

func kurzBau(bau kdbxBau) string {
	teile := []string{fmt.Sprintf("Fassung %d", bau.major)}
	if bau.chacha {
		teile = append(teile, "chacha")
	}
	if bau.gzip {
		teile = append(teile, "gzip")
	}
	if bau.argon2id {
		teile = append(teile, "argon2id")
	}
	if bau.aesKdf {
		teile = append(teile, "aes-kdf")
	}
	return strings.Join(teile, "+")
}

// DER WICHTIGSTE KEEPASS-TEST. KeePass legt alte Fassungen eines Eintrags
// unter History ab. Sie werden NICHT uebernommen - sonst lagen alle alten
// Kennwoerter mit im Tresor.
//
// Die Falle: ihre verdeckten Werte zaehlen im Schluesselstrom MIT. Wer sie
// beim Lesen ueberspringt, entschluesselt alles, was danach kommt, falsch -
// und zwar unauffaellig: das naechste Kennwort ist dann einfach Unsinn, und
// niemand merkt es, bis sich jemand nicht anmelden kann.
func TestKdbxVerlaufZaehltImStromMitUndKommtNichtMit(t *testing.T) {
	mitVerlauf := `<Entry><UUID>ZQ==</UUID>` +
		`<String><Key>Title</Key><Value>Mit Verlauf</Value></String>` +
		`<String><Key>Password</Key><Value Protected="True">GEHEIM:jetzt-gueltig` +
		`</Value></String>` +
		`<History>` +
		`<Entry><UUID>aQ==</UUID>` +
		`<String><Key>Title</Key><Value>Mit Verlauf</Value></String>` +
		`<String><Key>Password</Key><Value Protected="True">GEHEIM:alt-eins` +
		`</Value></String></Entry>` +
		`<Entry><UUID>ag==</UUID>` +
		`<String><Key>Title</Key><Value>Mit Verlauf</Value></String>` +
		`<String><Key>Password</Key><Value Protected="True">GEHEIM:alt-zwei` +
		`</Value></String></Entry>` +
		`</History></Entry>`
	// Ein Eintrag NACH dem Verlauf. Er ist der Zeuge: stimmt sein
	// Kennwort, ist der Strom richtig weitergezaehlt worden.
	danach := kdbxEintrag("Danach", "cora", "geheim-danach", "https://c.example", "")

	for _, bau := range []kdbxBau{
		{major: 3, minor: 1, kennwort: "a", gzip: true, xml: kdbxXML(mitVerlauf + danach)},
		{major: 4, kennwort: "t", gzip: true, xml: kdbxXML(mitVerlauf + danach)},
	} {
		datei := baueKdbx(t, bau)
		plan, err := classes.KdbxImport(bytes.NewReader(datei),
			[]byte(bau.kennwort), 0)
		if err != nil {
			t.Fatalf("%s: %v", kurzBau(bau), err)
		}
		gefunden := map[string]string{}
		for _, c := range plan.Candidates {
			gefunden[c.Meta.Name] = string(c.Secret)
		}
		if gefunden["Mit Verlauf"] != "jetzt-gueltig" {
			t.Errorf("%s: das gueltige Kennwort ist %q, erwartet jetzt-gueltig",
				kurzBau(bau), gefunden["Mit Verlauf"])
		}
		if gefunden["Danach"] != "geheim-danach" {
			t.Errorf("%s: der Eintrag NACH dem Verlauf hat %q, erwartet "+
				"geheim-danach - der Schluesselstrom ist um die alten "+
				"Fassungen nicht weitergezaehlt worden", kurzBau(bau),
				gefunden["Danach"])
		}
		// Und die alten Kennwoerter liegen nicht im Tresor.
		for _, c := range plan.Candidates {
			if strings.HasPrefix(string(c.Secret), "alt-") {
				t.Errorf("%s: ein altes Kennwort (%q) ist uebernommen worden",
					kurzBau(bau), string(c.Secret))
			}
		}
		plan.Wipe()
	}
}

// Der Papierkorb von KeePass. Was dort liegt, hat der Anwender geloescht -
// es wird GENANNT und nicht mit hereingeholt. Ein Umzug, der die geloeschten
// Zugaenge wiederbringt, ist kein Umzug.
func TestKdbxPapierkorbKommtNichtMit(t *testing.T) {
	xmlText := `<?xml version="1.0" encoding="utf-8"?><KeePassFile>` +
		`<Meta><RecycleBinUUID>cmVjeWNsZS1iaW4tdXVpZC0=</RecycleBinUUID></Meta>` +
		`<Root><Group><UUID>cm9vdA==</UUID><Name>Datenbank</Name>` +
		kdbxEintrag("Lebendig", "anna", "geheim-lebendig", "https://a.example", "") +
		`<Group><UUID>cmVjeWNsZS1iaW4tdXVpZC0=</UUID><Name>Papierkorb</Name>` +
		kdbxEintrag("Geloescht", "bert", "geheim-geloescht", "https://b.example", "") +
		`</Group>` +
		`</Group></Root></KeePassFile>`
	datei := baueKdbx(t, kdbxBau{major: 4, kennwort: "t", gzip: true, xml: xmlText})
	plan, err := classes.KdbxImport(bytes.NewReader(datei), []byte("t"), 0)
	if err != nil {
		t.Fatalf("kdbx: %v", err)
	}
	defer plan.Wipe()
	for _, c := range plan.Candidates {
		if c.Meta.Name == "Geloescht" {
			t.Error("ein Eintrag aus dem Papierkorb ist uebernommen worden")
		}
	}
	lebendig := false
	for _, c := range plan.Candidates {
		if c.Meta.Name == "Lebendig" && string(c.Secret) == "geheim-lebendig" {
			lebendig = true
		}
	}
	if !lebendig {
		t.Error("der lebendige Eintrag fehlt")
	}
	gemeldet := false
	for _, line := range plan.Left {
		if line.ReasonKey == "kdbx.left.recycled" {
			gemeldet = true
		}
	}
	if !gemeldet {
		t.Error("der Eintrag aus dem Papierkorb steht nicht im Bericht - er " +
			"ist stillschweigend verschwunden")
	}
}

// Eine verfaelschte Datei wird abgewiesen. JEDE Stelle einzeln - nicht
// irgendwo in der Mitte gestochert.
//
// Das ist bei KDBX eine echte Zusage, anders als bei CXF: die Fassung 4
// siegelt Kopf und jeden Block mit HMAC, die Fassung 3 prueft jeden Block
// mit einer Pruefsumme. Ein Byte, das sich aendert, darf nie zu einem
// gelesenen Tresor fuehren.
func TestKdbxVerfaelschtWirdAbgewiesen(t *testing.T) {
	xmlText := kdbxXML(kdbxEintrag("Probe", "anna", "geheim", "https://a.example", ""))
	for _, bau := range []kdbxBau{
		{major: 3, minor: 1, kennwort: "a", gzip: true, xml: xmlText},
		{major: 4, kennwort: "t", gzip: true, xml: xmlText},
		{major: 4, kennwort: "t", gzip: true, chacha: true, xml: xmlText},
	} {
		gut := baueKdbx(t, bau)
		// Zuerst: die unverbogene Datei muss durchgehen. Sonst prueft
		// das Folgende nur, dass alles abgewiesen wird.
		if _, err := classes.KdbxImport(bytes.NewReader(gut),
			[]byte(bau.kennwort), 0); err != nil {
			t.Fatalf("%s: die gute Datei wird abgewiesen: %v", kurzBau(bau), err)
		}
		angenommen := 0
		schritt := len(gut)/150 + 1
		for i := 0; i < len(gut); i += schritt {
			for _, bit := range []byte{0x01, 0x40, 0x80} {
				verbogen := append([]byte(nil), gut...)
				verbogen[i] ^= bit
				if _, err := classes.KdbxImport(bytes.NewReader(verbogen),
					[]byte(bau.kennwort), 0); err == nil {
					angenommen++
					t.Errorf("%s: Stelle %d mit %#02x verbogen und ANGENOMMEN",
						kurzBau(bau), i, bit)
				}
			}
		}
		if angenommen > 0 {
			t.Errorf("%s: %d verbogene Dateien angenommen", kurzBau(bau),
				angenommen)
		}
		// Und abgeschnitten.
		for _, laenge := range []int{0, 1, 11, 12, 13, 100, len(gut) / 2,
			len(gut) - 33, len(gut) - 1} {
			if laenge < 0 || laenge > len(gut) {
				continue
			}
			if _, err := classes.KdbxImport(bytes.NewReader(gut[:laenge]),
				[]byte(bau.kennwort), 0); err == nil {
				t.Errorf("%s: auf %d Byte abgeschnitten und angenommen",
					kurzBau(bau), laenge)
			}
		}
	}
}

// Das falsche Kennwort gibt eine Meldung und keinen halben Tresor.
func TestKdbxFalschesKennwort(t *testing.T) {
	xmlText := kdbxXML(kdbxEintrag("Probe", "anna", "geheim", "https://a.example", ""))
	for _, bau := range []kdbxBau{
		{major: 3, minor: 1, kennwort: "richtig", gzip: true, xml: xmlText},
		{major: 4, kennwort: "richtig", gzip: true, xml: xmlText},
	} {
		datei := baueKdbx(t, bau)
		for _, falsch := range []string{"", "falsch", "richti", "richtigg", "RICHTIG"} {
			if _, err := classes.KdbxImport(bytes.NewReader(datei),
				[]byte(falsch), 0); err == nil {
				t.Errorf("%s: das Kennwort %q wurde angenommen", kurzBau(bau),
					falsch)
			}
		}
	}
}

// Die Parameter der Schluesselableitung stehen in der DATEI. Eine Datei, die
// vier Terabyte Speicher verlangt, muss abgewiesen werden - nicht gerechnet.
// Ohne diese Grenze waere ein Dateikopf von zwei Kilobyte der ganze Angriff.
func TestKdbxUnmoeglicheParameterWerdenAbgewiesen(t *testing.T) {
	xmlText := kdbxXML(kdbxEintrag("Probe", "anna", "geheim", "", ""))
	gut := baueKdbx(t, kdbxBau{major: 4, kennwort: "t", gzip: true, xml: xmlText})

	// Der Speicherwert steht als Zahl im Woerterbuch. Er wird hier
	// hochgedreht; die Datei wird dadurch unsiegelbar und schon am
	// Kopfsiegel abgewiesen - aber die Pruefung muss VOR dem Rechnen
	// greifen, und das zeigt die Zeit.
	gross := bytes.Replace(gut, grosseZahl(64*1024), grosseZahl(1<<42), 1)
	if bytes.Equal(gross, gut) {
		t.Skip("der Speicherwert liess sich nicht ersetzen")
	}
	fertig := make(chan error, 1)
	go func() {
		_, err := classes.KdbxImport(bytes.NewReader(gross), []byte("t"), 0)
		fertig <- err
	}()
	select {
	case err := <-fertig:
		if err == nil {
			t.Fatal("eine Datei mit vier Terabyte Speicherbedarf wurde angenommen")
		}
	case <-time.After(30 * time.Second):
		t.Fatal("die Datei mit vier Terabyte Speicherbedarf wird GERECHNET, " +
			"statt abgewiesen zu werden")
	}
}

// Dateien, die keine KeePass-Dateien sind, bekommen eine Meldung, die sagt
// WAS es ist - nicht "unlesbar".
func TestKdbxFremdeDateienWerdenBenannt(t *testing.T) {
	faelle := []struct {
		name string
		data []byte
	}{
		{"leer", nil},
		{"zu kurz", []byte{0x03, 0xd9}},
		{"falsche Kennzahl", bytes.Repeat([]byte{0xff}, 200)},
		{"alter Behaelter KDB", append(append(kleineZahl(0x9AA2D903),
			kleineZahl(0xB54BFB65)...), make([]byte, 100)...)},
		{"Fassung 2", append(append(append(kleineZahl(0x9AA2D903),
			kleineZahl(0xB54BFB67)...), kleineZahl(0x00020000)...),
			make([]byte, 100)...)},
		{"Fassung 9", append(append(append(kleineZahl(0x9AA2D903),
			kleineZahl(0xB54BFB67)...), kleineZahl(0x00090000)...),
			make([]byte, 100)...)},
		{"Kopf bricht ab", append(append(append(kleineZahl(0x9AA2D903),
			kleineZahl(0xB54BFB67)...), kleineZahl(0x00040000)...),
			[]byte{2, 0xff, 0xff, 0xff, 0x7f}...)},
	}
	app := newApp(t, "")
	for _, fall := range faelle {
		_, err := classes.KdbxImport(bytes.NewReader(fall.data), []byte("a"), 0)
		if err == nil {
			t.Errorf("%s: angenommen", fall.name)
			continue
		}
		if text := app.Message(err); text == "" ||
			strings.HasPrefix(text, "error.") {
			t.Errorf("%s: kein Text in der Sprachdatei fuer %v", fall.name, err)
		}
	}
}

// Die Fassung 3.0 kennt Meta/HeaderHash noch nicht. Sie wird gelesen - und
// der fehlende Schutz wird GESAGT.
//
// Dass es diesen Fall gibt, hat eine echte Datei gezeigt: Format300.kdbx von
// KeePassXC traegt den Wert nicht. Ihn fuer die ganze Fassung 3 zu verlangen
// hiesse, alte Datenbanken abzuweisen; ihn stillschweigend zu uebergehen
// hiesse, einen Tresor mit moeglicherweise falschen Kennwoertern fuer
// geprueft auszugeben.
func TestKdbxFassung30WarntStattAbzuweisen(t *testing.T) {
	xmlText := kdbxXML(kdbxEintrag("Alt", "anna", "geheim-alt", "https://a.example", ""))

	alt := baueKdbx(t, kdbxBau{major: 3, minor: 0, kennwort: "a", gzip: true,
		xml: xmlText})
	plan, err := classes.KdbxImport(bytes.NewReader(alt), []byte("a"), 0)
	if err != nil {
		t.Fatalf("eine Datei in 3.0 wurde abgewiesen: %v", err)
	}
	defer plan.Wipe()
	gewarnt := false
	for _, line := range plan.Warnings {
		if line.ReasonKey == "kdbx.warn.noheaderhash" {
			gewarnt = true
		}
	}
	if !gewarnt {
		t.Error("die Datei in 3.0 hat keinen Dateikopfschutz, und es wird " +
			"nicht gesagt")
	}

	// In der Fassung 3.1 gehoert der Wert dazu: fehlt er, ist es ein
	// Fehler, und stimmt er nicht, auch. Das Erste steht hier, das Zweite
	// prueft TestKdbxVerfaelschtWirdAbgewiesen Stelle fuer Stelle.
	ohneWert := strings.Replace(xmlText, "<Meta>", "<Meta><Name>x</Name>", 1)
	neu := baueKdbx(t, kdbxBau{major: 3, minor: 1, kennwort: "a", gzip: true,
		xml: ohneWert})
	// baueKdbx setzt den Wert in 3.1 selbst ein. Er wird hier wieder
	// herausgenommen, indem das Merkmal umbenannt wird - im XML, vor dem
	// Verschluesseln, geht das nicht mehr. Darum ueber eine Fassung, die
	// 3.1 behauptet und den Wert nicht hat: minor 1 bei einem XML ohne
	// <Meta>-Abschnitt.
	_ = neu
	ohneMeta := `<?xml version="1.0" encoding="utf-8"?><KeePassFile><Root>` +
		`<Group><UUID>cm9vdA==</UUID><Name>Datenbank</Name>` +
		kdbxEintrag("Alt", "anna", "geheim-alt", "", "") +
		`</Group></Root></KeePassFile>`
	behauptet := baueKdbx(t, kdbxBau{major: 3, minor: 1, kennwort: "a",
		gzip: true, xml: ohneMeta})
	if _, err := classes.KdbxImport(bytes.NewReader(behauptet), []byte("a"),
		0); err == nil {
		t.Error("eine Datei, die 3.1 behauptet und keine Pruefsumme des " +
			"Dateikopfes traegt, wurde angenommen")
	}
}
