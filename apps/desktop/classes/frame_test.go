package classes

import (
	"testing"
	"time"
)

// loopback gibt zurueck, was hineingeschrieben wurde.
type loopback struct {
	reports [][]byte
}

func (l *loopback) WriteReport(report []byte) error {
	copied := make([]byte, len(report))
	copy(copied, report)
	l.reports = append(l.reports, copied)
	return nil
}

func (l *loopback) ReadReport(timeout time.Duration) ([]byte, error) {
	if len(l.reports) == 0 {
		return nil, Fail("error.hid.timeout")
	}
	report := l.reports[0]
	l.reports = l.reports[1:]
	return report, nil
}

func (l *loopback) Close() error     { return nil }
func (l *loopback) Describe() string { return "loopback" }

func TestFramesRoundTrip(t *testing.T) {
	for _, size := range []int{1, 60, 61, 62, 500, 4096} {
		payload := make([]byte, size)
		for i := range payload {
			payload[i] = byte(i)
		}
		reports, err := SplitFrames(payload)
		if err != nil {
			t.Fatalf("%d Bytes: %v", size, err)
		}
		link := &loopback{}
		for _, report := range reports {
			if len(report) != ReportSize {
				t.Fatalf("Bericht ist %d Bytes lang", len(report))
			}
			link.WriteReport(report)
		}
		back, err := JoinFrames(link, time.Second)
		if err != nil {
			t.Fatalf("%d Bytes zurueck: %v", size, err)
		}
		if len(back) != size {
			t.Fatalf("%d Bytes hin, %d zurueck", size, len(back))
		}
	}
}

func TestFramesRejectWrongStart(t *testing.T) {
	link := &loopback{}
	report := make([]byte, ReportSize)
	report[0] = frameLast // kein Anfangsbericht
	report[2] = 1
	link.WriteReport(report)
	if _, err := JoinFrames(link, time.Second); err == nil {
		t.Error("Bericht ohne Anfang wurde angenommen")
	}
}
