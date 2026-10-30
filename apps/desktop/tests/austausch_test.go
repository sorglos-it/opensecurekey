// Die Wege hinein und hinaus: CXF.
//
// Was hier geprueft wird und WARUM so:
//
//   - Das Beispiel DER BESCHREIBUNG, Feld fuer Feld. Ein Format, das nur
//     das eigene Programm liest, ist nicht ausgetauscht - darum liegt in
//     tests/vektoren/cxf-fido-beispiel.json die Beispieldatei aus der
//     Nachtragsliste der FIDO Alliance vom 09.03.2026, und sie wird
//     gelesen. Die Zahlen und Namen darin stammen nicht aus diesem
//     Programm.
//   - Dass OHNE DIE DREI nichts hinausgeht. Gemessen wird es nicht am
//     Rueckgabewert allein, sondern am Geraeteprotokoll: ohne Freigabe
//     steht dort kein einziges `reveal`.
//   - Dass ein Eintrag, der nicht heraus kann, GENANNT wird. Und zwar
//     beides: der chipgebundene (den sagt die Liste des Geraets) und der,
//     den das Geraet verweigert (den sagt erst die Absage auf `reveal`).
//     Die zweite Haelfte ist die wichtigere - das Geraet entscheidet, nicht
//     dieses Programm.
//   - Dass NICHTS STILL VERSCHWINDET: jeder Eintrag, den das Geraet
//     auflistet, kommt im Bericht genau einmal vor. Das ist die Pruefung,
//     die eine halbe Sicherung findet.
//   - Dass eine verbogene Importdatei abgewiesen wird.
//
// Was hier NICHT geprueft werden kann, und das steht auch in docs/cxf.md:
// CXF traegt KEIN Siegel. Eine Datei, in der jemand ein Kennwort aendert,
// ist von einer echten nicht zu unterscheiden - von keinem Programm, nicht
// nur von diesem. "Verbogen" heisst hier darum: kaputt, abgeschnitten,
// ueberlang, falsch geschachtelt, falsche Fassung. Wer ein Siegel will,
// verschluesselt die Datei mit `osk encrypt`.
package tests

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"testing"
	"time"

	"osk/classes"
)

// --- Das Beispiel der Beschreibung --------------------------------------------

func beispielDatei(t *testing.T) []byte {
	t.Helper()
	data, err := os.ReadFile(filepath.Join("vektoren", "cxf-fido-beispiel.json"))
	if err != nil {
		t.Fatalf("Beispieldatei: %v", err)
	}
	return data
}

// Die Beispieldatei der Beschreibung wird gelesen, und zwar vollstaendig.
// Jede Zahl in diesem Test steht in der Beispieldatei und nicht in diesem
// Programm.
func TestCxfBeispielDerBeschreibungWirdGelesen(t *testing.T) {
	header, err := classes.CxfParse(beispielDatei(t))
	if err != nil {
		t.Fatalf("das Beispiel der Beschreibung ist unlesbar: %v", err)
	}
	if header.Version.Major != 1 || header.Version.Minor != 0 {
		t.Errorf("Fassung %d.%d, erwartet 1.0", header.Version.Major,
			header.Version.Minor)
	}
	if header.ExporterRpID != "exporter.example.com" {
		t.Errorf("Ausgeber %q", header.ExporterRpID)
	}
	if header.Timestamp != 1705228800 {
		t.Errorf("Zeitstempel %d, erwartet 1705228800", header.Timestamp)
	}
	if len(header.Accounts) != 1 {
		t.Fatalf("%d Konten, erwartet 1", len(header.Accounts))
	}
	account := header.Accounts[0]
	if account.Username != "jane_smith" || account.Email != "jane.smith@example.com" {
		t.Errorf("Konto %q / %q", account.Username, account.Email)
	}
	// Die Kennung steht in der Datei als Base64 ohne Fuellzeichen. Sie
	// muss als 16 Byte herauskommen - nicht als Text.
	if len(account.ID) != 16 {
		t.Errorf("Kontokennung %d Byte, erwartet 16", len(account.ID))
	}
	if len(account.Items) != 14 {
		t.Fatalf("%d Eintraege, erwartet 14", len(account.Items))
	}
	if len(account.Collections) != 1 {
		t.Fatalf("%d Sammlungen, erwartet 1", len(account.Collections))
	}
	if got := len(account.Collections[0].Items); got != 3 {
		t.Errorf("%d Verweise in der Sammlung, erwartet 3", got)
	}

	// Alle siebzehn Arten der Beschreibung kommen in der Datei nicht vor -
	// fuenfzehn tun es. Jede muss als die Art herauskommen, die dasteht.
	var arten []string
	for _, item := range account.Items {
		for _, credential := range item.Credentials {
			arten = append(arten, credential.CxfType())
		}
	}
	erwartet := []string{
		"address", "api-key", "basic-auth", "credit-card", "drivers-license",
		"file", "generated-password", "identity-document", "note", "passkey",
		"passport", "person-name", "ssh-key", "totp", "wifi",
	}
	sort.Strings(arten)
	if strings.Join(arten, ",") != strings.Join(erwartet, ",") {
		t.Errorf("Arten:\n  bekommen %v\n  erwartet %v", arten, erwartet)
	}
}

// Die Felder der Arten, die dieses Programm auseinandernimmt - einzeln
// gegen die Beispieldatei. Das ist der Teil, an dem ein stiller Fehler
// sitzt: ein Feld, das falsch heisst, kommt als leerer Wert durch und
// faellt nirgends auf.
func TestCxfBeispielFelderStimmenFeldFuerFeld(t *testing.T) {
	header, err := classes.CxfParse(beispielDatei(t))
	if err != nil {
		t.Fatalf("Beispiel: %v", err)
	}
	items := header.Accounts[0].Items

	// basic-auth
	login, ok := items[0].Credentials[0].(*classes.CxfBasicAuth)
	if !ok {
		t.Fatalf("erster Eintrag ist %T, erwartet CxfBasicAuth", items[0].Credentials[0])
	}
	if login.Username == nil || login.Username.Value != "johndoe" {
		t.Errorf("Benutzername %+v", login.Username)
	}
	if login.Password == nil || login.Password.Value != "securepassword123" {
		t.Errorf("Kennwort %+v", login.Password)
	}
	// Die Feldart ist nicht Zierde: daran entscheidet ein Programm, ob es
	// den Wert abdeckt.
	if login.Password == nil || login.Password.FieldType != classes.CxfFieldConcealed {
		t.Errorf("die Kennwortart ist %q, erwartet %q",
			login.Password.FieldType, classes.CxfFieldConcealed)
	}
	if login.Username == nil || len(login.Username.ID) == 0 {
		t.Error("die Feldkennung des Benutzernamens fehlt")
	}

	// totp
	totp, ok := items[0].Credentials[1].(*classes.CxfTotp)
	if !ok {
		t.Fatalf("zweites Zugangsdatum ist %T, erwartet CxfTotp", items[0].Credentials[1])
	}
	if totp.Secret != "JBSWY3DPEHPK3PXP" || totp.Period != 30 || totp.Digits != 6 {
		t.Errorf("totp %+v", totp)
	}
	if totp.Algorithm != "sha256" {
		t.Errorf("Verfahren %q, erwartet sha256 (klein geschrieben)", totp.Algorithm)
	}
	if totp.Issuer != "GitHub" {
		t.Errorf("Herausgeber %q, erwartet GitHub - die Nachtragsliste vom "+
			"09.03.2026 hat das von Google auf GitHub geaendert", totp.Issuer)
	}

	// Geltungsbereich
	if items[0].Scope == nil || len(items[0].Scope.URLs) != 1 ||
		items[0].Scope.URLs[0] != "https://github.com" {
		t.Errorf("Geltungsbereich %+v", items[0].Scope)
	}
	if len(items[0].Tags) != 3 {
		t.Errorf("%d Marken, erwartet 3", len(items[0].Tags))
	}

	// passkey
	passkey, ok := items[1].Credentials[0].(*classes.CxfPasskey)
	if !ok {
		t.Fatalf("Passkey ist %T", items[1].Credentials[0])
	}
	if passkey.RpID != "webauthn.io" || passkey.Username != "johndoe" ||
		passkey.UserDisplayName != "John Doe" {
		t.Errorf("Passkey %+v", passkey)
	}
	if len(passkey.CredentialID) == 0 || len(passkey.UserHandle) != 32 ||
		len(passkey.Key) == 0 {
		t.Errorf("Passkey-Rohdaten: id=%d handle=%d key=%d",
			len(passkey.CredentialID), len(passkey.UserHandle), len(passkey.Key))
	}
	// Die Erweiterungen werden durchgetragen und nicht umgeschrieben: die
	// Nachtragsliste hat dort Namen geaendert, und was dieses Programm
	// nicht auswertet, soll es auch nicht verlieren.
	if !bytes.Contains(passkey.Fido2Extensions, []byte("hmacCredentials")) {
		t.Error("die FIDO2-Erweiterungen sind nicht durchgetragen")
	}

	// note
	var notiz *classes.CxfNote
	for _, item := range items {
		if value, ok := item.Credentials[0].(*classes.CxfNote); ok {
			notiz = value
		}
	}
	if notiz == nil || !strings.Contains(notiz.Content.Value, "alarm") {
		t.Errorf("Notiz %+v", notiz)
	}

	// ssh-key
	var schluessel *classes.CxfSSHKey
	for _, item := range items {
		if value, ok := item.Credentials[0].(*classes.CxfSSHKey); ok {
			schluessel = value
		}
	}
	if schluessel == nil {
		t.Fatal("kein SSH-Schluessel gelesen")
	}
	if schluessel.KeyType != "ssh-rsa" {
		t.Errorf("Schluesselart %q", schluessel.KeyType)
	}
	if schluessel.KeyComment != "Work SSH Key" {
		t.Errorf("Bemerkung %q", schluessel.KeyComment)
	}
	// Der private Teil ist Base64 in der ADRESSSCHREIBWEISE ueber der
	// DER-Form. Dass er sich als PKCS#8 lesen laesst, ist die Probe
	// darauf, dass die Schreibweise stimmt: mit dem normalen Alphabet
	// kaeme Unsinn heraus.
	if len(schluessel.PrivateKey) < 1000 {
		t.Errorf("privater Teil %d Byte - zu kurz fuer einen RSA-Schluessel",
			len(schluessel.PrivateKey))
	}

	// generated-password: ein nackter Text, kein Feld.
	var erzeugt *classes.CxfGenPassword
	for _, item := range items {
		if value, ok := item.Credentials[0].(*classes.CxfGenPassword); ok {
			erzeugt = value
		}
	}
	if erzeugt == nil || erzeugt.Password != "KozyS!cf#Nc9C799" {
		t.Errorf("erzeugtes Kennwort %+v", erzeugt)
	}
}

// Die Beispieldatei wieder herausschreiben und noch einmal lesen muss
// dasselbe ergeben. Das prueft die Richtung hinaus gegen die Richtung
// hinein - mit Werten, die nicht aus diesem Programm stammen.
func TestCxfBeispielRundlaufVerliertNichts(t *testing.T) {
	header, err := classes.CxfParse(beispielDatei(t))
	if err != nil {
		t.Fatalf("Beispiel: %v", err)
	}
	var erste bytes.Buffer
	if err := classes.CxfWrite(header, &erste); err != nil {
		t.Fatalf("schreiben: %v", err)
	}
	wieder, err := classes.CxfParse(erste.Bytes())
	if err != nil {
		t.Fatalf("das eigene Geschriebene ist unlesbar: %v", err)
	}
	var zweite bytes.Buffer
	if err := classes.CxfWrite(wieder, &zweite); err != nil {
		t.Fatalf("zweites Schreiben: %v", err)
	}
	if !bytes.Equal(erste.Bytes(), zweite.Bytes()) {
		t.Error("zweimal geschrieben ergibt nicht dasselbe")
	}
	// Und die Arten muessen dieselben sein - auch die, die dieses
	// Programm nicht auseinandernimmt.
	if arten(header) != arten(wieder) {
		t.Errorf("Arten verloren:\n  vorher  %s\n  nachher %s",
			arten(header), arten(wieder))
	}
}

func arten(header *classes.CxfHeader) string {
	var out []string
	for _, account := range header.Accounts {
		for _, item := range account.Items {
			for _, credential := range item.Credentials {
				out = append(out, credential.CxfType())
			}
		}
	}
	sort.Strings(out)
	return strings.Join(out, ",")
}

// --- Die drei: PIN, Tastendruck, Anzeige --------------------------------------

// zaehleReveal liest das Geraeteprotokoll und zaehlt, wie oft ein Geheimnis
// herausgegeben wurde. Das ist die Messung - nicht der Rueckgabewert.
func zaehleReveal(t *testing.T, client *classes.Client) int {
	t.Helper()
	entries, err := client.Log(400)
	if err != nil {
		t.Fatalf("log: %v", err)
	}
	count := 0
	for _, entry := range entries {
		if strings.EqualFold(entry.Event, "reveal") {
			count++
		}
	}
	return count
}

// Ohne Freigabe geht nichts hinaus. Und zwar nicht "es kommt ein Fehler",
// sondern: das Geraet hat kein einziges Geheimnis herausgegeben.
func TestCxfOhneFreigabeWirdNichtsGeholt(t *testing.T) {
	app := newApp(t, startSimulator(t))
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	if err := client.Unlock([]byte(simPin)); err != nil {
		t.Fatalf("unlock: %v", app.Message(err))
	}
	if _, err := client.Add(classes.Item{Type: classes.ItemLogin,
		Name: "Freigabe-Probe", Site: "a.example", Mirrored: true},
		[]byte("geheim")); err != nil {
		t.Fatalf("anlegen: %v", app.Message(err))
	}
	vorher := zaehleReveal(t, client)

	faelle := []struct {
		name    string
		consent classes.CxfConsent
	}{
		{"die Nullform", classes.CxfConsent{}},
		{"ohne Ja", classes.CxfConsent{Destination: "sicherung.json"}},
		{"ohne Ziel", classes.CxfConsent{Confirmed: true}},
		{"Ziel aus Leerzeichen", classes.CxfConsent{Destination: "   ", Confirmed: true}},
		{"Ziel mit Steuerzeichen", classes.CxfConsent{
			Destination: "sicherung\x00.json", Confirmed: true}},
		{"Ziel zu lang", classes.CxfConsent{
			Destination: strings.Repeat("z", classes.MaxSiteLen+1), Confirmed: true}},
	}
	for _, fall := range faelle {
		header, _, err := classes.CxfExport(client, 0, fall.consent)
		if err == nil {
			t.Errorf("%s: angenommen, es entstand eine Datei mit %d Konten",
				fall.name, len(header.Accounts))
		}
	}
	if nachher := zaehleReveal(t, client); nachher != vorher {
		t.Fatalf("ohne Freigabe wurden %d Geheimnisse geholt (vorher %d, "+
			"nachher %d)", nachher-vorher, vorher, nachher)
	}
}

// Ein gesperrtes Geraet gibt nichts heraus - und gefragt wird das GERAET,
// nicht ein Merker in diesem Programm.
func TestCxfGesperrtesGeraetGibtNichtsHeraus(t *testing.T) {
	app := newApp(t, startSimulator(t))
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	// Absichtlich KEIN Unlock.
	_, _, err = classes.CxfExport(client, 0,
		classes.CxfConsent{Destination: "sicherung.json", Confirmed: true})
	if err == nil {
		t.Fatal("ein gesperrtes Geraet hat exportiert")
	}
	if got := app.Message(err); got == "" {
		t.Error("kein Text zur Meldung")
	}
}

// --- Was nicht mitkommt, wird genannt -----------------------------------------

// Der Kern dieses Strangs. Drei Faelle in einem Lauf:
//
//	gespiegelt   - kommt mit
//	chipgebunden - kommt nicht mit, sagt die LISTE des Geraets
//	verweigert   - kommt nicht mit, sagt die ABSAGE des Geraets auf reveal
//
// Der dritte ist der wichtige: dort entscheidet das Geraet, und dieses
// Programm darf die Absage weder uebergehen noch zum Abbruch machen. Die
// Attrappe wird dafuer mit -reveal-denied aufgerufen; sie behauptet damit
// keine Regel, sie verweigert genau den genannten Eintrag.
func TestCxfNichtExportierbaresWirdGemeldet(t *testing.T) {
	app := newApp(t, startSimulator(t, "-reveal-denied", "Verweigert"))
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	if err := client.Unlock([]byte(simPin)); err != nil {
		t.Fatalf("unlock: %v", app.Message(err))
	}
	anlegen := func(name string, mirrored bool) {
		if _, err := client.Add(classes.Item{Type: classes.ItemLogin, Name: name,
			Site: "x.example", Mirrored: mirrored}, []byte("geheim-"+name)); err != nil {
			t.Fatalf("anlegen %s: %v", name, app.Message(err))
		}
	}
	anlegen("Gespiegelt", true)
	anlegen("Chipgebunden", false)
	anlegen("Verweigert", true)

	header, report, err := classes.CxfExport(client, 0,
		classes.CxfConsent{Destination: "USB-Stick E:", Confirmed: true})
	if err != nil {
		t.Fatalf("export: %v", app.Message(err))
	}
	if report.Destination != "USB-Stick E:" {
		t.Errorf("Ziel im Bericht %q", report.Destination)
	}

	wo := map[string]string{}
	for _, line := range report.Taken {
		wo[line.Name] = "mit"
	}
	for _, line := range report.Flattened {
		wo[line.Name] = "ungenau:" + line.ReasonKey
	}
	for _, line := range report.Left {
		wo[line.Name] = "zurueck:" + line.ReasonKey
	}

	if wo["Gespiegelt"] != "mit" {
		t.Errorf("Gespiegelt: %q, erwartet mit", wo["Gespiegelt"])
	}
	if got := wo["Chipgebunden"]; got != "zurueck:cxf.left.chipbound" {
		t.Errorf("Chipgebunden: %q, erwartet zurueck:cxf.left.chipbound", got)
	}
	if got := wo["Verweigert"]; got != "zurueck:cxf.left.denied" {
		t.Errorf("Verweigert: %q, erwartet zurueck:cxf.left.denied - die Absage "+
			"des Geraets muss im Bericht stehen", got)
	}
	if report.Complete() {
		t.Error("der Bericht nennt sich vollstaendig, obwohl Eintraege fehlen")
	}
	if report.SummaryKey() != "cxf.report.incomplete" {
		t.Errorf("Zusammenfassung %q", report.SummaryKey())
	}

	// Und: in der Datei steht der zurueckgebliebene Eintrag auch wirklich
	// nicht. Ein Bericht, der ihn nennt, und eine Datei, die ihn doch
	// enthaelt, waere das Schlechteste von beidem.
	var geschrieben bytes.Buffer
	if err := classes.CxfWrite(header, &geschrieben); err != nil {
		t.Fatalf("schreiben: %v", err)
	}
	for _, name := range []string{"Chipgebunden", "Verweigert"} {
		if bytes.Contains(geschrieben.Bytes(), []byte(name)) {
			t.Errorf("%q steht in der Datei, obwohl er als zurueckgeblieben "+
				"gemeldet wurde", name)
		}
	}
	if !bytes.Contains(geschrieben.Bytes(), []byte("geheim-Gespiegelt")) {
		t.Error("das Geheimnis des gespiegelten Eintrags steht nicht in der Datei")
	}
}

// Die Pruefung, die eine halbe Sicherung findet: JEDER Eintrag, den das
// Geraet auflistet, kommt im Bericht genau einmal vor. Keiner mehr, keiner
// weniger.
//
// Diese eine Zahl ist mehr wert als jede Liste von Sonderfaellen: eine neue
// Eintragsart in items.go, fuer die niemand an den Ausgang gedacht hat,
// faellt hier auf - sie ist dann weder mit noch genannt.
func TestCxfNichtsVerschwindetStill(t *testing.T) {
	app := newApp(t, startSimulator(t, "-reveal-denied", "Verweigert"))
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	if err := client.Unlock([]byte(simPin)); err != nil {
		t.Fatalf("unlock: %v", app.Message(err))
	}
	// Von JEDER Art einen Eintrag, damit keine Art ungeprueft bleibt.
	for _, art := range classes.ItemTypes() {
		geheim := []byte("geheimnis fuer art " + app.Text(art.Key()))
		switch art {
		case classes.ItemSSHKey:
			geheim = []byte(pruefSchluessel)
		case classes.ItemTotp:
			geheim = []byte("JBSWY3DPEHPK3PXP")
		}
		if _, err := client.Add(classes.Item{Type: art,
			Name: "Art " + app.Text(art.Key()), Site: "art.example",
			Mirrored: true}, geheim); err != nil {
			t.Fatalf("anlegen %v: %v", art, app.Message(err))
		}
	}
	if _, err := client.Add(classes.Item{Type: classes.ItemLogin,
		Name: "Verweigert", Mirrored: true}, []byte("geheim")); err != nil {
		t.Fatalf("anlegen: %v", app.Message(err))
	}

	items, err := client.List(0, "")
	if err != nil {
		t.Fatalf("list: %v", app.Message(err))
	}
	_, report, err := classes.CxfExport(client, 0,
		classes.CxfConsent{Destination: "sicherung.json", Confirmed: true})
	if err != nil {
		t.Fatalf("export: %v", app.Message(err))
	}

	gesehen := map[uint32]int{}
	for _, gruppe := range [][]classes.CxfLine{report.Taken, report.Flattened,
		report.Left} {
		for _, line := range gruppe {
			gesehen[line.ID]++
		}
	}
	for _, item := range items {
		switch gesehen[item.ID] {
		case 1:
		case 0:
			t.Errorf("Eintrag %d (%q, Art %v) kommt im Bericht NICHT vor - er "+
				"ist stillschweigend verschwunden", item.ID, item.Name, item.Type)
		default:
			t.Errorf("Eintrag %d (%q) kommt %d mal im Bericht vor", item.ID,
				item.Name, gesehen[item.ID])
		}
	}
	gesamt := len(report.Taken) + len(report.Flattened) + len(report.Left)
	if gesamt != len(items) {
		t.Errorf("%d Zeilen im Bericht, %d Eintraege im Geraet", gesamt, len(items))
	}
	// Und jede zurueckgebliebene Zeile MUSS einen Grund tragen. Eine Zeile
	// ohne Grund ist eine Zeile, die niemand erklaeren kann.
	for _, line := range report.Left {
		if line.ReasonKey == "" {
			t.Errorf("Eintrag %d (%q) ist zurueckgeblieben, ohne Grund",
				line.ID, line.Name)
		}
	}
	for _, line := range report.Flattened {
		if line.ReasonKey == "" {
			t.Errorf("Eintrag %d (%q) kam ungenauer mit, ohne dass es dasteht",
				line.ID, line.Name)
		}
	}
}

// --- Der Rundlauf -------------------------------------------------------------

// Export, Import in ein ZWEITES Geraet, Export - und dann muss dasselbe
// herauskommen.
//
// "Dasselbe" heisst hier: dieselben Namen, Adressen, Arten und Geheimnisse.
// NICHT dieselben Kennungen und Zeitstempel: die Eintragsnummer gibt das
// Geraet, und das zweite Geraet gibt andere. Das ist kein Verlust, und es
// hier als Gleichheit zu verlangen hiesse, einen Test zu haben, der nur
// durch Zufall gruen ist.
func TestCxfRundlaufExportImportExport(t *testing.T) {
	ersterApp := newApp(t, startSimulator(t))
	erster, err := ersterApp.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", err)
	}
	if err := erster.Unlock([]byte(simPin)); err != nil {
		t.Fatalf("unlock: %v", err)
	}

	// Geheimnisse, bei denen sich ein Fehler zeigt: Sonderzeichen,
	// Umlaute, ein Zeilenumbruch, ein langes.
	proben := []struct {
		art    classes.ItemType
		name   string
		site   string
		geheim string
	}{
		{classes.ItemLogin, "Rundlauf Zugang", "https://a.example", "ge\"hei'm&<>\\/`$"},
		{classes.ItemLogin, "Rundlauf Umlaut", "b.example", "Käsekuchen-Paßwört-42"},
		{classes.ItemNote, "Rundlauf Notiz", "c.example", "Zeile eins\nZeile zwei\tTab"},
		{classes.ItemTotp, "Rundlauf Einmalcode", "d.example", "JBSWY3DPEHPK3PXP"},
		{classes.ItemAPIKey, "Rundlauf Schluessel", "e.example",
			strings.Repeat("k", 512)},
		{classes.ItemSSHKey, "Rundlauf SSH", "git.example", pruefSchluessel},
	}
	for _, probe := range proben {
		if _, err := erster.Add(classes.Item{Type: probe.art, Name: probe.name,
			Site: probe.site, Mirrored: true}, []byte(probe.geheim)); err != nil {
			t.Fatalf("anlegen %s: %v", probe.name, ersterApp.Message(err))
		}
	}

	header, _, err := classes.CxfExport(erster, 0,
		classes.CxfConsent{Destination: "rundlauf.json", Confirmed: true})
	if err != nil {
		t.Fatalf("erster Export: %v", ersterApp.Message(err))
	}
	var datei bytes.Buffer
	if err := classes.CxfWrite(header, &datei); err != nil {
		t.Fatalf("schreiben: %v", err)
	}

	// Die Datei wird wieder GELESEN, nicht die Struktur weitergegeben: ein
	// Rundlauf, der das Schreiben ueberspringt, prueft das Format nicht.
	gelesen, err := classes.CxfParse(datei.Bytes())
	if err != nil {
		t.Fatalf("die eigene Datei ist unlesbar: %v", err)
	}
	plan, err := classes.CxfImport(gelesen, 0)
	if err != nil {
		t.Fatalf("import: %v", err)
	}
	defer plan.Wipe()

	zweiterApp := newApp(t, startSimulator(t))
	zweiter, err := zweiterApp.Client()
	if err != nil {
		t.Fatalf("zweites Geraet: %v", err)
	}
	if err := zweiter.Unlock([]byte(simPin)); err != nil {
		t.Fatalf("unlock 2: %v", err)
	}
	if _, err := classes.CxfApply(zweiter, plan); err != nil {
		t.Fatalf("anlegen im zweiten Geraet: %v", zweiterApp.Message(err))
	}

	zweiterHeader, _, err := classes.CxfExport(zweiter, 0,
		classes.CxfConsent{Destination: "rundlauf2.json", Confirmed: true})
	if err != nil {
		t.Fatalf("zweiter Export: %v", zweiterApp.Message(err))
	}

	vorher := kern(t, header, "Rundlauf ")
	nachher := kern(t, zweiterHeader, "Rundlauf ")
	if len(vorher) == 0 {
		t.Fatal("der erste Export enthaelt keine Probe")
	}
	if len(vorher) != len(nachher) {
		t.Fatalf("%d Zugangsdaten hinaus, %d wieder hinaus:\n  %v\n  %v",
			len(vorher), len(nachher), vorher, nachher)
	}
	for i := range vorher {
		if vorher[i] != nachher[i] {
			t.Errorf("Rundlauf verschieden:\n  hinaus  %s\n  wieder  %s",
				vorher[i], nachher[i])
		}
	}
}

// kern macht aus einer Datei eine vergleichbare Liste: Art, Titel und
// Geheimnis, ohne Kennungen und Zeitstempel. Nur die Eintraege, deren Titel
// mit dem Vorsatz anfangen - die Attrappe bringt eigene Beispieleintraege
// mit, und die gehoeren nicht in diesen Vergleich.
func kern(t *testing.T, header *classes.CxfHeader, vorsatz string) []string {
	t.Helper()
	var out []string
	for _, account := range header.Accounts {
		for _, item := range account.Items {
			if !strings.HasPrefix(item.Title, vorsatz) {
				continue
			}
			for _, credential := range item.Credentials {
				raw, err := json.Marshal(credential)
				if err != nil {
					t.Fatalf("%s: %v", item.Title, err)
				}
				out = append(out, item.Title+"|"+item.Subtitle+"|"+string(raw))
			}
		}
	}
	sort.Strings(out)
	return out
}

// --- Verbogene Importdateien --------------------------------------------------

// Eine fremde Datei ist eine fremde Eingabe. Jeder dieser Faelle muss einen
// Fehler ergeben und nicht einen halb verstandenen Tresor.
func TestCxfVerbogeneDateiWirdAbgewiesen(t *testing.T) {
	gut := beispielDatei(t)
	faelle := []struct {
		name string
		data []byte
	}{
		{"leer", nil},
		{"nur Leerzeichen", []byte("   \n\t ")},
		{"kein JSON", []byte("das ist keine Datei")},
		{"nur eine Klammer", []byte("{")},
		{"abgeschnitten in der Mitte", gut[:len(gut)/2]},
		{"abgeschnitten um ein Byte", gut[:len(gut)-2]},
		{"zwei Dateien hintereinander", append(append([]byte(nil), gut...), gut...)},
		{"etwas dahinter", append(append([]byte(nil), gut...), []byte("xyz")...)},
		{"leeres Objekt", []byte(`{}`)},
		{"kein Konto", []byte(`{"version":{"major":1,"minor":0},` +
			`"exporterRpId":"a","exporterDisplayName":"b","timestamp":1,` +
			`"accounts":[]}`)},
		{"Fassung 2", bytes.Replace(gut, []byte(`"major": 1`), []byte(`"major": 2`), 1)},
		{"Fassung 0", bytes.Replace(gut, []byte(`"major": 1`), []byte(`"major": 0`), 1)},
		{"unbekanntes Feld im Rahmen",
			bytes.Replace(gut, []byte(`"timestamp": 1705228800`),
				[]byte(`"timestamp": 1705228800, "unbekannt": 1`), 1)},
		{"unbekanntes Feld in einem Login",
			bytes.Replace(gut, []byte(`"type": "basic-auth"`),
				[]byte(`"type": "basic-auth", "sonst": "was"`), 1)},
		{"Kennung mit dem falschen Alphabet",
			bytes.Replace(gut, []byte(`"DZSXp7iBQY-Fg-OofakQtQ"`),
				[]byte(`"DZSXp7iBQY+Fg/OofakQtQ"`), 1)},
		{"Kennung ist kein Base64",
			bytes.Replace(gut, []byte(`"DZSXp7iBQY-Fg-OofakQtQ"`),
				[]byte(`"nicht base64!!!"`), 1)},
		{"Kennung leer",
			bytes.Replace(gut, []byte(`"DZSXp7iBQY-Fg-OofakQtQ"`), []byte(`""`), 1)},
		{"Titel leer",
			bytes.Replace(gut, []byte(`"title": "GitHub Login"`),
				[]byte(`"title": "   "`), 1)},
		{"Art fehlt",
			bytes.Replace(gut, []byte(`"type": "note"`), []byte(`"x": "note"`), 1)},
		{"Art ist leer",
			bytes.Replace(gut, []byte(`"type": "note"`), []byte(`"type": ""`), 1)},
		{"Steuerzeichen im Titel",
			bytes.Replace(gut, []byte(`"title": "GitHub Login"`),
				[]byte(`"title": "Git\u0000Hub"`), 1)},
		{"Zugangsdaten sind kein Feld",
			bytes.Replace(gut, []byte(`"credentials": [`),
				[]byte(`"credentials": "nein", "x": [`), 1)},
	}
	for _, fall := range faelle {
		if _, err := classes.CxfParse(fall.data); err == nil {
			t.Errorf("%s: angenommen, erwartet war eine Absage", fall.name)
		}
	}
	// Und die gute Datei muss weiter durchgehen - sonst prueft das oben
	// nur, dass alles abgewiesen wird.
	if _, err := classes.CxfParse(gut); err != nil {
		t.Fatalf("die gute Datei wird abgewiesen: %v", err)
	}
}

// Eine Datei, die sich tausendfach in sich selbst schachtelt, muss
// anstossen - nicht im Stapelspeicher.
func TestCxfTiefeSchachtelungStoesstAn(t *testing.T) {
	var b strings.Builder
	b.WriteString(`{"version":{"major":1,"minor":0},"exporterRpId":"a",` +
		`"exporterDisplayName":"b","timestamp":1,"accounts":[{"id":"AAAA",` +
		`"username":"u","email":"e","items":[],"collections":[`)
	tiefe := classes.CxfMaxDepth + 50
	for i := 0; i < tiefe; i++ {
		b.WriteString(`{"id":"AAAA","title":"t","items":[],"subCollections":[`)
	}
	for i := 0; i < tiefe; i++ {
		b.WriteString(`]}`)
	}
	b.WriteString(`]}]}`)
	if _, err := classes.CxfParse([]byte(b.String())); err == nil {
		t.Fatal("eine Datei mit gut siebzig Ebenen wurde angenommen")
	}
}

// Eine Datei, die laenger ist als erlaubt, wird nicht erst eingelesen.
func TestCxfZuGrosseDateiWirdNichtGelesen(t *testing.T) {
	gross := bytes.NewReader(bytes.Repeat([]byte("a"), classes.CxfMaxFileLen+1024))
	if _, err := classes.CxfRead(gross); err == nil {
		t.Fatal("eine zu grosse Datei wurde angenommen")
	}
}

// --- Die Oberflaeche ----------------------------------------------------------

// Die Seite und die Schnittstelle. Der Punkt dieses Tests ist NICHT, dass
// die Seite laedt - sondern dass die Bedingung auch dort gilt, wo sie sich
// am leichtesten umgehen laesst: /api/export ohne Ziel und ohne Zustimmung.
//
// Eine Oberflaeche, die fragt, und eine Schnittstelle, die ohne Frage
// liefert, waeren zusammen keine Freigabe. Darum sitzt die Bedingung in
// CxfExport, und darum wird sie hier von aussen geprueft.
func TestExchangeSchnittstelleVerlangtDieFreigabe(t *testing.T) {
	app := newApp(t, startSimulator(t))
	server, err := classes.NewServer(app)
	if err != nil {
		t.Fatalf("Server: %v", app.Message(err))
	}
	defer server.Close()
	go server.Serve()

	base := server.Address()
	origin := strings.TrimSuffix(base, "/")
	client := &http.Client{Timeout: 30 * time.Second}

	// Die Seite muss es geben - sonst sind die Texte in den Sprachdateien
	// fuer nichts.
	answer, err := client.Get(base + "exchange")
	if err != nil {
		t.Fatalf("Seite: %v", err)
	}
	body, _ := io.ReadAll(answer.Body)
	answer.Body.Close()
	if answer.StatusCode != http.StatusOK {
		t.Fatalf("die Seite antwortet %d", answer.StatusCode)
	}
	// Der Warnsatz zum Weg hinaus muss auf der Seite stehen. Eine Anzeige,
	// die sagt WOHIN, ist der dritte Teil der Freigabe - und ohne diesen
	// Satz ist sie eine Eingabezeile ohne Hinweis.
	for _, satz := range []string{
		app.Text("cxf.export.warning"),
		app.Text("leak.explain"),
	} {
		if !bytes.Contains(body, []byte(satz)) {
			t.Errorf("dieser Satz steht nicht auf der Seite: %s", satz)
		}
	}

	apiCall(t, client, origin, http.MethodPost, base+"api/unlock",
		`{"pin":"`+simPin+`"}`)

	// Ohne Ziel, ohne Zustimmung, mit Zustimmung aber ohne Ziel, mit Ziel
	// aber ohne Zustimmung - alle vier muessen abgewiesen werden.
	for _, rumpf := range []string{
		`{}`,
		`{"confirm":true}`,
		`{"destination":"USB-Stick E:"}`,
		`{"destination":"   ","confirm":true}`,
	} {
		reply := apiCall(t, client, origin, http.MethodPost, base+"api/export",
			rumpf)
		if ok, _ := reply["ok"].(bool); ok {
			t.Errorf("%s wurde angenommen: %v", rumpf, reply)
		}
	}

	// Und mit allem: es kommt eine Datei UND ein Bericht.
	reply := apiCall(t, client, origin, http.MethodPost, base+"api/export",
		`{"destination":"USB-Stick E:","confirm":true}`)
	if ok, _ := reply["ok"].(bool); !ok {
		t.Fatalf("mit Freigabe abgewiesen: %v", reply)
	}
	data, _ := reply["data"].(map[string]any)
	datei, _ := data["file"].(string)
	if datei == "" {
		t.Fatal("es kam keine Datei")
	}
	if _, err := classes.CxfParse([]byte(datei)); err != nil {
		t.Fatalf("die Datei der Schnittstelle ist unlesbar: %v", err)
	}
	bericht, hat := data["report"].(map[string]any)
	if !hat {
		t.Fatal("es kam kein Bericht - die Oberflaeche koennte nicht sagen, " +
			"was nicht mitgekommen ist")
	}
	if bericht["destination"] != "USB-Stick E:" {
		t.Errorf("das Ziel im Bericht ist %v", bericht["destination"])
	}

	// Die Leak-Pruefung ohne Zustimmung: abgewiesen, und zwar von der
	// Schnittstelle, nicht von der Seite.
	for _, rumpf := range []string{`{"all":true}`, `{"all":true,"consent":false}`} {
		reply := apiCall(t, client, origin, http.MethodPost, base+"api/leaks",
			rumpf)
		if ok, _ := reply["ok"].(bool); ok {
			t.Errorf("Leak-Pruefung ohne Zustimmung angenommen: %v", reply)
		}
	}

	// Hereinholen in zwei Schritten: erst der Bericht, dann das Anlegen.
	gelesen := apiCall(t, client, origin, http.MethodPost, base+"api/import",
		`{"format":"csv","data":"`+
			base64.StdEncoding.EncodeToString([]byte(
				"name,url,password\nAus der Oberflaeche,https://a.example,geheim\n"))+
			`"}`)
	if ok, _ := gelesen["ok"].(bool); !ok {
		t.Fatalf("lesen abgewiesen: %v", gelesen)
	}
	gelesenData, _ := gelesen["data"].(map[string]any)
	if zahl, _ := gelesenData["count"].(float64); zahl != 1 {
		t.Errorf("count %v, erwartet 1", gelesenData["count"])
	}
	// Der erste Schritt darf NICHTS angelegt haben.
	vorher := apiCall(t, client, origin, http.MethodGet, base+"api/items", "")
	vorherListe, _ := vorher["data"].([]any)
	for _, eintrag := range vorherListe {
		if eintrag.(map[string]any)["name"] == "Aus der Oberflaeche" {
			t.Fatal("der erste Schritt hat schon angelegt")
		}
	}
}
