# Kleine LiPo-Zellen mit Schutzschaltung — erledigt, hinfällig

> **Status 25.10.2026: Entscheidung gefallen, die Zellenwahl entfällt.**
> Thomas hat entschieden, den Akku ganz zu streichen: keine Zelle, kein
> Laderegler, keine Schutzschaltung; das Gerät läuft nur aus USB. Diese Liste
> wird nicht mehr gebraucht. Sie bleibt stehen, damit niemand die Arbeit neu
> macht — **nicht als Einkaufsliste lesen.**
>
> **Warum der Akku fiel:** Die Uhr für zeitbasierte Einmalcodes (TOTP) braucht
> keine Dauerversorgung, weil sie beim Einstecken vom Rechner nachgestellt wird
> und der Code im Moment des Benutzens entsteht. Zeitstempel bei Signaturen holen
> die Zeit von einem Zeitstempel-Server. Damit kaufte der Akku nur noch NFC und
> Bluetooth ohne Rechner. Dagegen standen mehr Länge (wie viel, steht in `hardware/layout-notes.md`) und ein
> Verbrauchsteil mit wenigen Jahren Lebensdauer in einem Gerät, das Jahrzehnte
> halten soll. NFC und Bluetooth gibt es nun nur am Rechner — bewusst in Kauf
> genommen.
>
> **Was diese Arbeit ergeben hat, falls die Frage je wieder kommt:**
>
> - Die kürzeste passende Zelle (22 mm mit Schutzplatine) sparte nur etwa 4 mm
>   Platine; unter rund 9 mm Zellenlänge bestimmt die Schaltung das Maß.
> - 100 mAh waren nach der Rechnung die kleinste Kapazität, die alle drei
>   Betriebsfälle trug. Der Sprung auf 150 mAh kaufte im ungünstigen Satz nur
>   7 Tage.
> - Die **Selbstentladung** entschied die Laufzeit stärker als die Kapazität —
>   und sie war der größte offene Posten: belegt war nur ein Prüfkriterium
>   („über 80 % nach 28 Tagen"), kein wirklicher Wert; Faktor 10 zur Annahme.
> - Der **Eigenverbrauch der Schutzplatine** (YDL: bis 7 µA) kostete ein Drittel
>   der Laufzeit; bei den anderen Zellen stand er nicht im Datenblatt.
> - Für die beiden passenden Zellen (YDL 402020, LiPol LP302024) gab es keinen
>   belegten europäischen Händler.
>
> Die volle Rechnung steht im Git-Verlauf von `docs/stromaufnahme.md` (damals
> `docs/energiebudget.md`, Commit `87a19c1`). **Die Stromaufnahmen selbst gelten
> weiter**, siehe [`stromaufnahme.md`](stromaufnahme.md).
>
> **Die Platinenlängen unten sind Geschichte.** Die gültige Platinenlänge steht
> in `../hardware/`; nicht aus dieser Datei übernehmen.

---

Stand 25.10.2026. Gesucht waren Zellen **kürzer als 25 mm** bei höchstens
**4,05 mm** Dicke und rund **24 mm** Breite, mit **eingebauter Schutzschaltung**
und Anschluss. Jede Zahl hier stammt aus einem **Hersteller-Datenblatt**, nicht
aus einer Händlerliste; die Quelle steht in der Tabelle „Quellen".

**Die wichtigste Erkenntnis zuerst:** Die bisher im Repo geführten Zellenmaße
sind **nackte Zellenmaße ohne Schutzplatine**. Die Schutzplatine (PCM) sitzt am
Fahnenende und verlängert die Zelle um **2 bis 3 mm**. Für den Einbau zählt
allein das **Fertigmaß mit PCM** — und das ist die Zahl, die hier in der Tabelle
steht.

**Das gilt auch für die Zellenmaße in `docs/parts.md`.** Jede Zahl dort ist
eine nackte Zelle und damit für den Einbau **2 bis 3 mm zu kurz angesetzt**.
Wer mit Zellenlängen rechnet, nimmt die Zahlen aus dieser Datei, nicht die aus
`parts.md`.

## Die Tabelle

| Zelle | Kapazität | Länge × Breite × Dicke **mit PCM** | Anschluss | Schutz | Passt? |
| --- | --- | --- | --- | --- | --- |
| **YDL 402020** | 100 mAh | **22,0 × 20,0 × 4,0 mm** (alle drei „max.") | JST PHR-2, 2 Adern UL1007 28AWG, 50 ± 3 mm | **ja**, Werte unten | **ja** |
| **DTP301120** (SparkFun PRT-13852) | 40 mAh | **22,0 × 11,5 × 3,2 mm** (alle drei „Max") | JST-PHR-2PIN, UL3302 26AWG, 100 ± 3 mm | **ja**, DW01+ / 8205 | **ja** |
| **LiPol LP302024** | 100 mAh min., 110 mAh typ. | **24,0 ± 1,0 × 20,0 ± 0,5 × 3,0 ± 0,3 mm** | 2 Adern AWG28 UL1571, 50 ± 3 mm, **kein Stecker** | **ja**, Werte unten | **ja** (Höchstmaß 25,0) |
| PKCELL LP402025 (= Adafruit 1317, verbaut) | 150 mAh nom., 142 mAh min. | 25,0 ± 0,3 × 20,0 ± 0,3 × 4,0 ± 0,3 mm | JST PH-2P, UL1007 24AWG, 100 mm | ja, S-8261AAJMD + 8814 | **nein** — nicht kürzer als 25, und 4,3 mm Höchstdicke |
| SparkFun PRT-13853 (DTP401525) | 110 mAh | **27,0 × 15,5 × 4,2 mm** (alle drei „Max") | JST-PHR-2PIN, UL3302 26AWG, 100 ± 3 mm | ja, DW01+ / 8205 | **nein** — 27 statt der im Repo geführten 25,0 |

**Zu PRT-13853:** Die im Repo stehenden **25,0 mm sind das Maß der nackten
Zelle** (Datenblatt Abschn. 4, „L Max 25.0"). Dieselbe Datei zeichnet auf
Seite 9 das Fertigprodukt mit Schutzplatine und Kabel: **max. 27 mm lang,
max. 15,5 mm breit, max. 4,2 mm dick.** Diese Zelle ist damit **weder kürzer
als 25 mm noch dünner als 4,05 mm** — sie war nie die kürzeste belegte Zelle,
sondern 2 mm länger als gedacht.

**Zu PRT-13852:** Die 40 mAh, die im Repo ohne jedes Maß stehen, sind die
**DTP301120**: nackte Zelle max. 3,0 × 11,0 × 20,0 mm, **fertig mit PCM
max. 22 mm × 11,5 mm × 3,2 mm**. Sie passt — aber mit 40 mAh.

## Die zwei gerechneten Spalten

Gerechnet mit der Vorgabe aus dem Auftrag: die Zelle beginnt bei längs
**25,0 mm**, die Schaltung endet bei **33,87 mm**, das Antennenfeld ist
**18,98 mm** lang.

- Spalte 1: `Platinenlänge = max(25,0 + Zellenlänge; 33,87) + 18,98`
- Spalte 2: `Antennenfeld = 70 − max(25,0 + Zellenlänge; 33,87)`

| Zelle | Zellenlänge | Zellenende | **Platine, wenn Antenne 18,98 bleibt** | **Antennenfeld, wenn Platine 70 bleibt** |
| --- | --- | --- | --- | --- |
| **YDL 402020** | 22,0 | 47,0 | **65,98 mm** (−4,02) | **23,00 mm** (+4,02) |
| **DTP301120** (PRT-13852) | 22,0 | 47,0 | **65,98 mm** (−4,02) | **23,00 mm** (+4,02) |
| **LiPol LP302024** | 24,0 | 49,0 | **67,98 mm** (−2,02) | **21,00 mm** (+2,02) |
| LiPol LP302024, Höchstmaß | 25,0 | 50,0 | 68,98 mm (−1,02) | 20,00 mm (+1,02) |
| PKCELL LP402025 | 25,0 | 50,0 | 68,98 mm (−1,02) | 20,00 mm (+1,02) |
| PRT-13853 (DTP401525) | 27,0 | 52,0 | 70,98 mm (+0,98) | 18,00 mm (−0,98) |
| *heute verbaut: Adafruit 1317* | *26,02* | *51,02* | *70,00 mm* | *18,98 mm* |

Die letzte Zeile ist die Gegenprobe: Mit dem heute verbauten Maß gibt die
Rechnung genau die heutige Platine von 70 mm und das heutige Antennenfeld von
18,98 mm. Die Rechnung stimmt also mit dem Layout überein.

**Was das heißt:** Die kürzeste passende Zelle spart **4,02 mm Platine** —
nicht mehr. Der Grund steht schon im Auftrag: unter rund 9 mm Zellenlänge
bestimmt die Schaltung das Maß, und 22 mm sind weit davon entfernt. Von den
81,8 mm Gerätelänge sind 4 mm ein Zwanzigstel.

## Schutzschaltung: die belegten Werte

| | YDL 402020 | LiPol LP302024 | DTP301120 / DTP401525 |
| --- | --- | --- | --- |
| Überladung ab | 4,230 / **4,280** / 4,330 V | 4,275 ± 50 mV | 4,200 ± 50 mV |
| Tiefentladung ab | 2,300 / **2,400** / 2,500 V | 2,75 V ± 50 mV | 2,4 ± 0,1 V |
| Wiederfreigabe | 2,900 / **3,000** / 3,100 V | 2,50 V ± 50 mV | 2,80 ± 0,1 V |
| Überstrom | 5 / **6,5** / 9 A | 1 A bis 2,5 A | 2,5 bis 4,5 A |
| Innenwiderstand | max. 65 mΩ | < 200 mΩ (Zelle) | RDS ≤ 60 mΩ |
| **Eigenverbrauch der Schutzplatine** | **1,0 bis 7,0 µA** (Betrieb), max. 1 µA (Power-down) | nicht genannt | nicht genannt |

**Der Eigenverbrauch ist der Haken.** Das Energiebudget des Geräts rechnet mit
**9,5 µA Ruhestrom insgesamt** (frühere Fassung von `docs/power-budget.md`; die Stromzahlen jetzt in `docs/stromaufnahme.md`).
Die YDL-Schutzplatine allein zieht laut eigenem Datenblatt **bis zu 7,0 µA im
Betrieb** — das wäre fast das ganze Ruhebudget. Bei den anderen beiden Zellen
steht dazu **gar nichts** im Datenblatt. Das ist kein Nebensatz: Diese eine
Zahl kann die Laufzeit stärker verändern als die Kapazität.

## Was **nicht** belegt ist

1. **Die Kapazitätswahl steht auf keiner Rechnung, die kleinere Zellen
   abdeckt.** Es gibt zwar ein gerechnetes Energiebudget
   (frühere Fassung von `docs/power-budget.md` vom 23.10.2026), aber es ist **gerechnet, nicht gemessen** — nichts davon lief
   auf Hardware — und es behandelt nur 150 und 100 mAh. Für 40 mAh ist nichts
   gerechnet. **Hier wird bewusst keine Zelle empfohlen**: ohne zu wissen, wie
   lange sie hält, wäre eine Empfehlung dieselbe Art Fehler, die in diesem
   Projekt heute schon mehrfach gefunden wurde. Welche Kapazität die Laufzeit
   wirklich braucht, ist ein eigener Punkt.
2. **Eigenverbrauch der Schutzplatine** bei LP302024 und bei den beiden
   DTP-Zellen: `ungeprueft`, das Datenblatt schweigt.
3. **Kein Maß ist nachgemessen.** Alle Zahlen sind Datenblattzahlen, keine
   Zelle lag auf dem Tisch. Bei Adafruit 1317 weichen Datenblatt (25,0 ± 0,3)
   und Adafruits eigene Produktseite (26,02) voneinander ab — ein Hinweis
   darauf, dass das Fertigmaß in der Praxis am oberen Ende liegt.
4. **Die Breite ist nirgends geprüft worden**, ob 20 mm Zelle in den 24 mm
   nutzbarer Breite tatsächlich sitzen: Kleberaum, Gehäuserippen und das
   Antennenfeld sind in diese Rechnung nicht eingegangen. `ungeprueft`.
5. **Lieferbarkeit in Europa**: nur teilweise belegt (unten), für LP302024 und
   YDL 402020 gar nicht.
6. **Die Rechnung oben unterstellt, dass das Zellenfeld bei längs 25,0 beginnt
   und die Zelle bündig daran anschließt.** Das ist die Vorgabe aus dem
   Auftrag, nicht eine Messung am Layout dieser Sitzung.

## Händler, Lagerbestand, Lieferzeit

| Zelle | Händler | Bestand / Preis | Lieferzeit |
| --- | --- | --- | --- |
| SparkFun PRT-13852 (40 mAh) | SparkFun direkt | **auf Lager**, 7,14 $ (ab 25 Stück 6,78 $) | SparkFun versendet Akkus **nicht international** und nicht per Express — für Deutschland so **nicht bestellbar** |
| SparkFun PRT-13852 | Digi-Key DE | **„nicht mehr verfügbar"** | — |
| YDL 402020 (100 mAh) | YDL (Hersteller) direkt, Dongguan/China | 2,13 $, ab 4 Stück Mindestmenge, 1,60 $ ab 10 Stück | **nicht angegeben**; ob nach Deutschland geliefert wird, steht nicht auf der Seite — `ungeprueft` |
| LiPol LP302024 (100 mAh) | LiPol Battery Co. (Hersteller) direkt | nicht angegeben, Anfrage nötig | `ungeprueft` |
| Adafruit 1317 (150 mAh, verbaut) | Adafruit, BerryBase | siehe `docs/parts.md` | siehe `docs/parts.md` |

Es wurde **nichts bestellt und niemand angefragt** — Hausregel.

**Offen und wichtig:** Für die beiden Zellen, die wirklich passen (YDL 402020,
LiPol LP302024), gibt es **keinen belegten europäischen Händler**. Beide kommen
direkt vom chinesischen Hersteller. Das ist für ein Gerät, das gebaut werden
soll, ein eigenes Risiko und sollte vor jeder Entscheidung geklärt werden.

## Quellen

| Zahl | Quelle | Datum |
| --- | --- | --- |
| DTP301120: 3,0 × 11,0 × 20,0 nackt; **22 × 11,5 × 3,2 mit PCM**; 40 mAh; DW01+/8205; JST-PHR-2PIN | Data Power Technology Limited, „Product Specifications", File No. E-SPE-1106-01, Ver. 1.0, Abschn. 3, 4, 9.1, 9.2 und 9.5 (Seite 9). Verlinkt von der SparkFun-Produktseite PRT-13852 als Datenblatt. | Dokument 06.11.2015, abgerufen 25.10.2026 |
| DTP401525: 4,0 × 15,0 × 25,0 nackt; **27 × 15,5 × 4,2 mit PCM**; 110 mAh | Data Power Technology Limited, File No. E-SPE-1106-01, Ver. 1.0, Abschn. 3, 4 und 9.5 (Seite 9). Verlinkt von der SparkFun-Produktseite PRT-13853. | Dokument 06.11.2015, abgerufen 25.10.2026 |
| YDL 402020: Pack max. 22 × 20 × 4 mm, 100 mAh, JST PHR-2, PCM-Werte, Eigenverbrauch 1–7 µA | Dongguan Youdalipo New Energy Technology Co., Ltd, „Li-ion Polymer Battery Specification", Datei YDL-JS-SP-402020, Rev. A0, Abschn. 6 (Pack-Zeichnung), 7 (Stückliste) und 8.1 (PCB-Werte) | Rev. A0 ohne Datum; abgerufen 25.10.2026 |
| LiPol LP302024: 24 ± 1,0 × 20 ± 0,5 × 3,0 ± 0,3 mm mit PCM, 100 mAh min./110 typ., AWG28-Litzen, Schutzwerte | LiPol Battery Co., Ltd, Zeichnung FD_2024_30, Index 1, „Specification LP302024 100mAh 3.7V with Protection Circuit Module (PCM)" | Zeichnung 12.05.2021, abgerufen 25.10.2026 |
| PKCELL LP402025: 25 ± 0,3 × 20 ± 0,3 × 4,0 ± 0,3 mit PCM, 150 mAh nom./142 min., S-8261AAJMD + 8814, JST PH-2P | Shenzhen PKCELL Battery Co., Ltd, „Li-Polymer Battery Technology Specification 402025 150mAh 3.7V **with PCM**", Dokument QA.S.0228, Edition A, Form 1 und Abschn. 11 (Pack-Zeichnung) | gültig ab 28.07.2014, abgerufen 25.10.2026 |
| Adafruit 1317: 19,75 × 26,02 × 3,8 mm | Adafruit-Produktseite 1317 (Händlerangabe, **nicht** Datenblatt — steht hier nur als Gegenprobe) | abgerufen 25.10.2026 |
| Tagesverbrauch 0,577 mAh, Ruhestrom 9,5 µA, 208 Tage | früherer Stand von `docs/power-budget.md` im eigenen Repo | 23.10.2026 |

Die vier Datenblatt-PDFs lagen zum Lesen im Zwischenspeicher dieser Sitzung und
sind **nicht** ins Repo gelegt worden; die Fundstellen oben sind so genau
angegeben, dass jede Zahl nachgeschlagen werden kann.

## Geprüft, aber nicht in die Tabelle gekommen

- **EEMB LP302024 / LP382024 / LP402024**: die einzigen drei Modelle im
  EEMB-Übersichtsblatt, deren Zahlencode auf ≤ 4,1 mm Dicke, ≤ 24 mm Breite und
  < 25 mm Länge führt. **Kein einziges Datenblatt war erreichbar** (eemb.com
  liefert für die Download-Pfade 404). Damit gibt es weder Kapazität noch
  Fertigmaß noch die Frage, ob es sie überhaupt mit PCM gibt: `ungeprueft`.
  Die LP302024 in der Tabelle oben ist die **gleichnamige Zelle von LiPol**,
  nicht die von EEMB.
- **Jauch LP341018JS** (33 mAh) und **LP501218JH** (60 mAh): klein genug,
  aber die Maße standen nur in Händlerlisten; kein Jauch-Datenblatt gefunden.
  `ungeprueft`.
- **Jauch LP851719JU**: 8,5 mm dick — mehr als doppelt so dick wie erlaubt.
- **EEMB LP451124** (65 mAh): 4,5 mm dick, also über der Grenze von 4,05 mm.
