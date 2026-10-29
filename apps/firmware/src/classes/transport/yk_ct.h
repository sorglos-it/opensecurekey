/* Vergleiche mit gleicher Laufzeit.
 *
 * Warum das hier nicht als Zugabe steht: 2024 war jeder YubiKey 5 vor
 * Fassung 5.7 klonbar, weil eine Inversion im Infineon-Baustein ihre
 * Laufzeit von den Daten abhaengig machte (EUCLEAK). Die Fehlerklasse ist
 * nicht "ein Vergleich ist langsam", sondern "ein Vergleich verraet, an
 * welcher Stelle er abgebrochen ist".
 *
 * memcmp() ist dafuer unbrauchbar: es bricht beim ersten Unterschied ab
 * und darf vom Uebersetzer durch eine wortweise Fassung ersetzt werden.
 *
 * Diese Hilfe liegt im Transportordner, weil Strang E sie braucht und es
 * im Baum noch keine gemeinsame gibt (gemessen: kein Treffer fuer
 * zeitkonstanten Vergleich in src/ und include/). Sie gehoert mittelfristig
 * neben osk_wipe() in osk_mem.h - das ist eine Meldung an den Master, kein
 * Eingriff in einen fremden Vertrag.
 *
 * Umsetzung: src/classes/transport/yk_ct.c */
#ifndef OSK_YK_CT_H
#define OSK_YK_CT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* true, wenn beide Puffer gleich sind. Die Laufzeit haengt nur von len ab,
 * nicht vom Inhalt. NULL bei len > 0 gilt als ungleich. */
bool yk_ct_equal(const void *a, const void *b, size_t len);

/* Prueft einen 6-Byte-Zugangscode gegen den gespeicherten.
 *
 * Der Zugangscode ist das Geheimnis, das ein Schreiben auf einen
 * Steckplatz freigibt. Er kommt vom Draht und wird gegen einen
 * gespeicherten Wert gehalten - genau die Stelle, an der ein
 * abbrechender Vergleich die Bytes einzeln erraten liesse.
 *
 * Wo der Code im Rahmen liegt, weiss diese Schicht NICHT (siehe
 * yk_frame.h, "Was hier nicht entschieden wird"). Der Aufrufer gibt den
 * Zeiger; verglichen wird hier, damit der Vergleich nur einmal im Baum
 * steht. */
#define YK_ACC_CODE_LEN 6

bool yk_ct_acc_code_ok(const uint8_t given[YK_ACC_CODE_LEN],
		       const uint8_t stored[YK_ACC_CODE_LEN]);

#endif
