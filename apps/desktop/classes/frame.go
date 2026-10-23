// Rahmen: eine CBOR-Nachricht wird in 64-Byte-Berichte zerlegt und wieder
// zusammengesetzt. shared/protocol.md nennt die Berichtsgroesse, aber noch
// keine Kopfbytes - die hier verwendeten sind unser Vorschlag und in
// docs/desktop.md als solcher gekennzeichnet.
package classes

import "time"

// Groessen des Rahmens.
const (
	ReportSize   = 64
	frameHeader  = 3
	framePayload = ReportSize - frameHeader
	frameFirst   = 0x01
	frameLast    = 0x02
	maxReports   = CborMaxBytes/framePayload + 2
	readTimeout  = 30 * time.Second
	shortTimeout = 3 * time.Second
)

// Transport ist alles, was 64-Byte-Berichte austauscht: echtes HID oder die
// Attrappe aus tests/.
type Transport interface {
	WriteReport(report []byte) error
	ReadReport(timeout time.Duration) ([]byte, error)
	Close() error
	Describe() string
}

// SplitFrames zerlegt eine Nachricht in Berichte voller Laenge.
func SplitFrames(payload []byte) ([][]byte, error) {
	if len(payload) == 0 || len(payload) > CborMaxBytes {
		return nil, Fail("error.frame.size")
	}
	var out [][]byte
	for offset := 0; offset < len(payload); offset += framePayload {
		end := offset + framePayload
		if end > len(payload) {
			end = len(payload)
		}
		part := payload[offset:end]
		report := make([]byte, ReportSize)
		var flags byte
		if offset == 0 {
			flags |= frameFirst
		}
		if end == len(payload) {
			flags |= frameLast
		}
		report[0] = flags
		report[1] = byte(len(part) >> 8)
		report[2] = byte(len(part))
		copy(report[frameHeader:], part)
		out = append(out, report)
	}
	return out, nil
}

// JoinFrames liest Berichte, bis der letzte kommt, und gibt die Nachricht zurueck.
func JoinFrames(t Transport, timeout time.Duration) ([]byte, error) {
	var payload []byte
	for count := 0; count < maxReports; count++ {
		report, err := t.ReadReport(timeout)
		if err != nil {
			return nil, err
		}
		if len(report) != ReportSize {
			return nil, Fail("error.frame.size")
		}
		part, last, err := parseReport(report, count == 0)
		if err != nil {
			return nil, err
		}
		if len(payload)+len(part) > CborMaxBytes {
			return nil, Fail("error.frame.size")
		}
		payload = append(payload, part...)
		if last {
			if len(payload) == 0 {
				return nil, Fail("error.frame.size")
			}
			return payload, nil
		}
	}
	return nil, Fail("error.frame.size")
}

func parseReport(report []byte, wantFirst bool) ([]byte, bool, error) {
	flags := report[0]
	if flags&^(frameFirst|frameLast) != 0 {
		return nil, false, Fail("error.frame.format")
	}
	if wantFirst != (flags&frameFirst != 0) {
		return nil, false, Fail("error.frame.format")
	}
	size := int(report[1])<<8 | int(report[2])
	if size == 0 || size > framePayload {
		return nil, false, Fail("error.frame.format")
	}
	return report[frameHeader : frameHeader+size], flags&frameLast != 0, nil
}
