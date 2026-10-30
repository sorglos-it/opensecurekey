<?php
// Beispiel in PHP: ein Programm, das nur mit Stick laeuft, eine Datei
// verschliesst, die ohne Stick unlesbar bleibt, seine Lizenz prueft und
// einen Auftrag unterschreibt.
//
//     php example.php --stand ..\stand.json
//
// Jede Zeile, die mit "ergebnis:" anfaengt, liest der Pruefstand mit. Bricht
// das Beispiel ab, ist der Ausstiegscode nicht 0.

declare(strict_types=1);

require __DIR__ . '/OskGuard.php';

function standPath(array $argv): string
{
    for ($i = 1; $i < count($argv); $i++) {
        if ($argv[$i] === '--stand' && isset($argv[$i + 1])) {
            return $argv[$i + 1];
        }
    }
    return 'stand.json';
}

function main(array $argv): int
{
    $path = standPath($argv);
    $raw = @file_get_contents($path);
    if ($raw === false) {
        fwrite(STDERR, "FEHLER: $path nicht lesbar\n");
        return 1;
    }
    $stand = json_decode($raw, true);
    if (!is_array($stand)) {
        fwrite(STDERR, "FEHLER: $path ist kein JSON\n");
        return 1;
    }
    $deviceKey = hex2bin((string) $stand['deviceKey']);
    $issuerKey = hex2bin((string) $stand['issuerKey']);
    if ($deviceKey === false || $issuerKey === false) {
        fwrite(STDERR, "FEHLER: deviceKey oder issuerKey ist kein Hex\n");
        return 1;
    }

    try {
        // 1. Ohne Stick laeuft das Programm nicht los.
        $guard = OskGuard::open((string) $stand['address'], $deviceKey,
            $issuerKey);
        $guard->present(__FILE__);
        echo "ergebnis: praesenz ok\n";

        // 2. Eine Datei, die ohne Stick unlesbar bleibt.
        $label = (string) $stand['label'];
        $plain = 'die Tabelle, ohne die das Programm nichts kann';
        $blob = $guard->seal($label, 'version-1', $plain);
        $back = $guard->unseal($label, 'version-1', $blob);
        if ($back !== $plain) {
            fwrite(STDERR, "FEHLER: heraus kam etwas anderes\n");
            return 1;
        }
        // Mit der falschen Programmversion geht sie nicht auf - das muss so sein.
        try {
            $guard->unseal($label, 'version-2', $blob);
            fwrite(STDERR, "FEHLER: mit falschem Zusatz aufgegangen\n");
            return 1;
        } catch (OskGuardError $expected) {
            // So soll es sein.
        }
        printf("ergebnis: verschlossen %d byte, wieder offen\n", strlen($blob));

        // 2b. Ein verschluesselter PROGRAMMTEIL - der starke Weg: ohne
        // Stick fehlt dem Angreifer nicht die Antwort, sondern der Teil.
        $code = 'die Regeln, nach denen dieses Programm rechnet';
        [$codeBlob, $codeHash] = $guard->sealCode($label, $code);
        if ($guard->unsealCode($label, $codeBlob, $codeHash) !== $code) {
            fwrite(STDERR, "FEHLER: der Programmteil kam anders zurueck\n");
            return 1;
        }
        // Mit einem anderen Hash gibt es KEINE Bytes.
        $other = $codeHash;
        $other[0] = chr(ord($other[0]) ^ 1);
        try {
            $guard->unsealCode($label, $codeBlob, $other);
            fwrite(STDERR, "FEHLER: ein fremder Programmteil ging auf\n");
            return 1;
        } catch (OskGuardError $expected) {
            if ($expected->errorKey() === '') {
                fwrite(STDERR, "FEHLER: kein Fehlerschluessel\n");
                return 1;
            }
        }
        printf("ergebnis: programmteil %d byte, nur mit stick lesbar\n",
            strlen($codeBlob));

        // 3. Die Lizenz.
        $license = $guard->license((string) $stand['product']);
        if ($license->expired()) {
            fwrite(STDERR, "FEHLER: Lizenz abgelaufen\n");
            return 1;
        }
        if (!$license->hasTier('berichte')) {
            fwrite(STDERR, "FEHLER: Ausbaustufe fehlt\n");
            return 1;
        }
        printf("ergebnis: lizenz %s, %d plaetze, stufen %s\n",
            $license->product, $license->seats, implode(' ', $license->tiers));

        // 4. Ein Auftrag, unterschrieben vom Schluessel im Chip.
        $node = (string) $stand['nodeKey'];
        $public = $guard->nodePublic($node, 'ed25519');
        $job = '{"tat":"abbild-holen","wann":1700000000}';
        $signature = $guard->nodeSign($node, $job);
        if (!OskGuard::nodeVerify($public, $node, $job, $signature)) {
            fwrite(STDERR, "FEHLER: die eigene Unterschrift gilt nicht\n");
            return 1;
        }
        $bent = $job;
        $bent[3] = chr(ord($bent[3]) ^ 1);
        if (OskGuard::nodeVerify($public, $node, $bent, $signature)) {
            fwrite(STDERR, "FEHLER: ein veraenderter Auftrag galt\n");
            return 1;
        }
        echo "ergebnis: auftrag unterschrieben und geprueft\n";

        // 5. Der Schluesselaustausch - und der Fall, dass der Chip ihn
        // nicht kann. Dann wird gesagt, was wegfaellt, nicht abgebrochen.
        try {
            $peer = $guard->nodePublic($node, 'x25519');
            $secret = $guard->nodeAgree($node, $peer);
            printf("ergebnis: gemeinsames geheimnis %d byte\n", strlen($secret));
        } catch (OskGuardError $error) {
            if (!$error->isNoEcdh()) {
                throw $error;
            }
            echo "ergebnis: kein schluesselaustausch - "
                . "serverbetrieb mit schluessel im chip faellt weg\n";
        }

        // 6. Noch eine Runde, damit sichtbar ist, dass weiter gefragt wird.
        $guard->present(__FILE__);
        echo "ergebnis: ueberwachung laeuft\n";
    } catch (OskGuardError $error) {
        fwrite(STDERR, 'FEHLER: ' . $error->errorKey() . "\n");
        return 1;
    }
    return 0;
}

exit(main($argv));
