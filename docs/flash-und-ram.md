# Flash- und RAM-Rechnung

Offener Punkt 2 des Plans, gemessen am **30.10.2026**. Bis hierher war die
Antwort eine Hochrechnung vom 23.10. (264 kB von 960). Diese Seite ersetzt sie
durch eine Messung für den echten Befehlssatz.

**Die eine Antwort:** Der **Flash passt, mit Luft** — rund 297 kB von
959 kB, gemessen, nicht geschätzt. Der **RAM passt nicht verlässlich**: der
eigene Code allein belegt **134 kB fest** von 256 kB, und der tiefste
Aufrufweg braucht **12 344 Byte Stapel** auf einem Faden, für den der Plan
1–2 kB annahm.

> **BERICHTIGT am 30.10.2026 am Geräteabbild — zwei Sätze dieser Seite
> stimmen nicht, und ein dritter ist härter als er hier steht.**
> Der Satz „der RAM ist die Wand, nicht der Flash" stand hier mit der Zahl
> **246 865 Byte = 94 %**. Diese Zahl ist **doppelt gezählt**. Sie addiert
> den eigenen festen RAM (137 421 B) auf den Abbild-RAM vom 25.10.
> (97 100 B) — und der enthielt den gebundenen Teil desselben eigenen
> `.bss` schon. Abschnitt 3 sagt diese Unsicherheit ausdrücklich
> („wieviel davon, ist ohne das ELF nicht zu sagen") und addiert danach
> trotzdem. **Das ELF gibt es seit dem 30.10.2026**, und es sagt:
>
> | am Abbild gemessen | Byte | Anteil 256 kB |
> | --- | ---: | ---: |
> | **RAM des Abbilds, gesamt** | **105 292** | **40,2 %** |
> | davon eigener fester RAM | 71 706 | 27,3 % |
> | davon Zephyr-Seite | 33 586 | 12,8 % |
>
> Die Zephyr-Seite ist also **33 586 B**, nicht „höchstens 97 kB". Die
> ehrliche Hochrechnung, wenn aller eigene Code erreichbar wäre, lautet
> 137 425 + 33 586 = **171 011 B = 65 %** — eng, aber **keine Wand**, und
> USB, BLE und NFC sind darin nicht enthalten.
>
> **Die Wand steht an einer anderen Stelle, und dort ist sie echt:**
> der tiefste gemessene Aufrufweg braucht **12 344 Byte**, der Hauptfaden
> im Abbild ist **8 256 Byte** groß (`z_main_stack`, am ELF gemessen).
> **Das sind 4 088 Byte zu wenig** — zwei gemessene Zahlen, die sich
> direkt widersprechen, keine Hochrechnung. Am Gerät ist das der Absturz
> oder die Stapelwächter-Meldung.
>
> **Und der doppelte Index-Block ist gemessen:** `g_idx` 20 512 B und
> `g_idx_ct` 20 528 B, zusammen **41 040 B = 39 % des ganzen Abbild-RAMs**
> — derselbe Block einmal im Klartext, einmal verschlüsselt. Der Kommentar
> in `osk_vault_format.h:65` sagt „Im RAM ist immer nur EIN Block". Das
> stimmt und führt doch in die Irre: ein Block, aber zweimal.

Stufe **2 von 4**: übersetzt. Nichts davon ist im Geräteabbild gebunden,
nichts ist am Gerät gelaufen.

> **Auch dieser Satz ist überholt:** seit dem 30.10.2026 ist es gebunden.
> Das Abbild steht, `nrf52840dk/nrf52840`, 113 Quelldateien, FLASH
> **177 128 B (16,89 %)**, RAM **105 292 B (40,17 %)**. Gegengeprüft mit
> `arm-zephyr-eabi-size`: 176 136 + 992 = 177 128, auf das Byte. Am Gerät
> gelaufen ist es weiter nicht — Stufe **3 von 4**.

---

## 1. Womit gemessen wurde — und was damit nicht geht

**Das nRF Connect SDK ist auf diesem Rechner nicht installiert.** Geprüft:
`west`, `nrfutil`, `arm-zephyr-eabi-gcc`, `cmake` und `ninja` fehlen alle im
PATH von Windows. Ein vollständiges Geräteabbild ist hier **nicht** baubar;
`tools/check.py` überspringt `firmware/build` weiterhin zu Recht.

**Aber der ARM-Übersetzer ist da.** In der WSL (Debian) liegt
`gcc-arm-none-eabi 15:14.2.rel1-1`, also `arm-none-eabi-gcc 14.2.1` samt
`arm-none-eabi-size`, `-nm` und `-objdump`. **Nichts wurde nachinstalliert.**

Damit ist eine echte ARM-Messung je Baustein möglich, weil der eigene Code
fast kein Zephyr braucht: von 114 Quelldateien in `apps/firmware/src`
binden nur **4** einen `<zephyr/...>`-Kopf ein. **108 von 114 Dateien sind
einzeln für Cortex-M4 übersetzt worden.**

```
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
  -Os -ffunction-sections -fdata-sections -fstack-usage -c <datei>
arm-none-eabi-size -A <datei>.o
```

Die sechs Dateien, die nicht übersetzen, und warum:

| Datei | Grund |
| --- | --- |
| `src/main.c` | braucht `zephyr/kernel.h` — nur mit SDK |
| `src/classes/se050/se050_crypto_openssl.c` | braucht OpenSSL; ist der PC-Weg, gehört nicht ins Gerät |
| `src/views/view_pin.c`, `view_confirm.c`, `view_status.c`, `view_value.c` | `osk_pin_scheme_t` unbekannt. **Hier stand „ein Artefakt dieser Messung, kein Repo-Fehler — der echte Bau hat getrennte Einbindepfade". Das war falsch, nachgemessen am 30.10.2026:** der echte Bau legt alle Klassenordner in EINEN `-I`-Satz, `ctap/` davon vor `ui/`. Es war ein Repo-Fehler, und er hat den ersten Gerätebau gebrochen. Dazu trugen beide Dateien denselben Wächter `OSK_PIN_H`, womit die zweite in derselben Übersetzungseinheit gar nicht mehr ladbar war. Behoben: die UI-Fassung heißt jetzt `osk_pin_entry.h`. |

**Was diese Zahlen sind, und was sie nicht sind.** Sie sind echte
ARM-Zahlen, mit derselben Architektur, derselben Fließkommaeinstellung und
derselben Optimierung (`-Os`), die Zephyr für dieses Brett benutzt. Sie sind
**keine Abbildgröße**: der Binder wirft mit `--gc-sections` alles weg, was
`main()` nicht erreicht, und er zieht Zephyr, PSA und die C-Bibliothek dazu.
Die Summe unten ist also die Größe des **gesamten eigenen Codes**, nicht die
des Abbilds — genau die Zahl, nach der Punkt 2 fragt. Ein x86-Bau wäre für
diese Frage kein Beleg gewesen; er ist hier auch nicht benutzt worden.

---

## 2. Flash je Baustein

`arm-none-eabi-size -A`, Abschnitte getrennt. `Flash` = `.text` + `.rodata` +
`.data` (`.bss` liegt im RAM und steht in Abschnitt 3).

| Baustein | Dateien | `.text` | `.rodata` | `.data` | **Flash** |
| --- | ---: | ---: | ---: | ---: | ---: |
| `wallet` | 15 | 25 554 | 23 384 | 0 | **48 938** |
| `ctap` | 11 | 25 264 | 1 199 | 1 | **26 464** |
| `store` | 15 | 21 584 | 3 082 | 8 | **24 674** |
| `applets` (ohne Dongle) | 14 | 19 214 | 2 123 | 16 | **21 353** |
| `transport` | 15 | 12 180 | 222 | 13 | **12 415** |
| `se050` | 13 | 12 252 | 50 | 0 | **12 302** |
| `ui` | 9 | 4 480 | 6 261 | 0 | **10 741** |
| `authz` | 5 | 4 954 | 131 | 0 | **5 085** |
| `pqc` | 4 | 4 416 | 519 | 0 | **4 935** |
| Dongle (`applets/guard`) | 4 | 4 276 | 143 | 0 | **4 419** |
| `views` | 1 | 308 | 1 | 0 | **309** |
| `boot` (MCUboot-Anbindung) | 6 | 1 846 | 8 | 0 | **1 854** |
| `osk_mem.c`, `messbau.c` | 2 | 20 | 0 | 0 | **20** |
| **Summe ohne RSA** | **114** | **136 348** | **37 123** | **38** | **173 509** |
| **Summe mit RSA** | **114** | **136 478** | **37 123** | **38** | **173 639** |

### Die zwei Kconfig-Richtungen unterscheiden sich um 130 Byte

`CONFIG_OSK_RSA=y` gegen `=n`, Datei für Datei verglichen:

| Datei | Unterschied |
| --- | --- |
| `src/classes/applets/osk_rsa_card.c` | **+130 Byte** Flash, +5 Byte `.bss` |
| alle 107 anderen | 0 |

**Das ist der auffälligste Einzelbefund dieser Messung.** Die „zweite
Firmware-Fassung" kostet 130 Byte, nicht Kilobyte — weil RSA im SE050
gerechnet wird und `variant-rsa.conf` ausdrücklich keine
`PSA_WANT_ALG_RSA_*`-Zeile setzt. Die Entscheidung, die Mathematik im Chip zu
lassen, ist damit **gemessen** und nicht nur begründet. Alle weiteren Zahlen
dieser Seite gelten für beide Fassungen; wo nicht, steht es dabei.

### Passt es neben MCUboot in 1 MB?

Die Zahlen kommen aus `apps/firmware/boot/pm_static.yml`, nicht aus einer
Schätzung:

| Bereich | Größe |
| --- | ---: |
| internes Flash | 1 048 576 B (1 MB) |
| `mcuboot` | 49 152 B (48 kB) |
| `app` (laufender Platz) | 982 016 B (959 kB) |
| `osk_boot_state` + `settings_storage` + Reserve | 16 384 B |

**Der zweite Platz für das Update liegt im externen QSPI-Flash**
(`mcuboot_secondary`, 0xf0000 ab Adresse 0), nicht im internen. **A/B kostet
also intern nichts.** Das ist die Antwort auf „und bei A/B zweimal": nein,
nicht hier.

Die Hochrechnung auf das Abbild, mit der Zephyr-Seite aus der Messung vom
25.10.2026 (`docs/build.md`: Abbild 161 124 B, davon 30 758 B eigener Code,
also 130 366 B Zephyr + PSA + C-Bibliothek):

| | Byte | von 982 016 |
| --- | ---: | ---: |
| Zephyr, PSA, C-Bibliothek (gemessen 25.10.) | 130 366 | 13,3 % |
| eigener Code, alles erreichbar (gemessen 30.10.) | 173 509 | 17,7 % |
| **zusammen** | **303 875** (297 kB) | **30,9 %** |
| freier Rest | 678 141 (662 kB) | 69,1 % |

**Es passt, mit 662 kB Luft.** Die Hochrechnung vom 23.10. lautete 264 kB;
gemessen sind es 297 kB. Die Schätzung lag **33 kB zu niedrig** — die
Richtung stimmte, die Antwort bleibt dieselbe.

Was in den 297 kB **noch nicht** steckt, und alles davon macht sie größer:

- Die **Zephyr-Seite von USB, BLE und NFC** ist nicht geschrieben; `prj.conf`
  enthält heute keine einzige `CONFIG_USB_*`-, `CONFIG_BT_*`- oder
  `CONFIG_NFC_*`-Zeile. Das sind die großen fehlenden Posten.
- **Sechs Platzhalter** aus `src/messbau.c` stehen für Namen, die es am Gerät
  noch nicht gibt (I²C-Treiber, Vermittlung der Transporte, SCP03-Entpackung).
  Die echten Umsetzungen sind größer.
- Die **vier `views`-Dateien**, die hier nicht übersetzt haben.
- **MCUboot selbst** ist nicht gebaut; die 48 kB sind die Vorgabe des SDK,
  nicht eine Messung.

---

## 3. RAM — hier wird es eng

256 KB = 262 144 Byte. Fester Speicher des eigenen Codes, gemessen
(`.bss` + `.data`):

| Baustein | `.bss` | `.data` | **fester RAM** | Anteil an 256 kB |
| --- | ---: | ---: | ---: | ---: |
| `store` | 100 960 | 8 | **100 968** | **38,5 %** |
| `applets` (ohne Dongle) | 20 233 | 16 | **20 249** | 7,7 % |
| `transport` | 10 117 | 13 | **10 130** | 3,9 % |
| `ctap` | 3 246 | 1 | **3 247** | 1,2 % |
| `authz` | 1 364 | 0 | **1 364** | 0,5 % |
| Dongle (`applets/guard`) | 897 | 0 | **897** | 0,3 % |
| `ui` | 450 | 0 | **450** | 0,2 % |
| `se050` | 111 | 0 | **111** | 0,0 % |
| `wallet` | 5 | 0 | **5** | 0,0 % |
| `pqc` | 0 | 0 | **0** | 0,0 % |
| `boot`, `views`, Rest | 0 | 0 | **0** | 0,0 % |
| **Summe** | **137 383** | **38** | **137 421** (134 kB) | **52,4 %** |

Mit RSA: 137 426 B, also 5 Byte mehr.

### Wo die 100 kB des Tresors liegen

Nachgesehen mit `arm-none-eabi-nm --size-sort`, nicht hergeleitet:

| Symbol | Datei | Byte |
| --- | --- | ---: |
| `g_idx_ct` | `store/store_space.c:23` | 20 528 |
| `g_idx` | `store/store_space.c:20` | 20 512 |
| `v` (Zustand des Datenträgers) | `store/volume_core.c` | 17 032 |
| `g_blob` | `store/store_space.c:24` | 8 192 |
| `ct_buf` | `store/volume_core.c:84` | 4 119 |
| `scratch` | `store/volume_core.c:88` | 4 096 |
| `root_buf` | `store/volume_core.c:74` | 4 096 |
| `page_b` | `store/volume_core.c:73` | 4 096 |

Die beiden 20-kB-Puffer sind **Absicht und dokumentiert**:
`osk_vault_format.h:65` sagt „64 \* 320 B = 20 KiB RAM" für einen
Index-Block. Was dort nicht steht: der Block liegt **zweimal** im RAM, einmal
als Klartext (`g_idx`) und einmal als Chiffrat (`g_idx_ct`). Zusammen mit
`g_blob` sind das 49 kB in einer Datei.

Die vier 4-kB-Puffer in `volume_core.c` sind die **Datenträger-Betriebsarten**
und damit neu — sie waren am 25.10. noch nicht da.

### Die Gesamtrechnung, und warum sie nicht aufgeht

| Posten | Byte | Herkunft |
| --- | ---: | --- |
| Abbild-RAM vom 25.10.2026 | 97 100 | gemessen, `docs/build.md` |
| eigener fester RAM, alles erreichbar | 137 421 | gemessen, 30.10.2026 |
| tiefster Stapelweg, **ein** Faden | 12 344 | gemessen, Abschnitt 4 |

Die 97 100 Byte vom 25.10. enthalten die Zephyr-Seite **und** den Teil des
eigenen `.bss`, den der Binder nicht wegwerfen konnte — wieviel davon, ist
ohne das ELF nicht zu sagen, und ich sage es darum nicht. Die 97 100 sind
also eine **Obergrenze** für die Zephyr-Seite. Selbst in der freundlichsten
Lesart:

> 137 421 (eigener fester RAM) + 12 344 (ein tiefer Faden)
> = **149 765 Byte = 57 % der 256 kB**, bevor Zephyr, der USB-Stapel, der
> BLE-Controller, NFC und der zweite, dritte und vierte Fadenstapel
> überhaupt mitgerechnet sind.

Die Zephyr-Seite ist heute, **ohne** USB, BLE und NFC, höchstens 97 kB. Mit
diesen drei kommt sie dazu, nicht an ihre Stelle. 149 765 + 97 100 =
**246 865 Byte = 94 % der 256 kB** — und das ist eine Rechnung ohne den
USB-Stapel.

**Befund: der RAM reicht in der heutigen Form nicht.** Das ist keine
Schätzung der Richtung, sondern eine Addition gemessener Zahlen. Es ist auch
kein Fehler in einem Baustein: es ist die Summe aus einem Tresor, der 100 kB
feste Puffer hält, und einem Faden, der 12 kB Stapel braucht.

Was die Rechnung **nicht** sagt: ob es sich beheben lässt. Die 100 kB des
Tresors sind Puffer, nicht Nutzdaten — ob zwei 20-kB-Puffer gleichzeitig
gebraucht werden, entscheidet der Tresor-Strang, nicht diese Seite. Diese
Seite misst.

---

## 4. Der Stapel — offener Punkt 4a, und er ist größer als gedacht

Gemessen mit `-fstack-usage` je Funktion und den Aufrufkanten aus
`arm-none-eabi-objdump -d -r`, zum tiefsten Weg aufsummiert (Zyklen
abgebrochen). 970 Funktionen.

**Was Punkt 4a annahm:** drei Puffer von zusammen knapp 1,8 kB auf einem
Faden, der 1–2 kB groß ist. **Was gemessen ist:** einzelne Funktionsrahmen
bis **7 056 Byte**, und tiefste Wege bis **12 344 Byte**. Die Annahme lag um
den **Faktor 7** zu niedrig — und sie lag beim falschen Baustein. Der SE050
ist nicht der schlimmste Fall.

### Die zehn tiefsten Wege

| Byte | Eintritt | Fundstelle | Weg (gekürzt) |
| ---: | --- | --- | --- |
| **12 344** | `osk_apdu_handle` | `wallet/wallet_apdu.c:252` | → `cmd_sign_psbt` → `osk_psbt_sign` → `osk_btc_sighash_legacy` → `osk_sha256_final` |
| **12 064** | `cmd_sign_psbt` | `wallet/wallet_apdu.c:229` | → `osk_psbt_sign` → `osk_btc_sighash_legacy` → … |
| **11 248** | `osk_pqc_decaps` | `pqc/pqc_hybrid.c:129` | → `osk_mlkem_decaps` → `kpke_encrypt` → `osk_poly_sample_ntt` → `permute` |
| **11 168** | `osk_mlkem_decaps` | `pqc/pqc_mlkem.c:269` | → `kpke_encrypt` → … |
| **11 072** | `route` | `transport/nfc_tag.c:63` | → `osk_nfc_ctap_apdu` → `do_ctap_msg` → `osk_ctap_handle` → `do_get_assertion` → `osk_store_update` |
| **11 024** | `osk_nfc_ctap_apdu` | `transport/nfc_ctap.c:221` | → `do_ctap_msg` → … |
| **10 976** | `do_ctap_msg` | `transport/nfc_ctap.c:125` | → `osk_ctap_handle` → … |
| **9 936** | `osk_ble_gatt_write_request` | `transport/ble_gatt.c:174` | → `dispatch` → `osk_ctap_handle` → … |
| **9 276** | `osk_se050_apdu_exchange_in` | `se050/se050_apdu.c:445` | → `session_cmd` → `plain_exchange` → `osk_se050_t1_transceive` → `s_block` |
| **8 304** | `osk_ctap_handle` | `ctap/osk_ctap_core.c:1563` | → `do_get_assertion` → `osk_store_update` → `osk_blob_write` |

Der Wert **11 248 Byte** für `osk_pqc_decaps` deckt sich aufs Byte mit der
Messung, die Strang N unabhängig mit demselben Übersetzer gemacht hat
(`docs/pqc.md`). Zwei Messungen, eine Zahl.

### Die zehn größten einzelnen Rahmen

Das sind die Stellen, an denen der Platz tatsächlich belegt wird — die
Fundstellen für jeden, der kürzen soll.

| Byte | Funktion | Fundstelle |
| ---: | --- | --- |
| **7 056** | `cmd_sign_psbt` | `wallet/wallet_apdu.c:229` |
| **6 328** | `kpke_encrypt` | `pqc/pqc_mlkem.c:127` |
| **5 400** | `kpke_keygen` | `pqc/pqc_mlkem.c:53` |
| **4 232** | `osk_mlkem_decaps` | `pqc/pqc_mlkem.c:269` |
| **4 056** | `osk_se050_apdu_exchange_in` | `se050/se050_apdu.c:445` |
| **3 704** | `do_get_assertion` | `ctap/osk_ctap_core.c:1119` |
| **3 184** | `osk_applets_handle` | `applets/osk_applets.c:128` |
| **3 120** | `cmd_get_data` | `applets/osk_piv.c:373` |
| **2 672** | `do_ctap_msg` | `transport/nfc_ctap.c:125` |
| **2 664** | `osk_btc_sighash_legacy` | `wallet/wallet_btc.c:327` |

Größter Rahmen je Baustein: `wallet` 7 056 · `pqc` 6 328 · `se050` 4 056 ·
`ctap` 3 704 · `applets` 3 184 · `transport` 2 672 · `store` 1 408 ·
`authz` 224 · `ui` 128 · `views` 72.

### Die drei Puffer aus Punkt 4a — nachgesehen

Sie sind **weiter gewachsen**, seit der Punkt geschrieben wurde:

| Puffer | Fundstelle | im Plan | heute |
| --- | --- | ---: | ---: |
| `cmd[7 + SE050_MAX_PAYLOAD + 2]` | `se050/se050_apdu.c:78` | 898 | **898** |
| `payload[16 + SE050_MAX_PAYLOAD]` | `se050/se050_apdu.c:201` | 905 | **905** |
| `plain[7 + SE050_MAX_PAYLOAD]` | `se050/se050_apdu.c:490` | 896 | **896** |
| `plain[1024]` | `se050/se050_scp03.c:321` | — | **1 024** |
| `plain[1024]` | `se050/se050_scp03.c:463` | — | **1 024** |

`SE050_MAX_PAYLOAD` steht bei 889 (`se050_cmd.h:30`). Die beiden
`plain[1024]` in der SCP03-Schicht stehen **nicht** in Punkt 4a — der Punkt
nennt dort `plain[896]`. Der tiefste Weg durch den SE050 ist damit **9 276
Byte**, nicht 1,8 kB.

### Weitere große Felder auf dem Stapel, außerhalb des SE050

Gesucht wurde in allen Bausteinen, nicht nur im Treiber:

| Byte | Feld | Fundstelle |
| ---: | --- | --- |
| 1 024 | `buf[1024]` | `applets/osk_openpgp.c:271` |
| 1 024 | `buf[1024]` | `applets/osk_openpgp.c:284` |
| 516 | `buf[516]` | `ctap/osk_ctap_ext.c:515` |
| 512 | `blob[512]` | `ctap/osk_ctap_ext.c:485` |
| 320 | `rsp[320]` | `se050/se050.c:918` |
| 256 | `rsp[256]` | `se050/se050.c:652` |

### Was das am Gerät bedeutet

`prj.conf` setzt heute `CONFIG_MAIN_STACK_SIZE=8192`. Der Haupt-Faden hätte
damit 8 kB — **weniger als die 12 344 Byte des tiefsten Weges und weniger
als die 11 248 Byte des PQC-Weges.** Ein Faden mit Zephyrs Vorgabe von
1–2 kB stürzt an jedem der zehn obersten Wege ab.

Das ist **nicht gemessen am Gerät**, sondern aus Übersetzerzahlen
aufsummiert. Was es nicht ist: eine Schätzung. Die Rahmen sind die, die der
Übersetzer wirklich anlegt; der Weg ist der, den die Aufrufkanten wirklich
erlauben. Was zur echten Zahl noch fehlt, macht sie **größer**, nicht
kleiner: Zephyrs eigene Rahmen auf dem Weg, die Unterbrechungsbehandlung und
die sechs Platzhalter.

**Entschieden wird es trotzdem erst mit `CONFIG_THREAD_ANALYZER` am
Teststand** — so steht es in Punkt 4a, und das bleibt richtig. Diese Seite
nimmt die Entscheidung nicht vorweg, sie sagt nur, dass die Zahl, um die es
geht, nicht 1,8 kB ist.

---

## 5. Schätzung gegen Messung

| Baustein | Schätzung des Plans | gemessen (ARM) | Richtung |
| --- | --- | ---: | --- |
| `pqc` (ML-KEM-768 hybrid) | ~11 kB Flash | **4 935** | Schätzung **2,2-fach zu hoch** |
| RSA-Fassung gegen ohne | als eigener Posten geführt | **+130 B** | Schätzung **weit zu hoch** |
| eigener Code insgesamt | 140 261 B (80 Dateien, 25.10.) | **173 509** (114 Dateien) | +33 kB, **24 % mehr Code** |
| Abbild, alles erreichbar | 264 kB (Hochrechnung 25.10.) | **297 kB** (hochgerechnet) | Schätzung **33 kB zu niedrig** |
| Stapel SE050-Treiber | ~1,8 kB | **9 276** B (tiefster Weg) | Schätzung **5-fach zu niedrig** |
| Stapel insgesamt | gar nicht geschätzt | **12 344** B | neu |
| fester RAM eigener Code | gar nicht geschätzt | **137 421** B | neu |

Die Schätzungen lagen bei **Flash zu hoch** (ML-KEM, RSA) und beim **Stapel
weit zu niedrig**. Das ist kein Zufall: Flash ist das, worüber nachgedacht
wurde, und der Stapel ist das, was am PC nicht auffällt.

---

## 6. Was geprüft ist und was nicht

| | Stufe |
| --- | --- |
| 108 von 114 Quelldateien für Cortex-M4 übersetzt, beide Kconfig-Richtungen | **übersetzt** |
| Flash und fester RAM je Baustein mit `size -A` gemessen | **übersetzt** |
| Stapelrahmen mit `-fstack-usage`, Wege über `objdump`-Kanten | **übersetzt** |
| die 100 kB im Tresor mit `nm --size-sort` an den Symbolen nachgesehen | **übersetzt** |
| ein gebundenes Geräteabbild mit diesem Code | **nein** — kein SDK auf diesem Rechner |
| die Hochrechnung auf 297 kB | **gerechnet**, nicht gebunden |
| Stapelbedarf am Gerät (`CONFIG_THREAD_ANALYZER`) | **nein** — braucht Hardware |
| irgendetwas davon am Gerät gelaufen | **nein** |

Wer die Zahlen nachrechnet, braucht nur die WSL und
`arm-none-eabi-gcc 14.2.1` — kein SDK. Die Befehle stehen in Abschnitt 1.
