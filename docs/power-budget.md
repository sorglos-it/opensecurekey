# Versorgung aus USB

Stand 25.10.2026. **Gerechnet, nicht gemessen** — nichts davon lief auf Hardware.
Jede Datenblattzahl hat ihre Quelle dabei; jede Annahme ist als **Annahme**
gekennzeichnet. Wo das Datenblatt schweigt, steht `ungeprueft`.

**Der Akku entfällt (Entscheidung von Thomas, 25.10.2026).** Das Gerät läuft
ausschließlich aus USB. Diese Datei hieß „Energiebudget" und enthielt Ruhestrom,
Laden, Laufzeit und die Akkugröße. Davon bleibt hier nur die **Versorgung**: wie
der Chip an `VDDH` hängt und wie viel er nach außen abgeben darf. Die
**Stromaufnahmen** stehen jetzt allein in [`stromaufnahme.md`](stromaufnahme.md);
Laden, Laufzeit und Akkugröße sind erledigt und stehen in
[`zellen.md`](zellen.md).

Quellen der Chipzahlen:

- **nRF52840 Product Specification v1.1, Doc-ID 4413_417, 28.02.2019.**
  Die gewünschte Fassung v1.11 war nicht beschaffbar (Cloudflare 403, kein
  Spiegel gefunden). Die beiden wichtigsten Tabellen (Schlaf- und CPU-Ströme)
  wurden gegen die heutige Online-Fassung auf docs.nordicsemi.com geprüft und
  sind **Zahl für Zahl gleich**. Dass auch die übrigen Werte in v1.11
  unverändert sind, ist `ungeprueft`. Raytac beruft sich seinerseits auf v1.7.
- **SE051 Datenblatt Rev. 2.0, 8.7.2024, Tabelle 11.**
- **Winbond W25Q512JV, Revision B, 25.6.2019, Kap. 9.4.**
- **Solomon Systech SSD1306, Rev 1.1, April 2008, Tab. 12-1** und die
  Herstellerseite des gewählten Glases.

Alle nRF52840-Werte sind **Typ.-Werte bei 3 V, 25 °C**; das Datenblatt gibt für
die Stromszenarien weder Min. noch Max. an (PS v1.1, Abschn. 5.2).

## Die wichtigste Zahl zuerst: wie das Gerät versorgt wird

Das entscheidet alles andere, und hier steckt eine Falle, die man leicht
übersieht.

**Der nRF52840 darf nicht mit den 5 V des USB direkt an `VDD`.** `VDD` endet bei
**3,6 V**, die absolute Grenze liegt bei **3,9 V** (PS v1.1, Tab. 172 und
Tab. 173). Das zerstört den Chip. (Früher stand hier der volle LiPo mit 4,2 V;
den gibt es nicht mehr, die Falle ist dieselbe.)

**Richtig ist `VDDH` im High-voltage-Betrieb:** 2,5 V bis **5,5 V** (Tab. 172),
absolute Grenze 5,8 V. Der Chip erzeugt sein `VDD` dann selbst in der
REG0-Stufe. **Ein externer Spannungsregler ist nicht nötig.** Die
USB-5-V (`VBUS`) passen an `VDDH`.

Welche Betriebsart gilt, liest die Firmware im Register `MAINREGSTATUS`
(POWER, Offset 0x640): `Normal` = aus `VDD` gespeist, `High` = aus `VDDH`
(PS v1.1, Abschn. 5.3.7.20).

### Und hier die Falle: wie viel darf extern aus dem internen VDD fließen

PS v1.1, Abschn. 5.3.8.2, REG0-Stufe:

| Größe | Bedeutung | Grenze |
| --- | --- | --- |
| `IEXT,HIGH` | externer Stromzug, High-voltage-Betrieb, **Funk ≤ 4 dBm** | **max. 25 mA** |
| `IEXT,LOW` | externer Stromzug, High-voltage-Betrieb, **Funk > 4 dBm** | **max. 5 mA** |
| `IEXT,OFF` | externer Stromzug, System OFF | max. 1 mA |
| `VREG0,DROP` | Mindestabfall `VDDH` − `VDD` | **0,3 V** |
| `VDDOUT` | einstellbare Ausgangsspannung von REG0 | 1,8 … 3,3 V |

„Externer Stromzug" ist alles, was aus dem intern erzeugten `VDD` herausfließt —
die GPIO-Ströme **und** alles, was man als Nebenverbraucher daran hängt.

**Was das für uns heißt, mit unseren Teilen gerechnet:**

| Nebenverbraucher | Strom im Betrieb |
| --- | --- |
| OLED 0,71" (Herstellerangabe max.) | **25 mA** |
| Flash W25Q512JV beim Lesen (50-MHz-Spalte) | **20 mA** |
| SE051C2 bei asymmetrischer Krypto | **14,4 mA** |
| Fingerabdrucksensor bei der Aufnahme | 12 mA |
| RGB-LED, eine Farbe | bis 20 mA |

**Jeder einzelne dieser Verbraucher sprengt allein schon die 5 mA, die bei
Sendeleistung über 4 dBm erlaubt sind.** Und OLED plus Flash zusammen
überschreiten auch die 25 mA.

Drei Wege, drei Preise:

| Weg | Was man tut | Kosten |
| --- | --- | --- |
| **A (empfohlen)** | Alles aus dem internen `VDD`, aber **Sendeleistung auf ≤ 4 dBm begrenzen** und per Lastschalter dafür sorgen, dass nie zwei große Verbraucher gleichzeitig laufen | Reichweite sinkt; Firmware muss die Reihenfolge einhalten |
| B | Eigene 3,0-V-Schiene aus `VBUS` für OLED, Flash und Sicherheitschip | ein Abwärtswandler mehr (aus `VBUS`); mehr Platz, mehr Ruhestrom |
| C | Sendeleistung frei, aber alle Nebenverbraucher an eine eigene Schiene | wie B |

**Entschieden: Weg A.** Für einen Schlüssel, der in Armlänge zu einem Handy oder
Rechner redet, reichen 0 dBm. Der Gewinn ist groß: bei 0 dBm mit
DC-DC-Wandler sind es **4,8 mA** Sendestrom, bei +8 dBm ohne Wandler
**32,7 mA** — fast das Siebenfache (PS v1.1, Abschn. 6.20.15.2).

Ein Wandler, der nur Funkreichweite bringt, die niemand braucht, wäre Vorrat:
mehr Teile, mehr Platz, mehr Ruhestrom, mehr Fehlerquellen. **Die Firmware
begrenzt `TXPOWER` daher auf höchstens +4 dBm** — das ist die Bedingung dafür,
dass die 25 mA gelten, keine Empfehlung.

**Pflicht in jedem Fall: der DC-DC-Wandler muss an.** Er halbiert fast alles:

| | mit DC-DC | mit LDO |
| --- | --- | --- |
| CPU 64 MHz aus Flash | **3,3 mA** | 6,3 mA |
| Senden 0 dBm (nur Funk) | **4,8 mA** | 10,6 mA |
| Senden +8 dBm (nur Funk) | **14,8 mA** | 32,7 mA |
| Empfangen 1 Mbit BLE (nur Funk) | **4,6 mA** | 9,9 mA |
| CPU + Senden 0 dBm zusammen | **8,1 mA** | 15,4 mA |
| CPU + Empfangen zusammen | **8,6 mA** | 16,2 mA |

Dafür braucht das Raytac-Modul **extern eine Spule L4: 10 µH, 0603, mindestens
80 mA** (Raytac Approval Sheet Ver. L, Kap. 8). Die Spule für REG1 und der
32-MHz-Quarz sind im Modul drin.

## USB als einzige Quelle

| Größe | Wert | Quelle |
| --- | --- | --- |
| Strom ohne Aushandlung | **100 mA** dürfen immer fließen | USB, hier nicht nachgelesen: `ungeprueft` |
| USB-Regler des nRF52840 | **170 µA** aus `VBUS` bei aktivem USBD | PS v1.1, Abschn. 5.3.8.5 |
| `VBUS` erkannt (steigend) | 3,4 / 4,0 / 4,3 V | ebd. |
| `VBUS` weg (fallend) | 3,0 / 3,6 / 3,9 V | ebd. |

Die Stromzahlen der Verbraucher und der Spitzenstrom (67,5 mA im ungünstigsten
Fall, gegen die 25 mA oben und die 100 mA hier) stehen in
[`stromaufnahme.md`](stromaufnahme.md).

**Offen:** ob der Spannungseinbruch auf `VBUS` beim Einstecken oder bei einem
Lastsprung die Brownout-Erkennung auslöst (`VPOF,HV` ist im
High-voltage-Betrieb zwischen 2,7 und 4,2 V einstellbar, ±5 %). Nicht gerechnet,
nicht gemessen.

## Was entfallen ist

Laden am USB (MCP73832, `R_PROG`, Ladezeit, Wärme im Laderegler), die
Laufzeitrechnung für 100 und 150 mAh und die Antwort auf „wie groß muss der Akku
sein". Alles gerechnet, nie gebaut. Die Gründe stehen in
[`zellen.md`](zellen.md); der Wortlaut im Git-Verlauf dieser Datei (Stand
23.10.2026).

## Quellen

- nRF52840 Product Specification v1.1, Doc-ID 4413_417, 28.02.2019 —
  Abschn. 5.2.1.1 (Schlaf), 5.2.1.3 (CPU), 5.2.1.11 (zusammengesetzt),
  5.3.8.2 (REG0), 5.4.4 (LFCLK), 6.14.14.1 (NFCT), 6.20.15.2/3 (Funk),
  Tab. 172 (Betriebsbedingungen), Tab. 173 (absolute Grenzwerte)
- nRF52840 PS, Online-Fassung, abgerufen 23.10.2026 —
  https://docs.nordicsemi.com/bundle/ps_nrf52840
- SE051 Datenblatt Rev. 2.0, 8. Juli 2024, Tab. 11, Abschn. 5.2
- Winbond W25Q512JV, Revision B, 25. Juni 2019, Kap. 9.4
- Solomon Systech SSD1306, Rev 1.1, April 2008, Tab. 12-1
- Raytac MDBT50Q-1MV2 Approval Sheet Ver. L, 24.05.2023, Kap. 8
