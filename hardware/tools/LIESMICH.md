# Werkzeuge zum Platinen-Strang opensecurekey

Messskripte, die beim Zeichnen der Platine entstanden sind. Sie lagen bis zum
29.10.2026 außerhalb des Repos in `D:\claude\working\opensecurekey-platine-werkzeuge`
und sind dort beim Aufräumen gefunden worden — der Zweig `platine`, auf dem sie
entstanden, ist längst in `main` aufgegangen und gelöscht. **Die Skripte waren
nirgends eingecheckt.** Darum liegen sie jetzt hier: sie sind die Messmittel,
mit denen die Platine geprüft wurde, und die Regeln darunter stehen sonst nur
als Prosa im Plan.

Sie sprechen die EasyEDA-Brücke und Gerber-Exporte an und laufen deshalb nur
mit laufendem EasyEDA Pro beziehungsweise mit einem Export zur Hand. Geprüft
ist hier nur, dass alle acht übersetzbar sind — **gelaufen sind sie zuletzt am
25.10.2026**, und `pcb_*`-Werkzeugnamen können sich seitdem geändert haben.

| Datei | wofür |
| --- | --- |
| `gerber-kupfer.py` | **Liegt an einem Punkt Kupfer?** Beantwortet am Gerber-Export, Lage für Lage. Das zuverlässigste Messmittel — die Vorschau kennt keine Lagenauswahl. |
| `pruef.py` | Pad gegen Pad aus `pcb_pads_list`: alle Paare unter der Vorschrift, Randabstände, fremde Pads in der Freihaltezone. MULTI zählt auf beiden Lagen. |
| `kond.py` | Abstand jedes Kondensators zu dem Pin, den er abblockt. |
| `simkond.py`, `sim2.py` | Verschiebungen **vor** dem Senden durchrechnen. |
| `u1.py`, `u1b.py`, `u1c.py` | Hülle des Funkmoduls aus seinen 61 Pads, daraus Körper und Freihaltezone. |

**Zwei Regeln, die in jedem dieser Skripte stecken:**

1. **Erst den Test an Kontrollpunkten prüfen, deren Antwort vorher
   feststeht** — und zwar in beide Richtungen, eine muss „frei" ergeben und
   eine „Kupfer". Eine Prüfung, die man nicht zum Anschlagen gebracht hat,
   ist keine.
2. **`pcb_layout_boxes` misst Pad-Mitten, nicht Kupfer.** Für Abstände immer
   `pcb_pads_list` mit `extent_mm` nehmen — das ist das Maß *nach* der
   Drehung.
