# Energiebudget: Laufzeit je Zellenkapazität

Stand 25.10.2026. **Gerechnet, nicht gemessen.** Nichts davon lief auf
Hardware. Jede Zahl hat ihre Quelle dabei, jede Annahme ist als **Annahme**
gekennzeichnet, und wo das Datenblatt schweigt, steht `ungeprueft`.

Diese Datei beantwortet **eine** Frage, die in
[`power-budget.md`](power-budget.md) offen geblieben ist und in
[`zellen.md`](zellen.md) ausdrücklich als Lücke vermerkt ist:

> Wie lange hält das Gerät ohne Rechner — bei **40, 100 und 150 mAh** — und was
> treibt den Verbrauch?

`power-budget.md` vom 23.10.2026 hat die Chipströme aus den Datenblättern
geholt und rechnet 150 und 100 mAh. **Für 40 mAh war nichts gerechnet.** Die
Stromaufnahmen werden hier nicht neu erhoben, sondern aus jener Datei
übernommen; die Herleitung je Chip steht dort. Neu sind hier: die drei
Betriebsfälle über drei Kapazitäten, die **Selbstentladung** und die
**Alterung**.

**Das Ergebnis in einem Satz vorweg:** Die Kapazität ist nicht das, was die
Laufzeit bestimmt. Nach dem einzigen belegten Datenblattwert zur
Selbstentladung bringt der Sprung von 40 auf 150 mAh nur **62 auf 92 Tage** —
weil die Selbstentladung mit der Kapazität mitwächst.

## Woher der Akku überhaupt gebraucht wird

Der nRF52840 kann sich **nicht** aus dem NFC-Feld versorgen. Ohne Akku gäbe es
NFC und Bluetooth nur am Rechner, und die Uhr für zeitbasierte Einmalcodes
(TOTP) stünde still. Am Rechner steckend lädt das Gerät und läuft aus USB —
die Laufzeiten hier gelten also für die Zeit **zwischen** zwei Einsteckvorgängen.

## 1. Die Stromaufnahmen, mit Quelle

Alle nRF52840-Werte sind **Typ.-Werte bei 3 V, 25 °C**, mit DC-DC-Wandler; das
Datenblatt gibt für die Stromszenarien weder Min. noch Max. an (PS v1.1,
Abschn. 5.2).

### Ruhe

| Posten | Strom | Quelle |
| --- | --- | --- |
| nRF52840, System ON, 256 kB RAM-Erhaltung, **Wecken per RTC** | **3,16 µA** | nRF52840 PS v1.1, Abschn. 5.2.1.1 |
| 32,768-kHz-Quarz (`ILFXO`) | 0,23 µA | ebd., Abschn. 5.4.4 |
| NFC im SENSE STATE (`Isense`) | **0,10 µA** | ebd., Abschn. 6.14.14.1 |
| SE051C2, **Deep Power-down** (ENA = 0) | **3,00 µA** typ., 5 µA max. | SE051 DS Rev. 2.0, Tab. 11 |
| SE051C2, Power-down (zum Vergleich) | 430 µA typ. | ebd. |
| W25Q512JV, **Deep Power Down** (Befehl `B9h`) | **1,00 µA** typ., 20 µA max. | W25Q512JV Rev. B, Kap. 9.4 |
| W25Q512JV, Standby (zum Vergleich) | 10 µA typ., 60 µA max. | ebd. |
| OLED über Lastschalter abgeschaltet | ~0 | — |
| OLED-Controller im Sleep (zum Vergleich) | max. 10 µA IDD + max. 10 µA ICC | SSD1306 Rev 1.1, Tab. 12-1 |
| MCP73832, Rückentladung aus dem Akku ohne USB | **< 2,00 µA** | Microchip DS20001984H, Abschn. 4.1/4.3 |
| RGB-LED aus | 0 | — |
| **Summe Ruhe** | **≈ 9,5 µA** | `power-budget.md` |

**Zum Deep Power-down des SE051C2, wie im Auftrag gefragt:** Der Befund hält
stand. `ENA` liegt auf `P1.10` (`../hardware/schematic.md`, Zeile 218, Pin 11),
der Chip wird über `Vin` versorgt und `VCC` aus `Vout` gespeist — genau die
Beschaltung, die das Datenblatt für Deep Power-down verlangt (SE051 DS
Rev. 2.0, Abschn. 5.2). **Das macht aus 430 µA 3 µA, Faktor 143.** Ohne diese
Beschaltung wäre der Ruhestrom 436 statt 9,5 µA und das Gerät hielte bei
150 mAh noch **elf Tage** statt Monate. Es ist der größte Einzelgewinn des
ganzen Entwurfs, und er ist auf der Platine gezeichnet, nicht nur gewünscht.

**Zwei Fallen aus dem Datenblatt, die hier mitgerechnet sind:**

- Wecken per **GPIOTE-PORT** kostet 2,36 µA, per **GPIOTE-Event** 17,37 µA
  (PS v1.1, Abschn. 5.2.1.1) — Faktor 7 im Weckpfad des Tasters.
- „Aufwachen, wenn ein NFC-Feld kommt" kostet **100 nA**. Das Datenblatt sagt
  zum ACTIVATED-Strom von 400 µA ausdrücklich: *„This current does not apply
  when in NFC field."* NFC-Bereitschaft ist praktisch gratis.

### Betrieb

| Posten | Strom | Quelle |
| --- | --- | --- |
| CPU 64 MHz aus Flash, mit DC-DC | **3,3 mA** | PS v1.1, Abschn. 5.2.1.3 |
| Senden 0 dBm, nur Funk, mit DC-DC | **4,8 mA** | ebd., Abschn. 6.20.15.2 |
| Empfangen 1 Mbit BLE, nur Funk, mit DC-DC | **4,6 mA** | ebd. |
| CPU + Senden 0 dBm zusammen | **8,1 mA** | ebd., Abschn. 5.2.1.11 |
| NFC im ACTIVATED STATE | **400 µA** | ebd., Abschn. 6.14.14.1 |
| SE051C2 bei asymmetrischer Krypto | **14,4 mA** | SE051 DS Rev. 2.0, Tab. 11 |
| W25Q512JV beim Lesen (50-MHz-Spalte) | **20 mA** | W25Q512JV Rev. B, Kap. 9.4 |
| OLED 0,71", `IBAT` bei `VCC` = 7,5 V | typ. **12,0 mA**, max. **25,0 mA** | Crystalfontz CFAL4864A-071BW, 2017-07-05, „Electrical Characteristics" |

Gerechnet wird unten mit den **25 mA** der Anzeige, also mit der Obergrenze.
Welcher der beiden Werte gilt, hängt davon ab, wie viele Pixel leuchten, und
ist **zu messen**.

## 2. Selbstentladung — der Posten, der alles kippt

Im Auftrag stand die Vermutung, bei 40 mAh könne die Selbstentladung in
derselben Größenordnung liegen wie der Ruhestrom. **Sie liegt darüber, und das
bei jeder der drei Kapazitäten.**

Der einzige belegte Wert, den die Zelldatenblätter zu diesem Thema hergeben,
ist ein Prüfkriterium:

> Nach **28 Tagen Lagerung bei 25 ± 5 °C** liegt die Restkapazität über
> **80 % der Nennkapazität** (geladen und entladen mit 0,2 C bis 3,0 V).
>
> — PKCELL, „Li-Polymer Battery Technology Specification" 402025 150 mAh
> (= Adafruit 1317, die verbaute Zelle), Dokument QA.S.0228

**Wie belastbar diese Zahl ist, offen gesagt:** Ich habe sie **nicht selbst im
PDF gelesen.** Das Dokument benutzt CID-kodierte Schriften; aus den Textströmen
war nur die Titelseite zu gewinnen. Die Zahl stammt aus den Fassungen desselben
Dokuments bei **DigiKey und Mouser**, die darin übereinstimmen. Dass sie im
Original so steht, ist damit **wahrscheinlich, aber `ungeprueft`**.

**Und wichtiger als die Herkunft ist, was die Zahl bedeutet:** „über 80 % nach
28 Tagen" ist eine **Annahmegrenze für eine Prüfung**, keine Angabe der
tatsächlichen Selbstentladung. Die Zelle darf in 28 Tagen bis zu 20 % verlieren
und gilt noch als in Ordnung. Was sie wirklich verliert, sagt kein gelesenes
Datenblatt. Deshalb wird unten **mit beiden Sätzen gerechnet**:

| Satz | Wert | Was er ist |
| --- | --- | --- |
| **Datenblatt-Obergrenze** | **20 % der Nennkapazität in 28 Tagen** (≈ 21 %/Monat) | belegt, aber Prüfgrenze — der ungünstigste zulässige Fall |
| **Annahme** | **2 %/Monat** | **frei geschätzt**, in keinem Datenblatt belegt; der Wert, mit dem `power-budget.md` rechnet |

Zwischen diesen beiden liegt **Faktor 10**. Die Wahrheit liegt dazwischen, und
**wo genau, entscheidet die Laufzeit stärker als die Kapazität.** Das ist der
wichtigste offene Posten dieses Budgets.

**Die anderen beiden Zellen:** Zu DTP301120 (40 mAh) und LiPol LP302024
(100 mAh) ist mir kein Retentionswert zugänglich geworden. Das DTP-Datenblatt
(Data Power, File No. E-SPE-1106-01, Ver. 1.0, 06.11.2015) führt im
Inhaltsverzeichnis einen Lagerversuch, aber auch dieses PDF ist CID-kodiert und
gab seine Tabellen nicht her; die bei Händlern genannten Werte widersprechen
sich und nennen keine Prüfbedingungen. **`ungeprueft`.** Gerechnet wird für alle
drei Kapazitäten mit dem PKCELL-Kriterium — das ist eine **Übertragung**, keine
Datenblattangabe für die kleinen Zellen.

### Der zweite stille Verbraucher: die Schutzplatine

Keine dieser Zellen ist nackt; alle haben eine Schutzschaltung, und die
verbraucht selbst. Belegt ist das nur bei einer:

| Zelle | Eigenverbrauch der Schutzplatine | Quelle |
| --- | --- | --- |
| YDL 402020 (100 mAh) | **1,0 bis 7,0 µA** im Betrieb, max. 1 µA im Power-down | YDL-JS-SP-402020 Rev. A0, Abschn. 8.1 |
| LiPol LP302024 (100 mAh) | nicht genannt | `ungeprueft` |
| DTP301120 (40 mAh) | nicht genannt | `ungeprueft` |
| PKCELL 402025 (150 mAh, verbaut) | nicht genannt | `ungeprueft` |

**7 µA sind drei Viertel des gesamten Ruhebudgets von 9,5 µA.** Was das kostet,
wenn man eine Zelle mit einer solchen Platine nimmt (Fall 1, Selbstentladung
nach Annahme):

| Nennkapazität | ohne PCM-Eigenverbrauch | mit 7 µA | Verlust |
| --- | --- | --- | --- |
| 40 mAh | 126 Tage | **76 Tage** | −40 % |
| 100 mAh | 271 Tage | **173 Tage** | −36 % |
| 150 mAh | 366 Tage | **242 Tage** | −34 % |

In den Laufzeittabellen unten ist dieser Posten **nicht** enthalten, weil er für
die verbaute Zelle nicht belegt ist. Wer eine Zelle mit unbekannter
Schutzplatine wählt, muss bis zu einem Drittel der Laufzeit abziehen.

## 3. Alterung

Der Plan nennt LiPo im Schlüsselanhänger als Verbrauchsteil. Was dazu belegt
ist:

> **> 300 Zyklen**, gemessen mit 0,2 C Entladung bis 3,0 V, Ende der Prüfung
> bei Unterschreiten von **60 % der Nennkapazität**.
>
> — PKCELL QA.S.0228 (Edition von 2014; Herkunft wie oben: DigiKey-/Mouser-Fassung,
> von mir nicht im Original nachgelesen, `ungeprueft`)

Lagerung: 1 Monat bei −5 … 35 °C, 6 Monate bei −20 … 45 °C.

**Was daraus für „ein, zwei Jahre" folgt — und was nicht:**

Das Datenblatt gibt Kapazitätsverlust **über Zyklen**, nicht **über Zeit**. Für
die Frage „wie viel bleibt nach einem Jahr im Schlüsselbund" ist das die falsche
Achse: kalendarische Alterung ohne Zyklen steht in **keinem** der gelesenen
Datenblätter. **Das ist eine echte Lücke, keine Nachlässigkeit der Rechnung.**

Über die Zyklenachse lässt sich wenigstens einordnen, wie weit das Datenblatt
reicht:

| Vollzyklen je Jahr | nach 1 Jahr | nach 2 Jahren |
| --- | --- | --- |
| 52 (wöchentlich leergefahren) | 52 — Datenblatt deckt es ab | 104 — gedeckt |
| 150 | 150 — gedeckt | 300 — **Grenze erreicht** |
| 365 (täglich leergefahren) | 365 — **jenseits der Grenze** | 730 — jenseits |

Bei dem Gerät, wie es gedacht ist — fast täglich am USB, wo es nachlädt —
kommen keine **Voll**zyklen zusammen; ein Nachladen von 80 auf 100 % ist ein
Fünftel eines Zyklus. Realistisch bleibt man damit über zwei Jahre deutlich
unter 300 Zyklen und also im gedeckten Bereich.

**Was sich daraus belegt sagen lässt:** Innerhalb von 300 Zyklen garantiert das
Datenblatt **≥ 60 %** der Nennkapazität. Mehr nicht — keine Kurve, kein Wert für
ein Jahr, keine Angabe zum Verlust ohne Benutzung. Wer auf der sicheren Seite
rechnen will, nimmt die Laufzeiten unten **mal 0,6** als Zustand am Ende der
gedeckten Lebensdauer:

| Nennkapazität | Fall 1 neu (Annahme-Selbstentladung) | bei 60 % Restkapazität |
| --- | --- | --- |
| 40 mAh | 126 Tage | **≈ 76 Tage** |
| 100 mAh | 271 Tage | **≈ 163 Tage** |
| 150 mAh | 366 Tage | **≈ 220 Tage** |

(Die Multiplikation mit 0,6 ist eine **Vereinfachung**: sie unterstellt, dass
der Verbrauch gleich bleibt und nur die Kapazität sinkt. Der Innenwiderstand
steigt mit der Alterung ebenfalls; das ist hier nicht gerechnet und
`ungeprueft`.)

## 4. Die drei Betriebsfälle

**Annahmen, die für alle drei gelten** — alle frei gewählt und zu prüfen:

- **80 % der Nennkapazität nutzbar.** Abschaltung oberhalb der
  Zellschutzschwelle von 3,0 V. **Annahme.**
- Ruhestrom **9,5 µA** nach der Tabelle oben, also mit Lastschalter für die
  Anzeige und mit SE051C2 im Deep Power-down.
- Eigenverbrauch der Schutzplatine **nicht** enthalten (für die verbaute Zelle
  nicht belegt; Abzug siehe oben).
- Anzeige mit **25 mA** gerechnet, der Obergrenze.

### Fall 1 — nur Uhr

Das Gerät liegt in der Tasche, nichts passiert, die Uhr läuft für die
Einmalcodes. Verbrauch: **9,5 µA × 24 h = 0,228 mAh/Tag** plus
Selbstentladung.

| Nennkapazität | nutzbar | Selbstentladung nach **Annahme** (2 %/Monat) | nach **Datenblatt-Obergrenze** (20 %/28 d) |
| --- | --- | --- | --- |
| **40 mAh** | 32 mAh | **126 Tage** (4,1 Monate) | **62 Tage** (2,0 Monate) |
| **100 mAh** | 80 mAh | **271 Tage** (8,9 Monate) | **85 Tage** (2,8 Monate) |
| **150 mAh** | 120 mAh | **366 Tage** (12,0 Monate) | **92 Tage** (3,0 Monate) |

Rechenweg 150 mAh/Annahme: 0,228 mAh Ruhe + 0,100 mAh Selbstentladung
(2 % von 150 mAh auf 30 Tage) = 0,328 mAh/Tag; 120 / 0,328 = 366 Tage.

**Hier sieht man den Kern des Befundes.** Nach der Annahme wächst die Laufzeit
fast mit der Kapazität (126 → 366 Tage, Faktor 2,9 bei Faktor 3,75 Kapazität).
Nach der Datenblatt-Obergrenze wächst sie **kaum** (62 → 92 Tage, Faktor 1,5) —
weil die Selbstentladung proportional zur Kapazität mitwächst und bei großen
Zellen alles andere auffrisst.

### Fall 2 — gelegentlich NFC

**Annahmen, ausdrücklich benannt:** **8 Antippvorgänge am Tag**, jeder **1,5 s**
Funkbetrieb, dazu **2 s Anzeige** zur Bestätigung am Gerät. Acht ist gewählt als
„ein paarmal am Tag angemeldet"; die 1,5 s sind eine Annahme — das Datenblatt
belegt nur, dass das Feld binnen **20 µs** erkannt (`tsense`) und der Chip binnen
**500 µs** bereit ist (`tactivate`, PS v1.1, Abschn. 6.14.14.1), nicht, wie lange
ein ganzer Vorgang dauert.

Ladung je Vorgang, aus den Strömen oben gerechnet:

| Posten | Strom | Dauer | Ladung |
| --- | --- | --- | --- |
| NFC ACTIVATED | 0,4 mA | 1,5 s | 0,6 mA·s |
| CPU 64 MHz | 3,3 mA | 1,5 s | 4,95 mA·s |
| SE051C2 Signatur | 14,4 mA | 0,2 s (**Annahme**) | 2,88 mA·s |
| Flash lesen | 20 mA | 0,1 s (**Annahme**) | 2,0 mA·s |
| Anzeige | 25 mA | 2,0 s (**Annahme**) | 50,0 mA·s |
| **Summe mit Anzeige** | | | **60,4 mA·s = 16,8 µAh** |
| Summe **ohne** Anzeige | | | 10,4 mA·s = **2,90 µAh** |

8 × 16,8 µAh = **0,134 mAh/Tag** zusätzlich.

| Nennkapazität | nach **Annahme** | nach **Datenblatt-Obergrenze** |
| --- | --- | --- |
| **40 mAh** | **82 Tage** (2,7 Monate) | **49 Tage** (1,6 Monate) |
| **100 mAh** | **186 Tage** (6,1 Monate) | **74 Tage** (2,4 Monate) |
| **150 mAh** | **260 Tage** (8,5 Monate) | **84 Tage** (2,8 Monate) |

**Ohne Bestätigung auf der Anzeige** (das Telefon zeigt es an) wird daraus
115 / 252 / 342 Tage nach Annahme. **Die zwei Sekunden Anzeige je Antippen
kosten bei 150 mAh 82 Tage Laufzeit** — ein Viertel. Die Anzeige ist der
teuerste Posten einer Benutzung, nicht die Krypto: 50 von 60 mA·s.

### Fall 3 — Bluetooth in Bereitschaft

**Annahmen:** Werbepakete im Abstand von **1 s**, 3 Kanäle, zusammen **1,5 ms**
Funkzeit je Ereignis, mittlerer Strom **8,1 mA** (CPU + Senden 0 dBm mit DC-DC,
PS v1.1 Abschn. 5.2.1.11). Dazu dieselben 8 NFC-Vorgänge wie in Fall 2. Die
Paketlänge hängt vom Stack ab und ist **nicht gemessen**.

Rechenweg: 8,1 mA × 1,5 ms = 0,01215 mA·s je Ereignis; 3600 Ereignisse je Stunde
= 43,7 mA·s/h = 0,0121 mAh/h → **12,1 µA im Mittel**. Ruhestrom also
9,5 + 12 = **21,5 µA** → 0,516 mAh/Tag.

| Nennkapazität | nach **Annahme** | nach **Datenblatt-Obergrenze** |
| --- | --- | --- |
| **40 mAh** | **47 Tage** (1,5 Monate) | **34 Tage** (1,1 Monate) |
| **100 mAh** | **112 Tage** (3,7 Monate) | **59 Tage** (1,9 Monate) |
| **150 mAh** | **160 Tage** (5,2 Monate) | **70 Tage** (2,3 Monate) |

**Der Auftrag vermutete, das sei der teuerste Fall. Das trifft zu:** Bluetooth
in Dauerbereitschaft kostet **mehr als das gesamte übrige Ruhebudget** — 12 µA
gegen 9,5 µA — und halbiert die Laufzeit. Mit 2 s Abstand wären es 6 µA, mit
5 s noch 2,4 µA.

**Empfehlung: Bluetooth nicht dauernd anbieten, sondern erst auf Tastendruck.**
Das Gerät ist ein Schlüssel, kein Sensor — es muss nicht gefunden werden wollen,
bis der Nutzer es will. Das ist zugleich sicherheitstechnisch besser: ein Gerät,
das nicht wirbt, ist nicht anpeilbar (siehe [`threat-model.md`](threat-model.md)).

## 5. Woher der Verbrauch kommt

Fall 2, 150 mAh, Anteile am Tagesverbrauch:

| Satz | Ruhe | Nutzung (8× NFC) | Selbstentladung | Summe |
| --- | --- | --- | --- | --- |
| Annahme 2 %/Monat | 0,228 mAh (**49 %**) | 0,134 mAh (29 %) | 0,100 mAh (22 %) | 0,462 mAh/Tag |
| Datenblatt-Obergrenze | 0,228 mAh (16 %) | 0,134 mAh (9 %) | 1,071 mAh (**75 %**) | 1,434 mAh/Tag |

**Das ist die Antwort auf „was treibt den Verbrauch":** Es hängt daran, welcher
Selbstentladungssatz gilt — und das ist unbekannt. Gilt die Annahme, ist der
**Ruhestrom** der Treiber, und die drei Firmware-Entscheidungen (Lastschalter,
GPIOTE-PORT, BLE auf Abruf) entscheiden die Laufzeit. Gilt die
Datenblatt-Obergrenze, ist die **Selbstentladung** der Treiber, und keine
Firmware-Entscheidung hilft noch viel.

**Beides spricht gegen eine große Zelle.** Im ersten Fall, weil die Firmware
mehr bringt als die Kapazität; im zweiten, weil die Selbstentladung mitwächst.

## 6. Die Antwort auf die gestellte Frage

Ein Satz je Fall, nach dem **ungünstigeren** belegten Satz (Datenblatt-Obergrenze)
und nach der **Annahme** (2 %/Monat):

- **Fall 1, nur Uhr:** 40 mAh halten **2 bis 4 Monate**, 100 mAh **2,8 bis 9**,
  150 mAh **3 bis 12**. **Unbrauchbar wird es unter 40 mAh** — dort fällt die
  Laufzeit im ungünstigen Satz unter zwei Monate, und ein Schlüssel, dessen
  TOTP-Uhr im Urlaub stehenbleibt, ist kaputt.
- **Fall 2, 8× NFC am Tag:** 40 mAh halten **1,6 bis 2,7 Monate**, 100 mAh
  **2,4 bis 6,1**, 150 mAh **2,8 bis 8,5**. **40 mAh sind hier die Grenze der
  Brauchbarkeit** — sechs Wochen im ungünstigen Satz heißt: nachladen, bevor man
  es vergisst, nicht wenn es leer ist.
- **Fall 3, Bluetooth dauernd bereit:** 40 mAh halten **1,1 bis 1,5 Monate**,
  100 mAh **1,9 bis 3,7**, 150 mAh **2,3 bis 5,2**. **Unter 100 mAh ist dieser
  Betriebsfall nicht sinnvoll anzubieten.** Mit BLE nur auf Tastendruck
  verschwindet die Einschränkung.

**Welche Kapazität es braucht:**

**100 mAh sind die kleinste Kapazität, die alle drei Fälle trägt.** Sie hält im
ungünstigen Satz in jedem Fall über einen Monat und nach der Annahme ein halbes
Jahr bis neun Monate. 150 mAh sind komfortabler, kaufen aber im ungünstigen Satz
nur **7 Tage** mehr (Fall 1: 85 → 92 Tage) — für 3 mm mehr Zellenlänge. **Das
ist der schlechteste Tausch im ganzen Entwurf.**

**40 mAh** sind nicht ausgeschlossen, aber nur für Fall 1 und 2 **ohne**
Dauer-Bluetooth, und sie haben keine Reserve: jeder nicht belegte Posten
(Schutzplatine bis 7 µA, Alterung auf 60 %) frisst bei 40 mAh sofort ein Drittel
bis die Hälfte. Treffen zwei davon zusammen, sind es **unter vier Wochen**.

**Für das Ziel „so klein wie möglich"** ist damit die ehrliche Auskunft: Die
Kapazität darf von 150 auf 100 mAh fallen, ohne dass ein Betriebsfall verloren
geht — das sind nach den Maßen in [`zellen.md`](zellen.md) rund 3 mm Platine.
**Der Schritt auf 40 mAh spart weitere 2 mm und kostet dafür Fall 3 und jede
Reserve.** Ob 2 mm das wert sind, ist eine Abwägung, keine Rechnung; die Rechnung
sagt nur, was sie kostet.

## 7. Was **nicht** belegt ist

Nach den Vier Stufen von „geprüft": alles hier ist **gerechnet**, nichts ist
**gemessen**.

1. **Die tatsächliche Selbstentladung.** Der größte offene Posten, Faktor 10
   zwischen belegter Obergrenze und Annahme. Er entscheidet die Laufzeit stärker
   als die Kapazität. Die belegte Zahl ist dazu nur ein **Prüfkriterium**
   („über 80 % nach 28 Tagen"), keine Angabe des wirklichen Verlusts.
2. **Dass das PKCELL-Kriterium im Original so steht.** Aus DigiKey- und
   Mouser-Fassung übereinstimmend entnommen, von mir **nicht im PDF nachgelesen**
   (CID-Schriften). Dasselbe gilt für die 300 Zyklen.
3. **Retention und Zyklenzahl der 40- und 100-mAh-Zellen.** Kein Wert
   zugänglich; oben mit dem PKCELL-Kriterium gerechnet, also **übertragen**.
4. **Kalendarische Alterung.** Kein gelesenes Datenblatt nennt den
   Kapazitätsverlust über Zeit ohne Zyklen. Die Frage „was bleibt nach einem
   Jahr" ist mit Datenblättern **nicht beantwortbar**; belegt ist nur ≥ 60 %
   innerhalb 300 Zyklen.
5. **Eigenverbrauch der Schutzplatine** der verbauten Zelle. Belegt nur bei YDL
   (bis 7 µA = 74 % des Ruhebudgets). In den Laufzeiten oben **nicht** enthalten.
6. **Der tatsächliche Gesamt-Ruhestrom.** Das nRF52840-Datenblatt gibt die
   Weck-Szenarien als **Entweder-oder** an; ein Gerät, das per RTC **und** Taster
   **und** NFC-Feld geweckt werden kann, hat keine Datenblattzahl. Die 3,16 µA
   sind der größte Einzelwert, realistisch 3–4 µA — Schätzung.
7. **Welcher Anzeigestrom gilt**, typ. 12 oder max. 25 mA. Größter Einzelposten
   je Benutzung; der Unterschied ist bei 150 mAh in Fall 2 rund 1,5 Monate.
8. **Die Betriebsströme von QSPI, I2C und SPI.** Nordic spezifiziert sie in
   **keiner** Fassung des Datenblatts. Ein QSPI-Zugriff liegt mit Sicherheit über
   den 418 µA eines TIMER, lässt sich aber nicht belegen.
9. **Die Dauer einer SE051C2-Signatur** (0,2 s angenommen) und **die Dauer eines
   NFC-Vorgangs** (1,5 s angenommen). Ohne sie ist die Ladung je Benutzung
   Annahme.
10. **Die 80 % nutzbare Kapazität.** Annahme; hängt davon ab, wo die Firmware
    abschaltet.

**Was zuerst gemessen werden sollte:** Punkt 1. Eine geladene Zelle vier Wochen
liegen lassen und nachmessen kostet nichts außer Zeit und ersetzt die
Spannweite 62–366 Tage durch eine Zahl.

## Quellen

- nRF52840 Product Specification v1.1, Doc-ID 4413_417, 28.02.2019 —
  Abschn. 5.2.1.1 (Schlaf), 5.2.1.3 (CPU), 5.2.1.11 (zusammengesetzt),
  5.4.4 (LFCLK), 6.14.14.1 (NFCT), 6.20.15.2 (Funk)
- SE051 Datenblatt Rev. 2.0, 08.07.2024, Tab. 11, Abschn. 5.2
- Winbond W25Q512JV, Revision B, 25.06.2019, Kap. 9.4
- Solomon Systech SSD1306, Rev 1.1, April 2008, Tab. 12-1
- Crystalfontz CFAL4864A-071BW, Ausgabe 2017-07-05, „Electrical Characteristics"
- Microchip DS20001984H, MCP73831/2, Rev. H, Abschn. 4.1, 4.3
- PKCELL „Li-Polymer Battery Technology Specification" 402025 150 mAh,
  Dokument QA.S.0228, Edition A (2014) — Retention und Zyklenzahl aus der
  DigiKey- und Mouser-Fassung, `ungeprueft` im Original
- Data Power Technology Limited, File No. E-SPE-1106-01, Ver. 1.0, 06.11.2015
  (DTP301120, 40 mAh) — Lagerversuch im Inhaltsverzeichnis, Tabellen nicht
  lesbar
- Dongguan Youdalipo, YDL-JS-SP-402020 Rev. A0, Abschn. 8.1 (Eigenverbrauch PCM)
- Eigenes Repo: [`power-budget.md`](power-budget.md) (23.10.2026, Chipströme und
  Ruhestromsumme), [`zellen.md`](zellen.md) (Zellmaße und Schutzwerte),
  [`../hardware/schematic.md`](../hardware/schematic.md) (ENA auf `P1.10`)
