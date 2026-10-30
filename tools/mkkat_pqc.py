# -*- coding: utf-8 -*-
"""Macht aus den amtlichen ACVP-Dateien des NIST den Pruefkopf kat_mlkem768.h.

    python tools/mkkat_pqc.py <acvp-ordner> apps/firmware/test/pqc/kat_mlkem768.h

Im <acvp-ordner> liegen die beiden Dateien aus usnistgov/ACVP-Server:

    gen-val/json-files/ML-KEM-keyGen-FIPS203/internalProjection.json
    gen-val/json-files/ML-KEM-encapDecap-FIPS203/internalProjection.json

Was mitgenommen wird und warum nur das: die vollen Schluessel sind lang
(ein dk hat 2400 Byte). Bei der Schluesselerzeugung reicht der SHA3-256
ueber das amtliche Ergebnis - verglichen wird dann Hash gegen Hash, und
das ist genauso scharf. Wo das amtliche Ergebnis Eingabe ist (Kapseln,
Entkapseln), muss es vollstaendig dabei sein.

Die Dateien selbst liegen nicht im Repo: sie sind zusammen 2,6 MB, und
das Erzeugen ist mit diesem Werkzeug jederzeit nachvollziehbar.
"""
import hashlib
import io
import json
import os
import subprocess
import sys

SET = 'ML-KEM-768'
HEADER = '''/* Amtliche Testwerte. NICHT von Hand aendern.
 *
 * Herkunft: usnistgov/ACVP-Server, gen-val/json-files/
 *   ML-KEM-keyGen-FIPS203/internalProjection.json
 *   ML-KEM-encapDecap-FIPS203/internalProjection.json
 * Geholt am 30.10.2026, Satz ML-KEM-768. Erzeugt von tools/mkkat_pqc.py.
 *
 * Das ist der Punkt, an dem diese Umsetzung gegen etwas anderes als sich
 * selbst geprueft wird. Ein Rundlauf "gekapselt und wieder entkapselt"
 * geht auch mit einer falschen Umsetzung auf, solange beide Seiten
 * denselben Fehler machen. Hier stehen Werte, die eine fremde Umsetzung
 * erzeugt hat, gegen die das NIST seine Pruefstelle fahren laesst.
 *
 * Bei der Schluesselerzeugung steht der SHA3-256 ueber dem amtlichen
 * Ergebnis statt des Ergebnisses selbst - ein dk hat 2400 Byte, und
 * Hash gegen Hash ist genauso scharf. Wo das amtliche Ergebnis Eingabe
 * ist, steht es vollstaendig da.
 *
 * Die Keccak-Werte unten kommen nicht vom NIST, sondern aus hashlib
 * (CPython) - auch das eine fremde Umsetzung und damit eine echte
 * Gegenprobe, nur keine amtliche.
 */
#ifndef KAT_MLKEM768_H
#define KAT_MLKEM768_H

struct kat_keygen {
\tconst char *d;
\tconst char *z;
\tconst char *ek_sha3;
\tconst char *dk_sha3;
};

struct kat_encaps {
\tconst char *ek;
\tconst char *m;
\tconst char *c_sha3;
\tconst char *k;
};

struct kat_decaps {
\tconst char *dk;
\tconst char *c;
\tconst char *k;
\tconst char *reason;
};

struct kat_keccak {
\tconst char *msg;
\tconst char *sha3_256;
\tconst char *sha3_512;
\tconst char *shake128_64;
\tconst char *shake256_64;
};

'''


def h(data_hex):
    return hashlib.sha3_256(bytes.fromhex(data_hex)).hexdigest()


def lit(value):
    """Langer Hexwert als aneinandergereihte Zeichenketten, damit keine
    Zeile ueber die 100 Spalten des Projektstils hinausgeht."""
    value = value.lower()
    parts = [value[i:i + 64] for i in range(0, len(value), 64)] or ['']
    return ('\n\t  ').join('"%s"' % p for p in parts)


def group(doc, **want):
    for g in doc['testGroups']:
        if all(g.get(k) == v for k, v in want.items()):
            return g
    raise SystemExit('Gruppe fehlt: %r' % (want,))


def load(src, name):
    path = os.path.join(src, 'ML-KEM-%s-FIPS203.internalProjection.json' % name)
    if not os.path.isfile(path):
        path = os.path.join(src, name, 'internalProjection.json')
    with io.open(path, encoding='utf-8') as handle:
        return json.load(handle)


def keccak_cases():
    """Gegenprobe fuer Keccak aus hashlib. Die Laengen sind mit Absicht
    um die Blockgroessen herum gewaehlt (SHA3-256 nimmt 136 Byte je
    Block): genau dort sitzen die Fehler im Abschluss."""
    out = []
    for n in (0, 1, 7, 71, 135, 136, 137, 200):
        msg = bytes((i * 7 + 1) & 0xff for i in range(n))
        out.append((msg.hex(),
                    hashlib.sha3_256(msg).hexdigest(),
                    hashlib.sha3_512(msg).hexdigest(),
                    hashlib.shake_128(msg).hexdigest(64),
                    hashlib.shake_256(msg).hexdigest(64)))
    return out


def table(w, decl, rows):
    w.write('static const struct %s = {\n' % decl)
    for row in rows:
        w.write('\t{ %s },\n' % (',\n\t  '.join(row)))
    w.write('};\n\n')


def main():
    src, dest = sys.argv[1], sys.argv[2]
    kg = load(src, 'keyGen')
    ed = load(src, 'encapDecap')

    keygen = group(kg, parameterSet=SET)['tests']
    encaps = group(ed, parameterSet=SET, function='encapsulation')['tests'][:3]
    decaps = group(ed, parameterSet=SET, function='decapsulation')['tests']

    # Entkapseln: beide Sorten muessen dabei sein. "modified ciphertext"
    # prueft die stille Verwerfung - der Weg, auf dem ein Fehler kein
    # Fehler sein darf, sondern ein anderes Geheimnis.
    chosen = ([t for t in decaps if 'valid' in t['reason']][:2] +
              [t for t in decaps if 'valid' not in t['reason']][:2])

    with io.open(dest, 'w', encoding='utf-8', newline='\n') as w:
        w.write(HEADER)
        table(w, 'kat_keygen k_kat_keygen[]',
              [(lit(t['d']), lit(t['z']), lit(h(t['ek'])), lit(h(t['dk']))) for t in keygen])
        table(w, 'kat_encaps k_kat_encaps[]',
              [(lit(t['ek']), lit(t['m']), lit(h(t['c'])), lit(t['k'])) for t in encaps])
        table(w, 'kat_decaps k_kat_decaps[]',
              [(lit(t['dk']), lit(t['c']), lit(t['k']), '"%s"' % t['reason']) for t in chosen])
        table(w, 'kat_keccak k_kat_keccak[]',
              [(lit(a), lit(b), lit(c), lit(d), lit(e)) for a, b, c, d, e in keccak_cases()])
        w.write('#endif\n')

    # Zum Schluss durch clang-format, sonst meldet tools/check.py die
    # erzeugte Datei als nicht formatiert - und ein zweiter Lauf dieses
    # Werkzeugs gaebe eine andere Datei als die eingecheckte.
    try:
        subprocess.check_call(['clang-format', '-i', dest])
    except (OSError, subprocess.CalledProcessError):
        print('Hinweis: clang-format fehlt - die Datei ist nicht formatiert.')

    print('keygen %d, encaps %d, decaps %d (%s), keccak %d'
          % (len(keygen), len(encaps), len(chosen),
             '; '.join(t['reason'] for t in chosen), len(keccak_cases())))


main()
