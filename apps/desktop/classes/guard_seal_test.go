// Pruefungen der zweiten Stufe: ohne Stick bleibt die Datei unlesbar.
package classes

import (
	"bytes"
	"errors"
	"testing"
)

func TestSealRoundTrip(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	plain := []byte("die Tabelle, ohne die das Programm nichts kann")
	blob, err := guard.Seal("datenmodell", []byte("v1"), plain)
	if err != nil {
		t.Fatalf("verschliessen: %v", err)
	}
	if bytes.Contains(blob, plain) {
		t.Fatal("der Klartext steht noch in der verschlossenen Datei")
	}
	back, err := guard.Unseal("datenmodell", []byte("v1"), blob)
	if err != nil {
		t.Fatalf("oeffnen: %v", err)
	}
	if !bytes.Equal(back, plain) {
		t.Fatal("heraus kam etwas anderes als hinein ging")
	}
}

// Der Kern der zweiten Stufe: ein anderer Stick macht die Datei nicht auf.
// Das ist kein Nein, das ein Programm uebergehen koennte - der Schluessel
// existiert ohne diesen Stick einfach nicht.
func TestSealStaysClosedWithAnotherDevice(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	plain := []byte("ohne Stick unlesbar")
	blob, err := guard.Seal("geheim", nil, plain)
	if err != nil {
		t.Fatalf("verschliessen: %v", err)
	}

	stranger, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("fremder stick")})
	if err != nil {
		t.Fatalf("zweite Attrappe: %v", err)
	}
	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = stranger.PublicKey()
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("egal"))
	other, err := NewGuard(stranger.GuardFakeClient("test"), cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	_, err = other.Unseal("geheim", nil, blob)
	if err == nil {
		t.Fatal("ein fremder Stick hat die Datei geoeffnet")
	}
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.guard.sealbroken" {
		t.Fatalf("falsche Ursache: %v", err)
	}
}

// Ganz ohne Geraet: die Verbindung ist weg, es gibt keinen Schluessel, und
// die Datei bleibt zu. Der Fehler sagt "kein Geraet" - nicht "kaputt".
func TestSealStaysClosedWithoutAnyDevice(t *testing.T) {
	_, guard, unplug := guardTestDevice(t, GuardFakeOptions{})
	blob, err := guard.Seal("geheim", nil, []byte("Inhalt"))
	if err != nil {
		t.Fatalf("verschliessen: %v", err)
	}
	unplug()
	if _, err := guard.Unseal("geheim", nil, blob); err == nil {
		t.Fatal("ohne Geraet aufgegangen")
	}
}

// Jede verschlossene Datei hat ihr eigenes Salz: zweimal dasselbe
// verschliessen ergibt nie dieselben Bytes. Sonst liesse sich aus zwei
// Dateien etwas ueber den Schluessel schliessen.
func TestSealUsesFreshSaltAndNonce(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	plain := []byte("immer derselbe Inhalt")
	seen := map[string]bool{}
	for runde := 0; runde < 10; runde++ {
		blob, err := guard.Seal("gleich", nil, plain)
		if err != nil {
			t.Fatalf("Runde %d: %v", runde, err)
		}
		if seen[string(blob)] {
			t.Fatalf("Runde %d: dieselben Bytes zum zweiten Mal", runde)
		}
		seen[string(blob)] = true
	}
}

// Die Bezeichnung ist mitgerechnet: unter einem anderen Namen geht die
// Datei nicht auf, obwohl derselbe Stick steckt.
func TestSealLabelIsBound(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	blob, err := guard.Seal("richtig", nil, []byte("Inhalt"))
	if err != nil {
		t.Fatalf("verschliessen: %v", err)
	}
	if _, err := guard.Unseal("anders", nil, blob); err == nil {
		t.Fatal("unter einer anderen Bezeichnung aufgegangen")
	}
}

// Der Zusatz ist mitgerechnet: andere Programmversion, andere Antwort.
func TestSealExtraIsBound(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	blob, err := guard.Seal("datei", []byte("version-1"), []byte("Inhalt"))
	if err != nil {
		t.Fatalf("verschliessen: %v", err)
	}
	if _, err := guard.Unseal("datei", []byte("version-2"), blob); err == nil {
		t.Fatal("mit dem falschen Zusatz aufgegangen")
	}
}

// Ein veraendertes Byte faellt auf - jedes einzelne.
func TestSealNoticesEveryChangedByte(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	blob, err := guard.Seal("datei", nil, []byte("ein Inhalt mittlerer Laenge"))
	if err != nil {
		t.Fatalf("verschliessen: %v", err)
	}
	for at := range blob {
		bent := make([]byte, len(blob))
		copy(bent, blob)
		bent[at] ^= 0x01
		if _, err := guard.Unseal("datei", nil, bent); err == nil {
			t.Fatalf("Byte %d veraendert und trotzdem aufgegangen", at)
		}
	}
}

// Ein kaputter Kopf ist ein Formatfehler, kein falscher Schluessel. Wer die
// beiden verwechselt, schickt den Anwender an die falsche Stelle.
func TestSealTellsFormatFromWrongKey(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	cases := []struct {
		name string
		blob []byte
		want string
	}{
		{"zu kurz", []byte("kurz"), "error.guard.sealformat"},
		{"falsche Kennung", append([]byte("NICHTOSK"),
			make([]byte, sealHeaderLen+sealTagLen)...), "error.guard.sealformat"},
	}
	for _, one := range cases {
		t.Run(one.name, func(t *testing.T) {
			_, err := guard.Unseal("datei", nil, one.blob)
			var problem *Problem
			if !errors.As(err, &problem) || problem.Key != one.want {
				t.Fatalf("erwartet %s, bekommen %v", one.want, err)
			}
		})
	}
	// Eine Fassung, die wir noch nicht kennen, wird als solche gemeldet.
	blob, err := guard.Seal("datei", nil, []byte("Inhalt"))
	if err != nil {
		t.Fatalf("verschliessen: %v", err)
	}
	blob[len(sealMagic)] = sealVersion + 1
	_, err = guard.Unseal("datei", nil, blob)
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.guard.sealversion" {
		t.Fatalf("erwartet error.guard.sealversion, bekommen %v", err)
	}
}

// Gleiche Bezeichnung, gleicher Schluessel aus dem Chip - sonst liesse sich
// eine Datei nach dem Neustart des Programms nicht mehr oeffnen.
func TestDeriveKeyIsStableAndLabelBound(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	first, err := guard.DeriveKey("a")
	if err != nil {
		t.Fatalf("ableiten: %v", err)
	}
	again, err := guard.DeriveKey("a")
	if err != nil {
		t.Fatalf("ableiten: %v", err)
	}
	if !bytes.Equal(first, again) {
		t.Fatal("dieselbe Bezeichnung ergab zwei verschiedene Schluessel")
	}
	other, err := guard.DeriveKey("b")
	if err != nil {
		t.Fatalf("ableiten: %v", err)
	}
	if bytes.Equal(first, other) {
		t.Fatal("zwei Bezeichnungen ergaben denselben Schluessel")
	}
	if len(first) != SealKeyLen {
		t.Fatalf("Schluessellaenge %d statt %d", len(first), SealKeyLen)
	}
}

func TestDeriveKeyRejectsBadLabels(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	for _, label := range []string{"", "mit\nZeilenumbruch", string(make([]byte, 65))} {
		if _, err := guard.DeriveKey(label); err == nil {
			t.Fatalf("Bezeichnung %q wurde angenommen", label)
		}
	}
}

func TestSealRejectsEmptyAndHuge(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	if _, err := guard.Seal("datei", nil, nil); err == nil {
		t.Fatal("leerer Inhalt wurde verschlossen")
	}
	if _, err := guard.Seal("datei", nil, make([]byte, sealMaxPlain+1)); err == nil {
		t.Fatal("zu grosser Inhalt wurde verschlossen")
	}
}
