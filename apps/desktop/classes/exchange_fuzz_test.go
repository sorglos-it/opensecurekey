// Fuzzing fuer die drei Leser fremder Dateien.
//
// WARUM: ein Parser fuer eine fremde Datei ist ein Parser, den ein Angreifer
// erreicht. Er braucht nichts zu duerfen - nur eine Datei, die der Anwender
// fuer eine Sicherung haelt. Die Pruefungen in tests/ gehen die Faelle
// durch, an die jemand gedacht hat; diese hier gehen die durch, an die
// niemand gedacht hat.
//
// WAS ALS FEHLER ZAEHLT, und das ist hier der ganze Punkt: nicht "es kommt
// ein Fehler zurueck" - ein Fehler ist die richtige Antwort auf eine kaputte
// Datei. Als Fehler zaehlt:
//
//   - ein Absturz oder ein Zugriff ueber eine Grenze hinaus (das findet Go
//     selbst),
//   - ein Eintrag, der durchkommt und den das GERAET abweisen wuerde (dann
//     faellt es erst nach der Freigabe auf),
//   - ein Eintrag ohne Geheimnis oder mit einem zu langen,
//   - ein Geheimnis, das stillschweigend abgeschnitten wurde.
//
// Laufen: `go test ./classes/ -run Fuzz` geht die mitgelieferten Faelle
// durch und laeuft bei jedem Satz mit. Laenger suchen:
//
//	go test ./classes/ -fuzz FuzzCxfParse -fuzztime 2m
package classes

import (
	"bytes"
	"strings"
	"testing"
)

// pruefeKandidaten ist die gemeinsame Pruefung: was durchkommt, muss der
// Tresor annehmen koennen.
func pruefeKandidaten(t *testing.T, candidates []CxfCandidate) {
	t.Helper()
	for _, candidate := range candidates {
		if err := CheckItem(candidate.Meta); err != nil {
			t.Fatalf("ein Eintrag kam durch, den das Geraet abweist: %q (%v)",
				candidate.Meta.Name, err)
		}
		if len(candidate.Secret) == 0 {
			t.Fatalf("ein Eintrag ohne Geheimnis kam durch: %q",
				candidate.Meta.Name)
		}
		if len(candidate.Secret) > MaxSecretLen {
			t.Fatalf("ein Geheimnis von %d Byte kam durch, die Grenze ist %d",
				len(candidate.Secret), MaxSecretLen)
		}
		if len(candidate.Meta.Name) > MaxNameLen {
			t.Fatalf("ein Name von %d Byte kam durch, die Grenze ist %d",
				len(candidate.Meta.Name), MaxNameLen)
		}
		if len(candidate.Meta.Site) > MaxSiteLen {
			t.Fatalf("eine Adresse von %d Byte kam durch, die Grenze ist %d",
				len(candidate.Meta.Site), MaxSiteLen)
		}
	}
}

func FuzzCxfParse(f *testing.F) {
	f.Add([]byte(`{"version":{"major":1,"minor":0},"exporterRpId":"a",` +
		`"exporterDisplayName":"b","timestamp":1,"accounts":[{"id":"AAAA",` +
		`"username":"u","email":"e","collections":[],"items":[{"id":"AQ",` +
		`"title":"t","credentials":[{"type":"basic-auth","password":` +
		`{"fieldType":"concealed-string","value":"geheim"}}]}]}]}`))
	f.Add([]byte(`{}`))
	f.Add([]byte(`{"version":{"major":1,"minor":0},"accounts":[]}`))
	f.Add([]byte(`[]`))
	f.Add([]byte("\x00\x01\x02"))
	f.Add([]byte(`{"version":{"major":1,"minor":0},"exporterRpId":"a",` +
		`"exporterDisplayName":"b","timestamp":1,"accounts":[{"id":"AAAA",` +
		`"username":"u","email":"e","collections":[],"items":[{"id":"AQ",` +
		`"title":"t","credentials":[{"type":"totp","secret":"JBSWY3DP",` +
		`"period":0,"digits":0,"algorithm":""}]}]}]}`))
	f.Fuzz(func(t *testing.T, data []byte) {
		header, err := CxfParse(data)
		if err != nil {
			return
		}
		// Was gelesen wurde, muss sich auch wieder schreiben und lesen
		// lassen. Eine Datei, die hinein darf und nicht hinaus, waere
		// eine Einbahnstrasse im Format.
		var out bytes.Buffer
		if err := CxfWrite(header, &out); err != nil {
			t.Fatalf("gelesen, aber nicht schreibbar: %v", err)
		}
		if _, err := CxfParse(out.Bytes()); err != nil {
			t.Fatalf("das eigene Geschriebene ist unlesbar: %v", err)
		}
		plan, err := CxfImport(header, 0)
		if err != nil {
			return
		}
		defer plan.Wipe()
		pruefeKandidaten(t, plan.Candidates)
		// Jede zurueckgebliebene Zeile braucht einen Grund - sonst
		// steht im Bericht eine Zeile, die niemand erklaeren kann.
		for _, line := range plan.Left {
			if line.ReasonKey == "" {
				t.Fatal("eine zurueckgebliebene Zeile ohne Grund")
			}
		}
	})
}

func FuzzCsvImport(f *testing.F) {
	f.Add([]byte("name,url,username,password,note\nA,https://a.example,anna," +
		"geheim,notiz\n"))
	f.Add([]byte("password\ngeheim\n"))
	f.Add([]byte("name,password\n"))
	f.Add([]byte(""))
	f.Add([]byte("\ufeffname,password\nA,geheim\n"))
	f.Add([]byte("name,password\n\"offen,geheim\n"))
	f.Add([]byte("name,url,password,login_totp\nA,b,geheim,JBSWY3DPEHPK3PXP\n"))
	f.Add([]byte("password,password,password\na,b,c\n"))
	f.Fuzz(func(t *testing.T, data []byte) {
		plan, err := CsvImport(bytes.NewReader(data), 0)
		if err != nil {
			return
		}
		defer plan.Wipe()
		pruefeKandidaten(t, plan.Candidates)
	})
}

// Das Kennwort geht mit in die Eingabe: ein Leser, der nur mit dem richtigen
// Kennwort geprueft wird, ist genau an der Stelle ungeprueft, an der die
// meisten Dateien ankommen.
//
// Die Eingabe wird am ersten Nullbyte geteilt. Das ist eine Eigenheit dieses
// Fuzzers und keine des Formats; sie steht hier, damit sie nicht fuer eine
// gehalten wird.
func FuzzKdbxImport(f *testing.F) {
	kopf := func(major uint32) []byte {
		out := []byte{0x03, 0xd9, 0xa2, 0x9a, 0x67, 0xfb, 0x4b, 0xb5}
		out = append(out, byte(0), byte(0), byte(major), byte(0))
		return out
	}
	f.Add(append(kopf(4), 0x00, 'a'))
	f.Add(append(kopf(3), 0x00, 'a'))
	f.Add([]byte{})
	f.Add([]byte("keine kdbx-datei\x00kennwort"))
	// Ein Dateikopf, der ein Feld mit einer absurden Laenge nennt.
	f.Add(append(append(kopf(4), 2, 0xff, 0xff, 0xff, 0x7f), 0x00, 'a'))
	// Ein Woerterbuch mit Argon2-Parametern, die niemand rechnen kann.
	f.Add(append(append(kopf(4), 11, 10, 0, 0, 0, 0, 1, 0x05, 1, 0, 0, 0, 'M',
		8, 0, 0, 0), 0x00, 'a'))
	f.Fuzz(func(t *testing.T, data []byte) {
		kennwort := []byte("a")
		if cut := bytes.IndexByte(data, 0); cut >= 0 {
			kennwort = data[cut+1:]
			data = data[:cut]
		}
		if len(kennwort) > 256 {
			kennwort = kennwort[:256]
		}
		plan, err := KdbxImport(bytes.NewReader(data),
			append([]byte(nil), kennwort...), 0)
		if err != nil {
			return
		}
		defer plan.Wipe()
		pruefeKandidaten(t, plan.Candidates)
	})
}

// Das XML des Behaelters hat seinen eigenen Fuzzer: es ist der Teil, der am
// weitesten vom Dateikopf entfernt liegt und am meisten Formen hat. Der Weg
// dorthin fuehrt sonst durch Argon2, und das macht jeden Lauf langsam -
// hier wird darum direkt der Leser gefuettert.
func FuzzKdbxXML(f *testing.F) {
	f.Add([]byte(`<KeePassFile><Root><Group><Name>G</Name><Entry>` +
		`<String><Key>Title</Key><Value>T</Value></String>` +
		`<String><Key>Password</Key><Value Protected="True">AAAA</Value></String>` +
		`</Entry></Group></Root></KeePassFile>`))
	f.Add([]byte(`<KeePassFile><Root><Group><Entry><History><Entry>` +
		`<String><Key>Password</Key><Value Protected="True">AA</Value></String>` +
		`</Entry></History></Entry></Group></Root></KeePassFile>`))
	f.Add([]byte(`<KeePassFile>`))
	f.Add([]byte(`</Entry>`))
	f.Add([]byte(strings.Repeat("<Group>", 200)))
	f.Add([]byte(`<KeePassFile><Meta><RecycleBinUUID>AAAA</RecycleBinUUID>` +
		`</Meta><Root><Group><UUID>AAAA</UUID><Entry><String><Key>Password` +
		`</Key><Value>x</Value></String></Entry></Group></Root></KeePassFile>`))
	f.Fuzz(func(t *testing.T, data []byte) {
		// Ohne Verdecken: dann braucht es keinen Schluessel, und der
		// Fuzzer kommt mit jeder Eingabe bis in den Leser.
		head := &kdbxHead{major: 4, innerID: kdbxStreamNone}
		plan, err := kdbxReadXML(head, data, 0)
		if err != nil {
			return
		}
		defer plan.Wipe()
		pruefeKandidaten(t, plan.Candidates)
	})
}
