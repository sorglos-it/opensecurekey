package classes

import (
	"bytes"
	"testing"
)

func TestCborRoundTrip(t *testing.T) {
	in := CborMap{
		"v":    uint64(1),
		"text": "Grüße",
		"bin":  []byte{0, 1, 255},
		"list": []any{uint64(1), int64(-2), true, nil},
		"sub":  CborMap{"a": "b"},
	}
	data, err := CborEncode(in)
	if err != nil {
		t.Fatalf("encode: %v", err)
	}
	out, err := CborDecode(data)
	if err != nil {
		t.Fatalf("decode: %v", err)
	}
	got, ok := out.(CborMap)
	if !ok {
		t.Fatalf("Abbildung erwartet, %T bekommen", out)
	}
	if got["text"] != "Grüße" {
		t.Errorf("Text verloren: %v", got["text"])
	}
	if !bytes.Equal(got["bin"].([]byte), []byte{0, 1, 255}) {
		t.Errorf("Bytes verloren: %v", got["bin"])
	}
}

// Gleiche Eingabe muss immer gleiche Bytes geben - sonst waere die Reihenfolge
// der Schluessel zufaellig.
func TestCborStableOrder(t *testing.T) {
	first, _ := CborEncode(CborMap{"b": uint64(2), "a": uint64(1)})
	second, _ := CborEncode(CborMap{"a": uint64(1), "b": uint64(2)})
	if !bytes.Equal(first, second) {
		t.Error("Reihenfolge der Schluessel ist nicht fest")
	}
}

func TestCborRejectsBadInput(t *testing.T) {
	cases := map[string][]byte{
		"leer":                {},
		"rest dahinter":       {0x01, 0x02},
		"zu lange Schreibung": {0x18, 0x01},
		"unbestimmte Laenge":  {0x5f, 0xff},
		"abgeschnitten":       {0x62, 0x41},
		"Gleitkomma":          {0xfb, 0, 0, 0, 0, 0, 0, 0, 0},
		"Zahl als Schluessel": {0xa1, 0x01, 0x01},
	}
	for name, data := range cases {
		if _, err := CborDecode(data); err == nil {
			t.Errorf("%s wurde angenommen", name)
		}
	}
}

func TestCborDoubleKey(t *testing.T) {
	// {"a": 1, "a": 2}
	data := []byte{0xa2, 0x61, 'a', 0x01, 0x61, 'a', 0x02}
	if _, err := CborDecode(data); err == nil {
		t.Error("doppelter Schluessel wurde angenommen")
	}
}

func TestCborDepthLimit(t *testing.T) {
	value := any(uint64(1))
	for i := 0; i < CborMaxDepth+3; i++ {
		value = []any{value}
	}
	if _, err := CborEncode(value); err == nil {
		t.Error("zu tiefe Verschachtelung wurde angenommen")
	}
}
