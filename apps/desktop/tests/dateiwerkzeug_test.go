// Das Dateiwerkzeug: osk encrypt und osk decrypt, beide Formate.
//
// Was hier geprueft wird und WARUM so:
//
//   - Rundlauf Byte fuer Byte, auch bei der leeren Datei und bei einer, die
//     groesser ist als ein Abschnitt. Die Abschnittsgrenze ist die Stelle,
//     an der ein Dateiformat schiefgeht, und die leere Datei die, an der ein
//     Sonderfall vergessen wird.
//   - Verfaelschung. Nicht "irgendwo ein Byte", sondern JEDE Stelle der
//     Datei einzeln: Kopf, Fach, Salz, Abschnitt, Siegel. Eine Pruefung, die
//     nur in der Mitte stochert, laesst die Stellen offen, an denen es
//     wirklich darauf ankommt.
//   - Der falsche Empfaenger. Ein zweiter Stick, der dieselbe Datei nicht
//     oeffnen kann.
//   - Dass der SCHLUESSEL NICHT HERAUSKOMMT. Das steht nicht im Kommentar,
//     sondern im Geraeteprotokoll: beim Entschluesseln steht dort `agree`
//     und kein `reveal`. Entschluesselt wird also im Chip, und nicht mit
//     einem geholten Schluessel.
//
// Die Attrappe ist GuardFakeDevice aus classes/ - die, die es schon gibt und
// die gegen shared/protocol.md gebaut ist. Welche ihrer Annahmen am echten
// Geraet noch offen sind, steht in docs/dateiwerkzeug.md.
package tests

import (
	"bytes"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"

	"osk/classes"
)

// dateiKennung baut eine Attrappe und die Kennung dazu.
func dateiKennung(t *testing.T, name string,
	opts classes.GuardFakeOptions) (*classes.GuardFakeDevice, *classes.FileDeviceIdentity) {
	t.Helper()
	opts.Seed = []byte("dateiwerkzeug-" + name)
	device, err := classes.NewGuardFakeDevice(opts)
	if err != nil {
		t.Fatalf("Attrappe %s: %v", name, err)
	}
	cfg := classes.DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.NoBinaryBinding = true
	guard, err := classes.NewGuard(device.GuardFakeClient("dateiwerkzeug"), cfg)
	if err != nil {
		t.Fatalf("Guard %s: %v", name, err)
	}
	identity, err := classes.NewFileDeviceIdentity(guard, "dateien")
	if err != nil {
		t.Fatalf("Kennung %s: %v", name, err)
	}
	return device, identity
}

// dateiFormate sind die zwei Formate. Jeder Fall laeuft in beiden - ein
// Format, das nur in einem der beiden geprueft ist, ist nicht geprueft.
var dateiFormate = []classes.FileFormat{classes.FileFormatAge, classes.FileFormatPGP}

// dateiLaengen sind die Groessen, bei denen ein Dateiformat schiefgeht.
var dateiLaengen = []int{
	0,                             // leer
	1,                             // ein Byte
	1000,                          // weniger als ein Abschnitt
	classes.FileChunkSize - 1,     // knapp darunter
	classes.FileChunkSize,         // genau ein Abschnitt
	classes.FileChunkSize + 1,     // knapp darueber
	3*classes.FileChunkSize + 777, // mehrere Abschnitte und ein Rest
}

// dateiMuster baut Pruefdaten, bei denen jede Verschiebung auffaellt.
func dateiMuster(n int) []byte {
	out := make([]byte, n)
	for i := range out {
		out[i] = byte(i*7 + i/251)
	}
	return out
}

func dateiVerschluesseln(t *testing.T, format classes.FileFormat,
	recipients []classes.FileRecipient, plain []byte) []byte {
	t.Helper()
	var sealed bytes.Buffer
	if err := classes.FileEncrypt(format, recipients, bytes.NewReader(plain), &sealed); err != nil {
		t.Fatalf("%s verschluesseln: %v", format, err)
	}
	return sealed.Bytes()
}

// Rundlauf: verschluesseln und wieder entschluesseln ergibt Byte fuer Byte
// dasselbe.
func TestDateiRundlaufByteFuerByte(t *testing.T) {
	_, identity := dateiKennung(t, "rundlauf", classes.GuardFakeOptions{})
	recipient, err := identity.Recipient()
	if err != nil {
		t.Fatalf("Empfaenger: %v", err)
	}
	for _, format := range dateiFormate {
		for _, length := range dateiLaengen {
			plain := dateiMuster(length)
			sealed := dateiVerschluesseln(t, format, []classes.FileRecipient{recipient}, plain)
			var back bytes.Buffer
			if err := classes.FileDecrypt(format, identity,
				bytes.NewReader(sealed), &back); err != nil {
				t.Fatalf("%s, %d Byte, entschluesseln: %v", format, length, err)
			}
			if !bytes.Equal(back.Bytes(), plain) {
				t.Fatalf("%s, %d Byte: es kommt nicht dasselbe heraus (%d Byte zurueck)",
					format, length, back.Len())
			}
			if length >= 32 && bytes.Contains(sealed, plain) {
				t.Fatalf("%s, %d Byte: der Klartext steht im Chiffrat", format, length)
			}
		}
	}
}

// Jede einzelne Stelle verfaelscht: keine darf still durchgehen.
//
// Geprueft wird jede Stelle, nicht eine Auswahl. Bei den grossen Dateien
// waere das zu viel - darum laeuft dieser Fall mit einer kleinen und einer,
// die ueber die Abschnittsgrenze geht, und bei der grossen wird gesprungen.
func TestDateiVerfaelschungWirdErkannt(t *testing.T) {
	_, identity := dateiKennung(t, "verfaelscht", classes.GuardFakeOptions{})
	recipient, err := identity.Recipient()
	if err != nil {
		t.Fatalf("Empfaenger: %v", err)
	}
	faelle := []struct {
		length int
		step   int
	}{
		{100, 1},
		{classes.FileChunkSize + 100, 997},
	}
	for _, format := range dateiFormate {
		for _, fall := range faelle {
			plain := dateiMuster(fall.length)
			sealed := dateiVerschluesseln(t, format,
				[]classes.FileRecipient{recipient}, plain)
			geprueft := 0
			for position := 0; position < len(sealed); position += fall.step {
				kaputt := append([]byte{}, sealed...)
				kaputt[position] ^= 0x40
				var back bytes.Buffer
				err := classes.FileDecrypt(format, identity,
					bytes.NewReader(kaputt), &back)
				if err == nil && bytes.Equal(back.Bytes(), plain) {
					// Ein Byte, auf das es nicht ankommt, waere ein
					// Fehler im Format - jedes Byte gehoert zu etwas.
					t.Fatalf("%s, %d Byte: Stelle %d geaendert und trotzdem gueltig",
						format, fall.length, position)
				}
				if err == nil {
					t.Fatalf("%s, %d Byte: Stelle %d geaendert, still falsch "+
						"entschluesselt (%d Byte)",
						format, fall.length, position, back.Len())
				}
				geprueft++
			}
			if geprueft == 0 {
				t.Fatal("es wurde keine Stelle geprueft")
			}
			t.Logf("%s, %d Byte: %d Stellen verfaelscht, alle erkannt",
				format, fall.length, geprueft)
		}
	}
}

// Abgeschnitten ist nicht kuerzer: eine Datei, der das Ende fehlt, muss
// auffallen und nicht als kuerzere Datei durchgehen.
func TestDateiAbgeschnittenWirdErkannt(t *testing.T) {
	_, identity := dateiKennung(t, "abgeschnitten", classes.GuardFakeOptions{})
	recipient, err := identity.Recipient()
	if err != nil {
		t.Fatalf("Empfaenger: %v", err)
	}
	for _, format := range dateiFormate {
		plain := dateiMuster(2*classes.FileChunkSize + 5)
		sealed := dateiVerschluesseln(t, format, []classes.FileRecipient{recipient}, plain)
		for _, weg := range []int{1, 17, classes.FileChunkSize} {
			var back bytes.Buffer
			err := classes.FileDecrypt(format, identity,
				bytes.NewReader(sealed[:len(sealed)-weg]), &back)
			if err == nil {
				t.Fatalf("%s: %d Byte abgeschnitten und trotzdem angenommen", format, weg)
			}
			if bytes.Equal(back.Bytes(), plain) {
				t.Fatalf("%s: abgeschnitten und doch alles da", format)
			}
		}
	}
}

// Der falsche Empfaenger kann nicht oeffnen.
func TestDateiFalscherEmpfaengerScheitert(t *testing.T) {
	_, meine := dateiKennung(t, "meine", classes.GuardFakeOptions{})
	_, fremde := dateiKennung(t, "fremde", classes.GuardFakeOptions{})
	recipient, err := meine.Recipient()
	if err != nil {
		t.Fatalf("Empfaenger: %v", err)
	}
	if bytes.Equal(meine.Public(), fremde.Public()) {
		t.Fatal("die zwei Attrappen haben denselben Schluessel - der Fall prueft nichts")
	}
	for _, format := range dateiFormate {
		plain := dateiMuster(5000)
		sealed := dateiVerschluesseln(t, format, []classes.FileRecipient{recipient}, plain)
		var back bytes.Buffer
		if err := classes.FileDecrypt(format, fremde, bytes.NewReader(sealed), &back); err == nil {
			t.Fatalf("%s: der fremde Stick hat die Datei geoeffnet", format)
		}
		if back.Len() != 0 {
			t.Fatalf("%s: der fremde Stick hat %d Byte Klartext bekommen", format, back.Len())
		}
	}
}

// Mehrere Empfaenger: jeder von ihnen kann allein oeffnen, und ein Dritter
// nicht.
func TestDateiMehrereEmpfaenger(t *testing.T) {
	_, eine := dateiKennung(t, "eine", classes.GuardFakeOptions{})
	_, zweite := dateiKennung(t, "zweite", classes.GuardFakeOptions{})
	_, dritte := dateiKennung(t, "dritte", classes.GuardFakeOptions{})
	var recipients []classes.FileRecipient
	for _, identity := range []*classes.FileDeviceIdentity{eine, zweite} {
		recipient, err := identity.Recipient()
		if err != nil {
			t.Fatalf("Empfaenger: %v", err)
		}
		recipients = append(recipients, recipient)
	}
	for _, format := range dateiFormate {
		plain := dateiMuster(3000)
		sealed := dateiVerschluesseln(t, format, recipients, plain)
		for nummer, identity := range []*classes.FileDeviceIdentity{eine, zweite} {
			var back bytes.Buffer
			if err := classes.FileDecrypt(format, identity,
				bytes.NewReader(sealed), &back); err != nil {
				t.Fatalf("%s: Empfaenger %d kann nicht oeffnen: %v", format, nummer+1, err)
			}
			if !bytes.Equal(back.Bytes(), plain) {
				t.Fatalf("%s: Empfaenger %d bekommt etwas anderes", format, nummer+1)
			}
		}
		var back bytes.Buffer
		if err := classes.FileDecrypt(format, dritte, bytes.NewReader(sealed), &back); err == nil {
			t.Fatalf("%s: ein Dritter hat die Datei geoeffnet", format)
		}
	}
}

// Der Schluessel verlaesst den Chip nicht. Das steht im Geraeteprotokoll:
// `agree` kommt vor, `reveal` nicht.
func TestDateiSchluesselBleibtImChip(t *testing.T) {
	device, identity := dateiKennung(t, "protokoll", classes.GuardFakeOptions{})
	recipient, err := identity.Recipient()
	if err != nil {
		t.Fatalf("Empfaenger: %v", err)
	}
	for _, format := range dateiFormate {
		plain := dateiMuster(2000)
		sealed := dateiVerschluesseln(t, format, []classes.FileRecipient{recipient}, plain)
		var back bytes.Buffer
		if err := classes.FileDecrypt(format, identity, bytes.NewReader(sealed), &back); err != nil {
			t.Fatalf("%s: %v", format, err)
		}
	}
	agree := 0
	for _, entry := range device.Log() {
		event := strings.ToLower(entry.Event)
		if strings.Contains(event, "reveal") {
			t.Fatalf("das Geraet hat ein Geheimnis herausgegeben: %s", entry.Event)
		}
		if strings.Contains(event, "agree") {
			agree++
		}
	}
	if agree < len(dateiFormate) {
		t.Fatalf("nur %d Schluesselaustausche im Protokoll, erwartet mindestens %d",
			agree, len(dateiFormate))
	}
}

// Verschluesseln braucht das Geraet gar nicht: der Empfaenger ist
// oeffentlich. Das ist kein Nebensatz, sondern der Grund, warum man fuer
// einen anderen Stick verschluesseln kann, ohne ihn zu haben.
func TestDateiVerschluesselnOhneGeraet(t *testing.T) {
	_, identity := dateiKennung(t, "ohnegeraet", classes.GuardFakeOptions{})
	recipient, err := classes.ParseFileRecipient(mustRecipient(t, identity).String())
	if err != nil {
		t.Fatalf("Empfaenger lesen: %v", err)
	}
	for _, format := range dateiFormate {
		plain := dateiMuster(777)
		sealed := dateiVerschluesseln(t, format, []classes.FileRecipient{recipient}, plain)
		var back bytes.Buffer
		if err := classes.FileDecrypt(format, identity, bytes.NewReader(sealed), &back); err != nil {
			t.Fatalf("%s: %v", format, err)
		}
		if !bytes.Equal(back.Bytes(), plain) {
			t.Fatalf("%s: es kommt nicht dasselbe heraus", format)
		}
	}
}

func mustRecipient(t *testing.T, identity *classes.FileDeviceIdentity) classes.FileRecipient {
	t.Helper()
	recipient, err := identity.Recipient()
	if err != nil {
		t.Fatalf("Empfaenger: %v", err)
	}
	return recipient
}

// Die Schreibweise des Empfaengers: hin und zurueck, und ein vertipptes
// Zeichen faellt auf.
func TestDateiEmpfaengerSchreibweise(t *testing.T) {
	_, identity := dateiKennung(t, "schreibweise", classes.GuardFakeOptions{})
	recipient := mustRecipient(t, identity)
	text := recipient.String()
	if !strings.HasPrefix(text, "age1") {
		t.Fatalf("Empfaenger faengt nicht mit age1 an: %s", text)
	}
	back, err := classes.ParseFileRecipient(text)
	if err != nil {
		t.Fatalf("zurueck lesen: %v", err)
	}
	if !bytes.Equal(back.Public, recipient.Public) {
		t.Fatal("hin und zurueck ergibt einen anderen Schluessel")
	}
	// Jedes einzelne Zeichen vertippen: die Pruefsumme muss es merken.
	const alphabet = "qpzry9x8gf2tvdw0s3jn54khce6mua7l"
	geprueft := 0
	for position := len("age1"); position < len(text); position++ {
		ersatz := alphabet[0]
		if text[position] == ersatz {
			ersatz = alphabet[1]
		}
		kaputt := text[:position] + string(ersatz) + text[position+1:]
		if _, err := classes.ParseFileRecipient(kaputt); err == nil {
			t.Fatalf("vertipptes Zeichen an Stelle %d wurde angenommen", position)
		}
		geprueft++
	}
	if geprueft == 0 {
		t.Fatal("es wurde kein Zeichen geprueft")
	}
}

// Ein privater age-Schluessel wird ausdruecklich abgewiesen. Die Meldung
// sagt, warum - und nicht nur "ungueltig".
func TestDateiPrivaterSchluesselWirdAbgewiesen(t *testing.T) {
	texts := classes.LoadTexts(os.DirFS(".."), "de")
	_, err := classes.ParseFileRecipient(
		"AGE-SECRET-KEY-1QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQSRXFPZQ")
	if err == nil {
		t.Fatal("ein privater Schluessel wurde als Empfaenger angenommen")
	}
	message := texts.Message(err)
	if !strings.Contains(strings.ToLower(message), "geraet") {
		t.Fatalf("die Meldung sagt nicht, dass der Schluessel im Geraet bleibt: %s", message)
	}
}

// Es gibt keinen Weg, der Schluesselmaterial herausgibt. Geprueft wird am
// Quelltext: das Dateiwerkzeug darf keine Ausgabefunktion dafuer haben.
//
// Ein Quelltextfall ist schwaecher als eine Messung, aber hier ist er das
// Richtige: es soll nicht nur heute keinen solchen Weg geben, es soll auch
// morgen keiner dazukommen.
func TestDateiKeinWegFuerPrivateSchluessel(t *testing.T) {
	entries, err := os.ReadDir(filepath.Join("..", "classes"))
	if err != nil {
		t.Fatalf("classes lesen: %v", err)
	}
	verboten := []string{"PrivateKey()", "ExportKey", "SecretKey()", "func (i *FileDeviceIdentity) Private"}
	geprueft := 0
	for _, entry := range entries {
		name := entry.Name()
		if !strings.HasPrefix(name, "file_") || strings.HasSuffix(name, "_test.go") {
			continue
		}
		data, err := os.ReadFile(filepath.Join("..", "classes", name))
		if err != nil {
			t.Fatalf("%s: %v", name, err)
		}
		for _, wort := range verboten {
			if strings.Contains(string(data), wort) {
				t.Errorf("%s enthaelt %q - das ist ein Weg nach draussen", name, wort)
			}
		}
		geprueft++
	}
	if geprueft < 4 {
		t.Fatalf("nur %d Dateien des Werkzeugs gefunden - der Fall prueft nichts", geprueft)
	}
}

// Das Geraet entscheidet, nicht das Programm: verlangt es einen Tastendruck
// und steht niemand davor, wird nicht entschluesselt.
func TestDateiOhneFreigabeKeinOeffnen(t *testing.T) {
	_, identity := dateiKennung(t, "freigabe", classes.GuardFakeOptions{})
	recipient := mustRecipient(t, identity)
	_, streng := dateiKennung(t, "freigabe", classes.GuardFakeOptions{RequireTouch: true})
	if !bytes.Equal(streng.Public(), identity.Public()) {
		t.Fatal("die strenge Attrappe hat einen anderen Schluessel - der Fall prueft nichts")
	}
	for _, format := range dateiFormate {
		plain := dateiMuster(1234)
		sealed := dateiVerschluesseln(t, format, []classes.FileRecipient{recipient}, plain)
		var back bytes.Buffer
		if err := classes.FileDecrypt(format, streng, bytes.NewReader(sealed), &back); err == nil {
			t.Fatalf("%s: ohne Freigabe entschluesselt", format)
		}
	}
}

// Ein Chip ohne Schluesselaustausch (im Werk auf FIPS gestellt) laesst das
// Werkzeug nicht halb laufen, sondern sagt es beim Anmelden der Kennung.
func TestDateiOhneECDHSagtEsFrueh(t *testing.T) {
	device, err := classes.NewGuardFakeDevice(classes.GuardFakeOptions{
		Seed: []byte("dateiwerkzeug-noecdh"), NoECDH: true})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	cfg := classes.DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.NoBinaryBinding = true
	guard, err := classes.NewGuard(device.GuardFakeClient("dateiwerkzeug"), cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	_, err = classes.NewFileDeviceIdentity(guard, "dateien")
	if err == nil {
		t.Fatal("ein Chip ohne Schluesselaustausch wurde angenommen")
	}
	if !classes.GuardNoECDH(err) {
		t.Fatalf("der Grund ist nicht als fehlender Schluesselaustausch erkennbar: %v", err)
	}
}

// Abschnittsweise, nachgerechnet: die Laenge des Chiffrats muss der Rechnung
// aus Abschnitten folgen. Waere die Datei in einem Stueck versiegelt, kaeme
// eine andere Laenge heraus.
func TestDateiLaengeFolgtDenAbschnitten(t *testing.T) {
	_, identity := dateiKennung(t, "abschnitte", classes.GuardFakeOptions{})
	recipient := mustRecipient(t, identity)
	grundlaenge := len(dateiVerschluesseln(t, classes.FileFormatAge,
		[]classes.FileRecipient{recipient}, nil))
	for _, length := range []int{1, classes.FileChunkSize, classes.FileChunkSize + 1,
		2 * classes.FileChunkSize} {
		sealed := dateiVerschluesseln(t, classes.FileFormatAge,
			[]classes.FileRecipient{recipient}, dateiMuster(length))
		abschnitte := length / classes.FileChunkSize
		if length%classes.FileChunkSize != 0 || length == 0 {
			abschnitte++
		}
		// Die leere Datei ist ein leerer Abschnitt; davon ausgehend
		// kommen je Abschnitt die Nutzbytes und ein Siegel hinzu.
		want := grundlaenge + length + (abschnitte-1)*classes.ChaChaTagLen
		if len(sealed) != want {
			t.Fatalf("%d Byte Klartext ergeben %d Byte, erwartet %d (%d Abschnitte)",
				length, len(sealed), want, abschnitte)
		}
	}
}

// Die Abschnittslaenge steht nur an einer Stelle: was das OpenPGP-Paket im
// Kopf ansagt, muss FileChunkSize sein.
func TestDateiAbschnittslaengeNurEinmal(t *testing.T) {
	_, identity := dateiKennung(t, "einezahl", classes.GuardFakeOptions{})
	recipient := mustRecipient(t, identity)
	sealed := dateiVerschluesseln(t, classes.FileFormatPGP,
		[]classes.FileRecipient{recipient}, dateiMuster(10))
	// Das Datenpaket folgt auf das Sitzungsschluesselpaket: Kopfbyte,
	// Laengenbyte, dann der Rumpf. Das vierte Byte des Rumpfes ist die
	// Potenz der Abschnittslaenge.
	start := 2 + int(sealed[1])
	if sealed[start] != 0xc0|18 {
		t.Fatalf("an Stelle %d steht kein Datenpaket (%#x)", start, sealed[start])
	}
	potenz := sealed[start+2+3] // Kopfbyte, Laengenbyte(Teilstueck), dann 4 Felder
	if 1<<(uint(potenz)+6) != classes.FileChunkSize {
		t.Fatalf("das Paket sagt %d Byte an, FileChunkSize ist %d",
			1<<(uint(potenz)+6), classes.FileChunkSize)
	}
}

// Gegenprobe mit einem fremden Programm: gpg liest die Paketstruktur unserer
// Nachricht. Das ist bewusst KEINE Eigenpruefung - ein zweites Programm sagt,
// ob die Huelle stimmt.
//
// Was damit NICHT geprueft ist: ob gpg die Nachricht entschluesseln kann. Das
// braeuchte den privaten Schluessel als Datei, und genau die gibt es hier
// nicht. Was offen bleibt, steht in docs/dateiwerkzeug.md.
func TestDateiGpgLiestDiePakete(t *testing.T) {
	gpg, err := exec.LookPath("gpg")
	if err != nil {
		t.Skip("gpg ist nicht da - diese Gegenprobe laeuft hier nicht")
	}
	_, identity := dateiKennung(t, "gpg", classes.GuardFakeOptions{})
	recipient := mustRecipient(t, identity)
	sealed := dateiVerschluesseln(t, classes.FileFormatPGP,
		[]classes.FileRecipient{recipient}, dateiMuster(3000))
	path := filepath.Join(t.TempDir(), "probe.pgp")
	if err := os.WriteFile(path, sealed, 0o600); err != nil {
		t.Fatalf("schreiben: %v", err)
	}
	cmd := exec.Command(gpg, "--list-packets", path)
	output, _ := cmd.CombinedOutput()
	text := string(output)
	t.Logf("gpg sagt:\n%s", text)
	// Was dieses gpg wirklich bestaetigt: die Huelle. Pakettyp 1 mit 76
	// Byte Rumpf, danach Pakettyp 18 mit genau dem Rest - gelesen von einem
	// Programm, das nicht von hier ist.
	if !strings.Contains(text, "ctb=c1 tag=1 hlen=2 plen=76") {
		t.Errorf("gpg liest das Sitzungsschluesselpaket nicht so, wie es geschrieben wurde")
	}
	rest := len(sealed) - 78 - 3 // Paketkopf 1 Byte, Laenge 2 Byte
	if !strings.Contains(text, "ctb=d2 tag=18 hlen=3 plen="+itoa(rest)) {
		t.Errorf("gpg liest das Datenpaket nicht mit %d Byte Rumpf", rest)
	}
	// Und was es NICHT bestaetigt, steht hier und nicht nur in der Doku:
	// dieses gpg (2.4) kennt die Fassungen aus RFC 9580 noch nicht. Es
	// meldet "unknown version" und liest danach Unsinn. Der Inhalt ist
	// damit NICHT gegengeprueft - erst gpg 2.5 oder neuer koennte das.
	if !strings.Contains(text, "unknown version") {
		t.Log("dieses gpg kennt die neuen Fassungen - dann kann der Inhalt " +
			"gegengeprueft werden, siehe docs/dateiwerkzeug.md")
	}
}

// itoa, damit der Fall ohne strconv lesbar bleibt.
func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	var digits []byte
	for n > 0 {
		digits = append([]byte{byte('0' + n%10)}, digits...)
		n /= 10
	}
	return string(digits)
}
