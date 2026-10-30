# -*- coding: utf-8 -*-
"""Der Beleg: ein FREMDES Programm liest die Zertifikatsanfrage und
bestaetigt ihre Unterschrift.

    make openssl
    python3 csr_openssl.py --tool build/tool_csr --out build/csr

WARUM DAS NOETIG IST. Am 29.10.2026 hat sich dieses Projekt an einer
selbstgeschriebenen Attrappe verletzt: sie fuehrte einen Zaehler anders
als die echte Gegenseite, alles war gruen, und am Geraet waere gar nicht
unterschrieben worden. Eine Anfrage, die nur das eigene Leseprogramm
versteht, beweist genau nichts. Darum liest hier openssl - ein Programm,
das nichts von diesem Projekt weiss.

WIE DER ABLAUF GEHT, und warum er zwei Schritte hat: unterschrieben wird
im Sicherheitschip, und der ist nicht da. Also:

  1. openssl erzeugt ein Schluesselpaar und gibt den oeffentlichen Teil
     heraus.
  2. Die FIRMWARE (tool_csr, derselbe Code wie am Geraet) baut mit diesem
     oeffentlichen Teil den unterschriebenen Teil der Anfrage.
  3. openssl unterschreibt GENAU diese Bytes mit dem privaten Teil.
  4. Die FIRMWARE setzt die Anfrage zusammen und schreibt DER und PEM.
  5. openssl liest die fertige Anfrage und bestaetigt die Unterschrift.

WAS DAMIT BEWIESEN IST: der Aufbau nach RFC 2986 stimmt, die
Erweiterungen stehen dort, wo ein fremdes Programm sie findet, und die
Unterschrift geht ueber genau den Teil, der unterschrieben sein muss -
bei beiden Schluesselarten und allen Laengen.

WAS DAMIT NICHT BEWIESEN IST: dass der SE051C2 diese Unterschrift
liefert. Hier unterschreibt openssl an seiner Stelle. Welche Annahmen
ueber den Chip dabei offen bleiben, steht in docs/csr.md.

Nur Standardbibliothek; openssl wird als Programm gerufen."""
import argparse
import os
import shutil
import subprocess
import sys

# Welche Faelle geprueft werden. Die Laenge in Byte steht hier, weil
# tool_csr den Modulus im Datentraeger genau dort enden sieht.
CASES = (
    ('p256', 'EC', 'prime256v1', 0),
    ('p384', 'EC', 'secp384r1', 0),
    ('rsa2048', 'RSA', '2048', 256),
    ('rsa3072', 'RSA', '3072', 384),
    ('rsa4096', 'RSA', '4096', 512),
)

# Was in der Ausgabe von `openssl req -text` stehen MUSS. Jede Zeile ist
# ein Stueck, das die Firmware gebaut hat - gefunden von einem fremden
# Leser und nicht vom eigenen.
WANT_TEXT = (
    # Der ganze Betreff in einer Zeile, und zwar in DIESER Reihenfolge:
    # von allgemein nach besonders. Dass openssl ihn so ausgibt, zeigt,
    # dass die Namensteile in der richtigen Folge im DER stehen.
    'Subject: C=DE, O=Sorglos, OU=openSecureKey PIV 9A, '
    'CN=key.example.org, serialNumber=0123456789ABCDEF',
    'self-signature verify OK',
    'X509v3 Subject Alternative Name',
    'DNS:key.example.org',
    'DNS:zweiter.example.org',
    'email:thomas@example.org',
    'URI:https://example.org/key',
    'X509v3 Key Usage: critical',
    'Digital Signature',
    'Key Encipherment',
    'X509v3 Extended Key Usage',
    'TLS Web Client Authentication',
    'E-mail Protection',
    'Requested Extensions',
)


def run(args, **kw):
    """Ein Programm rufen und Ausgabe samt Fehlerausgabe zurueckgeben.
    openssl schreibt die Bestaetigung der Unterschrift auf die
    Fehlerausgabe - wer nur die normale Ausgabe liest, sieht sie nie."""
    p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kw)
    return p.returncode, p.stdout.decode('utf-8', 'replace')


# --- ein winziger DER-Leser, nur fuer die Schluessel von openssl ---------

def der_next(buf, at):
    """Liefert (tag, wert, naechste_stelle). Laengen kurz, 81, 82, 83."""
    tag = buf[at]
    n = buf[at + 1]
    hdr = 2
    if n & 0x80:
        count = n & 0x7f
        if count == 0 or count > 3:
            raise ValueError('Laengenfeld mit %d Byte' % count)
        n = int.from_bytes(buf[at + 2:at + 2 + count], 'big')
        hdr = 2 + count
    return tag, buf[at + hdr:at + hdr + n], at + hdr + n


def spki_bitstring(der):
    """Der oeffentliche Teil aus einem SubjectPublicKeyInfo: der Inhalt
    des BIT STRING, ohne das Byte fuer die ungenutzten Bits."""
    _, body, _ = der_next(der, 0)
    _, _, at = der_next(body, 0)          # AlgorithmIdentifier ueberspringen
    tag, bits, _ = der_next(body, at)
    if tag != 0x03:
        raise ValueError('kein BIT STRING im SubjectPublicKeyInfo')
    return bits[1:]


def rsa_parts(der, mod_bytes):
    """Modulus und Exponent aus einem RSA-SubjectPublicKeyInfo."""
    inner = spki_bitstring(der)
    _, seq, _ = der_next(inner, 0)
    tag, mod, at = der_next(seq, 0)
    if tag != 0x02:
        raise ValueError('kein INTEGER fuer den Modulus')
    mod = mod.lstrip(b'\x00')
    if len(mod) != mod_bytes:
        raise ValueError('Modulus ist %d Byte, erwartet %d' % (len(mod), mod_bytes))
    tag, exp, _ = der_next(seq, at)
    if tag != 0x02:
        raise ValueError('kein INTEGER fuer den Exponenten')
    return mod, exp.lstrip(b'\x00')


# --- ein Fall ------------------------------------------------------------

def one_case(tool, out_dir, spec, kind, param, mod_bytes):
    key = os.path.join(out_dir, spec + '-key.pem')
    pub = os.path.join(out_dir, spec + '-pub.bin')
    tbs = os.path.join(out_dir, spec + '.tbs')
    sig = os.path.join(out_dir, spec + '.sig')
    der = os.path.join(out_dir, spec + '.der')
    pem = os.path.join(out_dir, spec + '.pem')
    fails = []

    # 1. Schluessel von openssl. Der private Teil bleibt hier und steht
    #    nie in der Anfrage - genau wie im Chip, nur dass er hier
    #    ueberhaupt existiert.
    if kind == 'EC':
        code, out = run(['openssl', 'genpkey', '-algorithm', 'EC', '-pkeyopt',
                         'ec_paramgen_curve:' + param, '-out', key])
    else:
        code, out = run(['openssl', 'genpkey', '-algorithm', 'RSA', '-pkeyopt',
                         'rsa_keygen_bits:' + param, '-out', key])
    if code != 0:
        return ['%s: openssl erzeugt keinen Schluessel\n%s' % (spec, out)]

    code, out = run(['openssl', 'pkey', '-in', key, '-pubout', '-outform', 'DER',
                     '-out', pub + '.spki'])
    if code != 0:
        return ['%s: oeffentlicher Teil kommt nicht heraus\n%s' % (spec, out)]
    with open(pub + '.spki', 'rb') as handle:
        spki = handle.read()
    if kind == 'EC':
        data = spki_bitstring(spki)
    else:
        mod, exp = rsa_parts(spki, mod_bytes)
        data = mod + exp
    with open(pub, 'wb') as handle:
        handle.write(data)

    # 2. Die Firmware baut den unterschriebenen Teil.
    code, out = run([tool, 'info', spec, pub, tbs])
    if code == 3:
        return ['UEBERSPRUNGEN %s: diese Fassung ist ohne RSA gebaut' % spec]
    if code != 0:
        return ['%s: tool_csr info scheitert\n%s' % (spec, out)]

    # 3. openssl unterschreibt GENAU diese Bytes. Nicht die ganze
    #    Anfrage, nicht einen Teil davon.
    code, out = run(['openssl', 'dgst', '-sha256', '-sign', key, '-out', sig, tbs])
    if code != 0:
        return ['%s: openssl unterschreibt nicht\n%s' % (spec, out)]

    # 4. Die Firmware setzt die Anfrage zusammen.
    code, out = run([tool, 'der', spec, pub, sig, der, pem])
    if code != 0:
        return ['%s: tool_csr der scheitert\n%s' % (spec, out)]

    # 5. Und nun liest ein fremdes Programm. ZWEIMAL: als DER und als
    #    PEM. Dass beide Schreibweisen dieselbe Anfrage sind, prueft der
    #    Pruefsatz in C; dass BEIDE von openssl angenommen werden, muss
    #    openssl sagen.
    code, text = run(['openssl', 'req', '-in', der, '-inform', 'DER', '-verify',
                      '-noout', '-text'])
    if code != 0:
        fails.append('%s: openssl liest das DER nicht\n%s' % (spec, text))
    elif 'verify OK' not in text:
        fails.append('%s: openssl bestaetigt die Unterschrift NICHT\n%s' % (spec, text))
    if 'verify failure' in text:
        fails.append('%s: openssl meldet eine falsche Unterschrift\n%s' % (spec, text))

    code, pem_text = run(['openssl', 'req', '-in', pem, '-verify', '-noout'])
    if code != 0 or 'verify OK' not in pem_text:
        fails.append('%s: das PEM wird nicht angenommen\n%s' % (spec, pem_text))

    for needle in WANT_TEXT:
        if needle not in text:
            fails.append('%s: openssl findet "%s" nicht' % (spec, needle))

    want_alg = 'ecdsa-with-SHA256' if kind == 'EC' else 'sha256WithRSAEncryption'
    if want_alg not in text:
        fails.append('%s: das Unterschriftsverfahren ist nicht %s' % (spec, want_alg))

    if not fails:
        print('  %-8s gelesen und Unterschrift bestaetigt (%s)' % (spec, want_alg))
    return fails


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--tool', required=True, help='gebautes tool_csr')
    ap.add_argument('--out', default='build/csr', help='Ordner fuer die Dateien')
    args = ap.parse_args(argv)

    if shutil.which('openssl') is None:
        print('openssl fehlt - DIESER BELEG IST DAMIT NICHT ERBRACHT.')
        return 1
    if not os.path.isfile(args.tool):
        print('%s fehlt - zuerst `make openssl` oder `make build/tool_csr`.' % args.tool)
        return 1

    code, version = run(['openssl', 'version'])
    print('Beleg mit einem fremden Programm: %s' % version.strip())
    os.makedirs(args.out, exist_ok=True)

    fails = []
    skipped = []
    for spec, kind, param, mod_bytes in CASES:
        got = one_case(args.tool, args.out, spec, kind, param, mod_bytes)
        for line in got:
            if line.startswith('UEBERSPRUNGEN'):
                skipped.append(line)
                print('  ' + line)
            else:
                fails.append(line)

    print('')
    for line in fails:
        print('FEHLER ' + line)
    done = len(CASES) - len(skipped)
    if fails:
        print('BELEG ROT: %d Beanstandungen bei %d Faellen.' % (len(fails), done))
        return 1
    if done == 0:
        print('Kein Fall gelaufen - das ist KEIN Beleg.')
        return 1
    print('Beleg erbracht: %d von %d Faellen von openssl gelesen und die '
          'Unterschrift bestaetigt.' % (done, len(CASES)))
    if skipped:
        print('Uebersprungen: %d (siehe oben).' % len(skipped))
    print('Nicht belegt: dass der SE051C2 diese Unterschrift liefert - der Chip')
    print('ist nicht da. Siehe docs/csr.md.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
