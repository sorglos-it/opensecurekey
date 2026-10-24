# -*- coding: utf-8 -*-
"""Das Symbol des Projekts zeichnen - nach dem CI (claude-md/CI.md).

    python tools/logo.py                 schreibt apps/web/assets/icon-128.png
    python tools/logo.py --out <datei>   an einen anderen Platz
    python tools/logo.py --size 256      in einer anderen Groesse

Ein flaechiges Zeichen in #191919 auf Weiss, das Farbspektrum laeuft durch
den unteren Teil und schneidet die Form; oben bleibt sie dunkel. Gezeichnet
wird vierfach und dann mit LANCZOS verkleinert - sonst werden die Kanten
treppig.

Motiv: ein aufrechter Schluessel. Er zeigt die Funktion, ist auch bei 57 px
noch zu erkennen und ist keine fremde Marke. Aufrecht, damit der Kopf im
dunklen Teil steht und das Spektrum nur durch den Bart laeuft.

Braucht Pillow und die Spektrum-Vorlage aus den CI-Dateien. Fehlt eines von
beiden, sagt das Skript es und bricht ab - es zeichnet nie ein Ersatzbild.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import common

# Vierfach zeichnen, dann verkleinern.
SCALE = 4
DARK = (0x19, 0x19, 0x19, 255)

# Die Spektrum-Vorlage aus den CI-Dateien. Sie liegt ausserhalb des Projekts,
# weil sie fuer alle Projekte dieselbe ist.
SPECTRUM = os.path.join(os.path.expanduser('~'), 'Documents', 'Sorglos.Org',
                        '_vorlagen', 'expectrum-1191724.png')

DEFAULT_OUT = os.path.join('apps', 'web', 'assets', 'icon-128.png')


def draw_key(size, image_module, draw_module):
    """Die Form: runder Kopf oben mit Loch, Schaft nach unten, zwei Zaehne."""
    s = SCALE
    mask = image_module.new('L', (size, size), 0)
    pen = draw_module.Draw(mask)
    pen.ellipse([74 * s, 24 * s, 182 * s, 132 * s], fill=255)
    # Das Loch wird als Weiss aus der Flaeche geschnitten, nicht daraufgemalt.
    pen.ellipse([108 * s, 58 * s, 148 * s, 98 * s], fill=0)
    pen.rounded_rectangle([114 * s, 120 * s, 142 * s, 230 * s],
                          radius=8 * s, fill=255)
    pen.rounded_rectangle([140 * s, 168 * s, 190 * s, 186 * s],
                          radius=6 * s, fill=255)
    pen.rounded_rectangle([140 * s, 202 * s, 174 * s, 220 * s],
                          radius=6 * s, fill=255)
    return mask


def fill(mask, size, image_module):
    """Flaeche dunkel fuellen, Spektrum darueberlegen, auf Weiss setzen."""
    box = mask.getbbox()
    width, height = box[2] - box[0], box[3] - box[1]
    spectrum = image_module.open(SPECTRUM).convert('RGBA')
    # Auf Breite der Form und 150 % ihrer Hoehe, 8 % nach oben versetzt:
    # so bleibt der obere Teil der Form dunkel.
    spectrum = spectrum.resize((width, int(height * 1.5)))
    layer = image_module.new('RGBA', (size, size), DARK)
    layer.alpha_composite(spectrum, (box[0], box[1] - int(height * 0.08)))
    out = image_module.new('RGBA', (size, size), (255, 255, 255, 255))
    out.paste(layer, (0, 0), mask)
    return out


def build(target, size):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        common.say('FEHLER: Pillow fehlt. Installieren mit: pip install Pillow')
        return 1
    if not os.path.isfile(SPECTRUM):
        common.say('FEHLER: die Spektrum-Vorlage fehlt: %s' % SPECTRUM)
        common.say('Sie gehoert zu den CI-Dateien, siehe claude-md/CI.md.')
        return 1

    big = 256 * SCALE
    mask = draw_key(big, Image, ImageDraw)
    image = fill(mask, big, Image).resize((size, size), Image.LANCZOS)
    image = image.convert('RGB')
    folder = os.path.dirname(os.path.abspath(target))
    if folder:
        os.makedirs(folder, exist_ok=True)
    image.save(target, 'PNG', optimize=True)
    common.say('%s: %d x %d, %d Bytes'
               % (target, image.width, image.height, os.path.getsize(target)))

    # Gegenprobe: bei 57 px muss noch etwas zu erkennen sein. Ein Symbol,
    # das dort zu einer Flaeche wird, ist keines - und das faellt sonst
    # erst auf, wenn es in einer Leiste steht.
    small = image.resize((57, 57), Image.LANCZOS)
    colours = small.getcolors(57 * 57) or []
    common.say('bei 57 px: %d verschiedene Farben' % len(colours))
    if len(colours) < 20:
        common.say('FEHLER: bei 57 px ist nichts mehr zu erkennen')
        return 1
    return 0


def main(argv=None, root=common.ROOT):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--out', help='Zieldatei (Vorgabe: ' + DEFAULT_OUT + ')')
    parser.add_argument('--size', type=int, default=128,
                        help='Kantenlaenge in Pixeln (Vorgabe: 128)')
    args = parser.parse_args(argv)
    if args.size < 16 or args.size > 1024:
        common.say('FEHLER: die Groesse muss zwischen 16 und 1024 liegen')
        return 1
    target = args.out or os.path.join(root, DEFAULT_OUT)
    return build(target, args.size)


if __name__ == '__main__':
    sys.exit(main())
