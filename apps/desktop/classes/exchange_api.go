// Die Schnittstelle der Oberflaeche fuer die Wege hinein und hinaus.
//
// DIE ANZEIGE, DIE SAGT WOHIN, steht in views/exchange.html - aber die
// BEDINGUNG steht nicht dort. Sie sitzt in CxfExport (exchange_out.go), und
// das ist Absicht: eine Oberflaeche, die man umgehen kann, indem man die
// Anfrage selbst schickt, ist keine Bedingung. Wer /api/export ohne
// destination und confirm aufruft, bekommt error.cxf.noconsent - genauso wie
// die Kommandozeile und genauso wie ein Aufruf aus dem Quelltext.
//
// Was hier NICHT liegt: ein Weg, der eine Datei ohne Nachfrage schreibt, und
// ein Weg, der eine Datei vom Datentraeger liest, ohne dass der Anwender sie
// ausgewaehlt hat. Die Datei kommt immer im Rumpf der Anfrage.
package classes

import (
	"bytes"
	"encoding/base64"
	"net/http"
)

// apiExchangeLimit ist der groesste Rumpf fuer eine Importdatei. Eine
// KeePass-Datei mit Anhaengen wird gross; die Grenze ist dieselbe wie fuer
// die Firmware.
const apiExchangeLimit = 64 << 20

// apiExportRequest ist die Anfrage zum Herausschreiben.
type apiExportRequest struct {
	// Destination ist das Ziel in Worten des Anwenders. Die Oberflaeche
	// zeigt es, bevor sie fragt; dieser Wert ist, was angezeigt wurde.
	Destination string `json:"destination"`
	// Confirm ist das Ja, nachdem das Ziel angezeigt wurde.
	Confirm bool  `json:"confirm"`
	Profile uint8 `json:"profile"`
}

// apiExportReply ist die Antwort: die Datei und der Bericht.
type apiExportReply struct {
	// File ist die fertige Datei als Text. Sie steht im RUMPF der
	// Antwort, nicht in einer Adresse - ein Chiffrat in einer Adresse
	// landete im Protokoll jedes Vermittlers, und hier ist es nicht
	// einmal ein Chiffrat.
	File string `json:"file"`
	// Report sagt, was mitgekommen ist und was nicht. Er steht
	// GLEICHRANGIG neben der Datei, nicht als Nebensache: die
	// Oberflaeche muss ihn anzeigen koennen, bevor der Anwender die
	// Datei speichert.
	Report CxfReport `json:"report"`
}

func (s *Server) apiExport(r *http.Request) (any, error) {
	var request apiExportRequest
	if err := readJSON(r, &request, 1<<16); err != nil {
		return nil, err
	}
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	header, report, err := CxfExport(client, request.Profile, CxfConsent{
		Destination: request.Destination,
		Confirmed:   request.Confirm,
	})
	if err != nil {
		return nil, err
	}
	var out bytes.Buffer
	if err := CxfWrite(header, &out); err != nil {
		return nil, err
	}
	return apiExportReply{File: out.String(), Report: report}, nil
}

// apiImportRequest ist die Anfrage zum Hereinholen.
type apiImportRequest struct {
	// Format ist cxf, csv oder keepass. Es wird genannt und nicht aus der
	// Datei geraten: wer die beiden verwechselt, soll es hier merken.
	Format string `json:"format"`
	// Data ist die Datei, Base64. Sie geht durch den Rumpf.
	Data string `json:"data"`
	// Password ist das Kennwort einer KeePass-Datei. Es geht nur in den
	// Speicher dieses Aufrufs und wird danach ueberschrieben.
	Password string `json:"password"`
	Profile  uint8  `json:"profile"`
	// Apply unterscheidet die zwei Schritte: ohne Apply wird nur gelesen
	// und berichtet, mit Apply wird angelegt. Die Oberflaeche zeigt
	// zwischen beiden, was nicht mitkommt - und dieser Bericht kann die
	// Antwort aendern.
	Apply bool `json:"apply"`
}

// apiImportReply ist die Antwort auf das Lesen.
type apiImportReply struct {
	From string `json:"from"`
	// Count ist, wie viele Eintraege angelegt werden koennten.
	Count    int       `json:"count"`
	Taken    []CxfLine `json:"taken"`
	Left     []CxfLine `json:"left"`
	Warnings []CxfLine `json:"warnings,omitempty"`
	// Report steht nur beim zweiten Schritt: was wirklich im Geraet
	// liegt.
	Report *CxfReport `json:"report,omitempty"`
}

func (s *Server) apiImport(r *http.Request) (any, error) {
	var request apiImportRequest
	if err := readJSON(r, &request, apiExchangeLimit); err != nil {
		return nil, err
	}
	data, err := base64.StdEncoding.DecodeString(request.Data)
	if err != nil {
		return nil, Fail("error.request.format")
	}
	defer Wipe(data)
	password := []byte(request.Password)
	request.Password = ""
	defer Wipe(password)

	plan, err := apiImportPlan(request, data, password)
	if err != nil {
		return nil, err
	}
	defer plan.Wipe()

	reply := apiImportReply{
		From:     plan.From,
		Count:    len(plan.Candidates),
		Taken:    plan.Taken,
		Left:     plan.Left,
		Warnings: plan.Warnings,
	}
	if !request.Apply {
		return reply, nil
	}
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	report, err := CxfApply(client, plan)
	if err != nil {
		return nil, err
	}
	reply.Report = &report
	return reply, nil
}

// apiImportPlan liest die Datei im genannten Format.
func apiImportPlan(request apiImportRequest, data, password []byte) (*CxfPlan, error) {
	switch request.Format {
	case "cxf":
		header, err := CxfParse(data)
		if err != nil {
			return nil, err
		}
		return CxfImport(header, request.Profile)
	case "csv":
		flat, err := CsvImport(bytes.NewReader(data), request.Profile)
		if err != nil {
			return nil, err
		}
		return flatPlan(flat, request.Format), nil
	case "keepass":
		flat, err := KdbxImport(bytes.NewReader(data),
			append([]byte(nil), password...), request.Profile)
		if err != nil {
			return nil, err
		}
		return flatPlan(flat, request.Format), nil
	}
	return nil, Fail("error.request.format")
}

// apiLeaksRequest ist die Anfrage zur Leak-Pruefung.
type apiLeaksRequest struct {
	// Consent ist die Zustimmung, dass eine Anfrage nach draussen geht.
	// Ohne sie geht keine - und zwar nicht, weil die Oberflaeche es
	// verhindert, sondern weil LeakChecker.Check es tut.
	Consent bool `json:"consent"`
	// Password ist ein einzelnes Kennwort. Leer zusammen mit All: der
	// ganze Tresor.
	Password string `json:"password"`
	All      bool   `json:"all"`
	Profile  uint8  `json:"profile"`
}

// apiLeaksReply ist die Antwort.
type apiLeaksReply struct {
	// Sent ist, was wirklich hinausgegangen ist. Es steht hier, damit die
	// Oberflaeche es ZEIGEN kann: nicht die Zusage, sondern die Zeichen.
	Sent      string        `json:"sent,omitempty"`
	GroupSize int           `json:"groupSize,omitempty"`
	Count     uint64        `json:"count,omitempty"`
	Findings  []LeakFinding `json:"findings,omitempty"`
}

func (s *Server) apiLeaks(r *http.Request) (any, error) {
	var request apiLeaksRequest
	if err := readJSON(r, &request, 1<<16); err != nil {
		return nil, err
	}
	password := []byte(request.Password)
	request.Password = ""
	defer Wipe(password)
	consent := LeakConsent{Allowed: request.Consent}
	checker := &LeakChecker{}
	if request.All {
		client, err := s.app.Client()
		if err != nil {
			return nil, err
		}
		findings, err := LeakScan(client, checker, request.Profile, consent)
		if err != nil {
			return nil, err
		}
		return apiLeaksReply{Findings: findings}, nil
	}
	result, err := checker.Check(append([]byte(nil), password...), consent)
	if err != nil {
		return nil, err
	}
	return apiLeaksReply{Sent: result.Sent, GroupSize: result.GroupSize,
		Count: result.Count}, nil
}
