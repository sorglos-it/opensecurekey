package classes

import "testing"

func TestCheckItem(t *testing.T) {
	good := Item{Type: ItemLogin, Name: "Beispiel", Site: "example.org"}
	if err := CheckItem(good); err != nil {
		t.Fatalf("guter Eintrag abgelehnt: %v", err)
	}
	bad := []Item{
		{Type: 0, Name: "x"},
		{Type: 99, Name: "x"},
		{Type: ItemLogin, Name: ""},
		{Type: ItemLogin, Name: "mit\nUmbruch"},
		{Type: ItemLogin, Name: "x", Site: "mit\tTab"},
	}
	for i, item := range bad {
		if err := CheckItem(item); err == nil {
			t.Errorf("Fall %d wurde angenommen", i)
		}
	}
}

func TestWipe(t *testing.T) {
	buf := []byte("geheim")
	Wipe(buf)
	for _, b := range buf {
		if b != 0 {
			t.Fatal("Puffer nicht geloescht")
		}
	}
}

// Alle Typen des Headers brauchen einen Sprachschluessel.
func TestItemTypesHaveKeys(t *testing.T) {
	for _, item := range ItemTypes() {
		if !item.Valid() || item.Key() == "item.type.unknown" {
			t.Errorf("Typ %d ohne Schluessel", item)
		}
	}
}
