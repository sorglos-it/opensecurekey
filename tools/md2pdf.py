"""Markdown -> PDF fuer den openSecureKey-Plan.

Aufruf:
    python tools/md2pdf.py docs/plan.md docs/plan.pdf "openSecureKey - Plan"

Braucht reportlab (pip install reportlab). Das ist die einzige
Abhaengigkeit, und sie gilt nur fuer dieses Werkzeug, nicht fuer das
Geraet - die Hausregel "Software ohne Abhaengigkeiten" betrifft das
Produkt, nicht die Werkzeugkette.

Drei Dinge, an denen eine Markdown-nach-PDF-Umwandlung erfahrungsgemaess
scheitert, und wie sie hier geloest sind:

1. Deutsche Umlaute und Sonderzeichen (>=, ->, Haken) koennen die
   eingebauten reportlab-Schriften NICHT - sie erscheinen als schwarze
   Kaesten. Darum wird eine echte TrueType-Schrift eingebettet
   (Arial aus C:/Windows/Fonts). Auf einem anderen System ist FONTS
   anzupassen.
2. Mehrzeilige Listenpunkte: Folgezeilen gehoeren zum Punkt, sonst
   beginnt fuer jede ein eigener Absatz und der Text bricht aus der
   Einrueckung aus. Siehe das Einsammeln in der Listen-Verzweigung.
3. Tabellenzellen brauchen Paragraph-Objekte, sonst laufen lange Zellen
   ueber den Seitenrand hinaus statt umzubrechen.

Geprueft am 26.10.2026 an docs/plan.md: 27 Seiten, Umlaute, Pfeile und
Tabellenumbrueche an drei Stellen im erzeugten PDF nachgesehen.
"""
import re
import sys
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    HRFlowable, KeepTogether, PageBreak, Paragraph, SimpleDocTemplate,
    Spacer, Table, TableStyle,
)

FONTS = Path("C:/Windows/Fonts")
BASE, BOLD, ITAL, MONO = "Doc", "Doc-B", "Doc-I", "DocMono"


def register_fonts():
    pairs = [
        (BASE, "arial.ttf"), (BOLD, "arialbd.ttf"),
        (ITAL, "ariali.ttf"), (MONO, "consola.ttf"),
    ]
    for name, file in pairs:
        path = FONTS / file
        if not path.exists():
            raise SystemExit(f"Schrift fehlt: {path}")
        pdfmetrics.registerFont(TTFont(name, str(path)))
    pdfmetrics.registerFontFamily(BASE, normal=BASE, bold=BOLD, italic=ITAL)


def esc(text):
    return (text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def inline(text):
    """**fett**, *kursiv*, `code`, [text](ziel) -> reportlab-Markup."""
    text = esc(text)
    text = re.sub(r"`([^`]+)`",
                  rf'<font face="{MONO}" size="8.5" color="#8a2a00">\1</font>',
                  text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"(?<![*\w])\*([^*\n]+)\*(?!\w)", r"<i>\1</i>", text)
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    return text


def build_styles():
    ss = getSampleStyleSheet()
    body = ParagraphStyle(
        "Body", parent=ss["Normal"], fontName=BASE, fontSize=9.5,
        leading=13.5, spaceAfter=5, alignment=TA_LEFT,
    )
    st = {
        "body": body,
        "h1": ParagraphStyle("H1", parent=body, fontName=BOLD, fontSize=19,
                             leading=23, spaceBefore=16, spaceAfter=9,
                             textColor=colors.HexColor("#111111")),
        "h2": ParagraphStyle("H2", parent=body, fontName=BOLD, fontSize=14.5,
                             leading=18, spaceBefore=14, spaceAfter=7,
                             textColor=colors.HexColor("#1a3a5c")),
        "h3": ParagraphStyle("H3", parent=body, fontName=BOLD, fontSize=11.5,
                             leading=15, spaceBefore=11, spaceAfter=5,
                             textColor=colors.HexColor("#2a2a2a")),
        "h4": ParagraphStyle("H4", parent=body, fontName=BOLD, fontSize=10,
                             leading=13, spaceBefore=9, spaceAfter=4),
        "li": ParagraphStyle("LI", parent=body, leftIndent=11, bulletIndent=2,
                             spaceAfter=2.5),
        "quote": ParagraphStyle("Q", parent=body, leftIndent=9,
                                rightIndent=4, fontSize=9.5,
                                textColor=colors.HexColor("#333333"),
                                borderPadding=(0, 0, 0, 6)),
        "code": ParagraphStyle("Code", parent=body, fontName=MONO, fontSize=8,
                               leading=10.5, leftIndent=7,
                               backColor=colors.HexColor("#f4f4f4"),
                               borderPadding=4, spaceAfter=7),
        "cell": ParagraphStyle("Cell", parent=body, fontSize=8.2, leading=10.8,
                               spaceAfter=0),
        "cellh": ParagraphStyle("CellH", parent=body, fontName=BOLD,
                                fontSize=8.2, leading=10.8, spaceAfter=0),
    }
    return st


def split_row(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [c.strip() for c in re.split(r"(?<!\\)\|", line)]


def is_sep(line):
    return bool(re.fullmatch(r"\s*\|?[\s:|-]+\|?\s*", line)) and "-" in line


def make_table(rows, st, avail):
    if not rows:
        return None
    ncol = max(len(r) for r in rows)
    rows = [r + [""] * (ncol - len(r)) for r in rows]
    head, body = rows[0], rows[1:]

    data = [[Paragraph(inline(c), st["cellh"]) for c in head]]
    for r in body:
        data.append([Paragraph(inline(c), st["cell"]) for c in r])

    # Spaltenbreiten nach Textmenge, erste Spalte etwas breiter
    weights = []
    for i in range(ncol):
        longest = max((len(r[i]) for r in rows), default=1)
        weights.append(max(longest, 6) ** 0.62)
    total = sum(weights)
    widths = [avail * w / total for w in weights]

    t = Table(data, colWidths=widths, repeatRows=1, hAlign="LEFT")
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e8edf2")),
        ("LINEBELOW", (0, 0), (-1, 0), 0.8, colors.HexColor("#8a9aa8")),
        ("GRID", (0, 0), (-1, -1), 0.3, colors.HexColor("#c8d0d8")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 4),
        ("RIGHTPADDING", (0, 0), (-1, -1), 4),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1),
         [colors.white, colors.HexColor("#fafbfc")]),
    ]))
    return t


def convert(md_path, pdf_path, title):
    register_fonts()
    st = build_styles()

    margin = 18 * mm
    doc = SimpleDocTemplate(
        str(pdf_path), pagesize=A4,
        leftMargin=margin, rightMargin=margin,
        topMargin=16 * mm, bottomMargin=16 * mm,
        title=title, author="Sorglos Thomas Weirich",
    )
    avail = A4[0] - 2 * margin

    story = []
    lines = Path(md_path).read_text(encoding="utf-8").splitlines()
    i, n = 0, len(lines)
    para, tbl, code, quote = [], [], [], []

    def flush_para():
        if para:
            story.append(Paragraph(inline(" ".join(para)), st["body"]))
            para.clear()

    def flush_tbl():
        if tbl:
            t = make_table(tbl, st, avail)
            if t:
                story.append(Spacer(1, 3))
                story.append(t)
                story.append(Spacer(1, 7))
            tbl.clear()

    def flush_code():
        if code:
            txt = "<br/>".join(esc(c).replace(" ", "&nbsp;") for c in code)
            story.append(Paragraph(txt, st["code"]))
            code.clear()

    def flush_quote():
        if quote:
            story.append(Table(
                [[Paragraph(inline(" ".join(quote)), st["quote"])]],
                colWidths=[avail], hAlign="LEFT",
                style=TableStyle([
                    ("LINEBEFORE", (0, 0), (0, -1), 2.2,
                     colors.HexColor("#5a7a9a")),
                    ("BACKGROUND", (0, 0), (-1, -1),
                     colors.HexColor("#f6f8fa")),
                    ("LEFTPADDING", (0, 0), (-1, -1), 8),
                    ("TOPPADDING", (0, 0), (-1, -1), 5),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
                ])))
            story.append(Spacer(1, 6))
            quote.clear()

    def flush_all():
        flush_para(); flush_tbl(); flush_code(); flush_quote()

    in_code = False
    while i < n:
        line = lines[i]
        stripped = line.strip()

        if stripped.startswith("```"):
            if in_code:
                flush_code()
                in_code = False
            else:
                flush_para(); flush_tbl(); flush_quote()
                in_code = True
            i += 1
            continue
        if in_code:
            code.append(line)
            i += 1
            continue

        if stripped.startswith("|") and "|" in stripped[1:]:
            flush_para(); flush_quote()
            if not is_sep(stripped):
                tbl.append(split_row(stripped))
            i += 1
            continue
        flush_tbl()

        if stripped.startswith(">"):
            flush_para()
            q = stripped.lstrip(">").strip()
            if q:
                quote.append(q)
            i += 1
            continue
        flush_quote()

        m = re.match(r"^(#{1,6})\s+(.*)", stripped)
        if m:
            flush_all()
            lvl = min(len(m.group(1)), 4)
            if lvl == 1 and story:
                story.append(PageBreak())
            story.append(Paragraph(inline(m.group(2)), st[f"h{lvl}"]))
            i += 1
            continue

        if re.fullmatch(r"-{3,}|\*{3,}|_{3,}", stripped):
            flush_all()
            story.append(Spacer(1, 4))
            story.append(HRFlowable(width="100%", thickness=0.7,
                                    color=colors.HexColor("#b0bcc8")))
            story.append(Spacer(1, 7))
            i += 1
            continue

        m = re.match(r"^(\s*)([-*+]|\d+\.)\s+(.*)", line)
        if m:
            flush_para()
            depth = len(m.group(1)) // 2
            marker = m.group(2)
            bullet = marker if marker[0].isdigit() else "\u2022"
            parts = [m.group(3)]
            # Fortsetzungszeilen eines Listenpunkts einsammeln: eingerueckt,
            # nicht leer, und kein Anfang eines neuen Blocks. Ohne das
            # beginnt reportlab fuer jede Folgezeile einen eigenen Absatz,
            # und der Text bricht aus der Einrueckung aus.
            j = i + 1
            while j < n:
                nxt = lines[j]
                if not nxt.strip():
                    break
                if re.match(r"^(\s*)([-*+]|\d+\.)\s+", nxt):
                    break
                if re.match(r"^\s*(#{1,6}\s|>|\||```|-{3,}$)", nxt):
                    break
                if not nxt.startswith((" ", "\t")):
                    break
                parts.append(nxt.strip())
                j += 1
            sty = ParagraphStyle(f"li{depth}", parent=st["li"],
                                 leftIndent=11 + depth * 11,
                                 bulletIndent=2 + depth * 11)
            story.append(Paragraph(inline(" ".join(parts)), sty,
                                   bulletText=bullet))
            i = j
            continue

        if not stripped:
            flush_para()
            i += 1
            continue

        para.append(stripped)
        i += 1

    flush_all()

    def footer(canvas, d):
        canvas.saveState()
        canvas.setFont(BASE, 7.5)
        canvas.setFillColor(colors.HexColor("#777777"))
        canvas.drawString(margin, 9 * mm, title)
        canvas.drawRightString(A4[0] - margin, 9 * mm, f"Seite {d.page}")
        canvas.setStrokeColor(colors.HexColor("#d8dde2"))
        canvas.line(margin, 12.5 * mm, A4[0] - margin, 12.5 * mm)
        canvas.restoreState()

    doc.build(story, onFirstPage=footer, onLaterPages=footer)
    return doc.page


if __name__ == "__main__":
    src, dst = sys.argv[1], sys.argv[2]
    name = sys.argv[3] if len(sys.argv) > 3 else "Plan"
    pages = convert(src, dst, name)
    size = Path(dst).stat().st_size
    print(f"{pages} Seiten, {size/1024:.0f} KB -> {dst}")
