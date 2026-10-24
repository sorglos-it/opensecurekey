// Der neue Weg (oeffentlicher Teil aus der Liste) und der alte Weg
// (Schluessel per reveal geholt) muessen genau dieselben Merkmale ergeben.
//
// Das ist die Gegenprobe zur Messung in tests/freigaben_pkcs11_test.go: die
// zaehlt nur noch Freigaben. Ob dabei auch das Richtige herauskommt, steht
// hier. Liefen die beiden Wege auseinander, wuerde Firefox nach einem
// Update desselben Programms einen anderen Schluessel sehen - und niemand
// wuesste, welcher der echte ist.
package classes

import (
	"bytes"
	"testing"
)

func TestPkcs11PubWegGleichtRevealWeg(t *testing.T) {
	saetze := []struct {
		name    string
		private string
	}{
		{"ed25519", goldenEd25519Private},
		{"ecdsa-p256", goldenEcdsaP256Private},
		{"rsa-2048", goldenRsa2048Private},
	}
	for _, satz := range saetze {
		t.Run(satz.name, func(t *testing.T) {
			key, err := ParseSSHKey([]byte(satz.private))
			if err != nil {
				t.Fatalf("Schluessel lesen: %v", err)
			}
			defer key.Wipe()
			item := Item{ID: 7, Type: ItemSSHKey, Name: "Pruefung",
				Pub: append([]byte(nil), key.Blob...)}

			handleA := uint64(1)
			ueberPub := publicOnlyObjects(item, &handleA)
			handleB := uint64(1)
			ueberReveal := keyObjects(item, []byte(satz.private), &handleB)

			if len(ueberPub) != 2 || len(ueberReveal) != 2 {
				t.Fatalf("%d Objekte ueber pub, %d ueber reveal, erwartet je 2",
					len(ueberPub), len(ueberReveal))
			}
			for i := range ueberPub {
				vergleicheObjekte(t, ueberPub[i], ueberReveal[i])
			}
		})
	}
}

// vergleicheObjekte prueft zwei Objekte Merkmal fuer Merkmal.
func vergleicheObjekte(t *testing.T, a, b pkcs11Object) {
	t.Helper()
	if a.handle != b.handle || a.class != b.class ||
		a.itemID != b.itemID || a.signKind != b.signKind {
		t.Errorf("Objekte unterscheiden sich im Rahmen: %+v gegen %+v",
			a.handle, b.handle)
	}
	if len(a.attributes) != len(b.attributes) {
		t.Fatalf("%d Merkmale ueber pub, %d ueber reveal",
			len(a.attributes), len(b.attributes))
	}
	for kind, wert := range a.attributes {
		andere, found := b.attributes[kind]
		if !found {
			t.Errorf("Merkmal %#x fehlt auf dem reveal-Weg", kind)
			continue
		}
		if wert.Kind != andere.Kind || wert.Bool != andere.Bool ||
			wert.Ulong != andere.Ulong || !bytes.Equal(wert.Bytes, andere.Bytes) {
			t.Errorf("Merkmal %#x weicht ab", kind)
		}
	}
}

// Ein Eintrag, dessen oeffentlicher Teil unbrauchbar ist, ergibt kein
// Objekt - lieber keines als eines mit geratenen Merkmalen.
func TestPkcs11PubWegWeistKaputtesAb(t *testing.T) {
	handle := uint64(1)
	item := Item{ID: 7, Type: ItemSSHKey, Name: "Pruefung",
		Pub: []byte("kein Schluessel")}
	if objects := publicOnlyObjects(item, &handle); objects != nil {
		t.Errorf("%d Objekte aus unbrauchbaren Bytes, erwartet keines",
			len(objects))
	}
	if handle != 1 {
		t.Errorf("Zaehler der Objektnummern wurde verbraucht: %d", handle)
	}
}
