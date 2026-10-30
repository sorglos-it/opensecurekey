<?php
// Pruefsatz der PHP-Fassung. Kein Testrahmen aus dem Netz - nur PHP selbst.
//
//     php -d extension=sodium test_guard.php
//
// Geprueft wird vor allem der selbst geschriebene CBOR-Leser: seine Regeln
// (kuerzeste Schreibweise, keine angehaengten Bytes, kein Schluessel
// zweimal) liessen sich ohne fertig unterschriebene Pruefwerte nicht
// messen, weil die Unterschrift VOR dem Lesen geprueft wird.

declare(strict_types=1);

require_once __DIR__ . '/OskGuard.php';

// Mindestzahlen: findet der Satz weniger, ist er selbst kaputt und nicht der
// Code. Null gelesene Faelle waeren sonst ein gruener Lauf.
const WANT_CASES = 24;
const WANT_VALID = 3;

$errors = 0;
$checked = 0;

function fail(string $text): void
{
    global $errors;
    $errors++;
    fwrite(STDERR, 'FEHLER: ' . $text . "\n");
}

function want(string $what, $got, $expected): void
{
    if ($got !== $expected) {
        fail(sprintf('%s ist %s, erwartet %s', $what,
            var_export($got, true), var_export($expected, true)));
    }
}

if (!extension_loaded('sodium')) {
    fwrite(STDERR, "FEHLER: die PHP-Erweiterung sodium ist aus.\n");
    fwrite(STDERR, "Einschalten mit extension=sodium in der php.ini.\n");
    exit(1);
}

// --- Lizenz-Pruefwerte ----------------------------------------------------

$path = __DIR__ . '/../tests/license-vectors.json';
$raw = @file_get_contents($path);
if ($raw === false) {
    // Fehlt die Datei, ist das ein Fehler und kein Grund zu ueberspringen.
    fwrite(STDERR, "FEHLER: $path nicht lesbar\n");
    exit(1);
}
$file = json_decode($raw, true);
if (!is_array($file) || !isset($file['cases'], $file['issuerPublicKey'])) {
    fwrite(STDERR, "FEHLER: $path ist kein brauchbares JSON\n");
    exit(1);
}
$issuer = hex2bin((string) $file['issuerPublicKey']);
if ($issuer === false || strlen($issuer) !== 32) {
    fwrite(STDERR, "FEHLER: Herstellerschluessel unbrauchbar\n");
    exit(1);
}
if (count($file['cases']) !== WANT_CASES) {
    fwrite(STDERR, sprintf("FEHLER: %d Faelle, erwartet %d\n",
        count($file['cases']), WANT_CASES));
    exit(1);
}

$valid = 0;
foreach ($file['cases'] as $case) {
    $checked++;
    $name = (string) $case['name'];
    $blob = $case['blob'] === '' ? '' : hex2bin((string) $case['blob']);
    if ($blob === false) {
        fail("$name: Block ist kein Hex");
        continue;
    }
    $isValid = !empty($case['valid']);
    if ($isValid) {
        $valid++;
    }
    try {
        $license = OskGuardLicenseReader::verify($issuer, $blob);
        if (!$isValid) {
            fail("$name: angenommen, obwohl falsch - " . $case['why']);
            continue;
        }
        $expected = $case['want'];
        want("$name/product", $license->product, $expected['product']);
        want("$name/holder", $license->holder, $expected['holder']);
        want("$name/number", $license->number, $expected['number']);
        want("$name/seats", $license->seats, (int) $expected['seats']);
        want("$name/issued", $license->issuedUtc, (int) $expected['issued']);
        want("$name/expires", $license->expiresUtc, (int) $expected['expires']);
        want("$name/tiers", $license->tiers, $expected['tiers']);
    } catch (OskGuardError $error) {
        if ($isValid) {
            fail("$name: abgewiesen, obwohl gueltig (" . $error->errorKey()
                . ') - ' . $case['why']);
            continue;
        }
        $expectedKey = (string) ($case['error'] ?? '');
        if ($expectedKey !== '' && $error->errorKey() !== $expectedKey) {
            fail("$name: erwartet $expectedKey, bekommen "
                . $error->errorKey() . ' - ' . $case['why']);
        }
    }
}
if ($valid !== WANT_VALID) {
    fail(sprintf('%d gueltige Faelle, erwartet %d', $valid, WANT_VALID));
}
printf("lizenzen: %d von %d geprueft, %d gueltig, %d abzuweisen\n",
    $checked, WANT_CASES, $valid, $checked - $valid);

// Ein fremder Herstellerschluessel macht jede Lizenz ungueltig - auch die
// richtige. Das ist der Fall, um den der ganze Umbau gemacht wurde.
$stranger = $issuer;
$stranger[0] = chr(ord($stranger[0]) ^ 0x01);
$strangerChecked = 0;
foreach ($file['cases'] as $case) {
    if (empty($case['valid'])) {
        continue;
    }
    $blob = hex2bin((string) $case['blob']);
    try {
        OskGuardLicenseReader::verify($stranger, $blob);
        fail($case['name'] . ': galt unter fremdem Herstellerschluessel');
    } catch (OskGuardError $error) {
        want('fremder Hersteller', $error->errorKey(), 'error.license.signature');
    }
    $strangerChecked++;
}
want('gueltige Faelle gegen fremden Hersteller', $strangerChecked, WANT_VALID);
printf("fremder hersteller: %d von %d gueltigen Lizenzen abgewiesen\n",
    $strangerChecked, WANT_VALID);

// --- Unterschriften-Pruefwerte (sodium, nicht selbst gerechnet) ----------
// Hier wird nicht die eigene Rechnung geprueft - die macht sodium - sondern
// dass die Bibliothek die Werte richtig herumdreht und nichts verwechselt.

$path = __DIR__ . '/../tests/vectors.json';
$raw = @file_get_contents($path);
if ($raw === false) {
    fwrite(STDERR, "FEHLER: $path nicht lesbar\n");
    exit(1);
}
$vectors = json_decode($raw, true);
if (!is_array($vectors) || !isset($vectors['verify'])) {
    fwrite(STDERR, "FEHLER: $path ist kein brauchbares JSON\n");
    exit(1);
}
$signChecked = 0;
foreach ($vectors['verify'] as $case) {
    $public = hex2bin((string) $case['public']);
    $message = $case['message'] === '' ? '' : hex2bin((string) $case['message']);
    $signature = hex2bin((string) $case['sig']);
    if ($public === false || $message === false || $signature === false) {
        fail($case['name'] . ': kein Hex');
        continue;
    }
    $ok = sodium_crypto_sign_verify_detached($signature, $message, $public);
    if ($ok !== (bool) $case['valid']) {
        fail(sprintf('%s: Urteil %s, erwartet %s - %s', $case['name'],
            var_export($ok, true), var_export((bool) $case['valid'], true),
            $case['why']));
    }
    $signChecked++;
}
if ($signChecked !== 21) {
    fail(sprintf('%d Unterschriftenfaelle, erwartet 21', $signChecked));
}
printf("unterschriften: %d von 21 geprueft\n", $signChecked);

// --- Laengenpruefungen von nodeVerify ------------------------------------

want('nodeVerify ohne Schluessel',
    OskGuard::nodeVerify('', 'a', 'b', str_repeat("\0", 64)), false);
want('nodeVerify kurze Unterschrift',
    OskGuard::nodeVerify(str_repeat("\0", 32), 'a', 'b', 'kurz'), false);
want('nodeVerify ohne Bezeichnung',
    OskGuard::nodeVerify(str_repeat("\0", 32), '', 'b', str_repeat("\0", 64)),
    false);
want('nodeVerify ohne Auftrag',
    OskGuard::nodeVerify(str_repeat("\0", 32), 'a', '', str_repeat("\0", 64)),
    false);

// --- Adresspruefung: nur die Rueckschleife -------------------------------

foreach (['8.8.8.8:8300', 'example.org:8300', 'kaputt', '127.0.0.1:'] as $bad) {
    try {
        OskGuard::open($bad, str_repeat("\0", 32));
        fail("die Adresse $bad wurde angenommen");
    } catch (OskGuardError $error) {
        want("Adresse $bad", $error->errorKey(), 'error.guard.request');
    }
}
// Ein zu kurzer Geraeteschluessel wird abgewiesen, bevor etwas verbunden wird.
try {
    OskGuard::open('127.0.0.1:1', 'kurz');
    fail('ein zu kurzer Geraeteschluessel wurde angenommen');
} catch (OskGuardError $error) {
    want('kurzer Geraeteschluessel', $error->errorKey(), 'error.guard.key');
}
try {
    OskGuard::open('127.0.0.1:1', str_repeat("\0", 32), 'kurz');
    fail('ein zu kurzer Herstellerschluessel wurde angenommen');
} catch (OskGuardError $error) {
    want('kurzer Herstellerschluessel', $error->errorKey(), 'error.guard.issuer');
}
echo "adressen und laengen: geprueft\n";

// --- aufgezeichnete Antworten --------------------------------------------
//
// Die eine Pruefung, um die es bei Stufe 1 geht: eine AUFGEZEICHNETE
// Antwort darf nicht wiederverwendbar sein. "Jede Frage ist neu" ist nicht
// dasselbe. Dieselben Bytes pruefen alle fuenf Sprachen.

const WANT_REPLAY_CASES = 6;
const WANT_REPLAY_VALID = 1;

$path = __DIR__ . '/../tests/replay-vectors.json';
$raw = @file_get_contents($path);
if ($raw === false) {
    fwrite(STDERR, "FEHLER: $path nicht lesbar\n");
    exit(1);
}
$replay = json_decode($raw, true);
if (!is_array($replay) || !isset($replay['cases'], $replay['device_pub'])) {
    fwrite(STDERR, "FEHLER: $path ist kein brauchbares JSON\n");
    exit(1);
}
$devicePub = hex2bin((string) $replay['device_pub']);
if ($devicePub === false || strlen($devicePub) !== 32) {
    fwrite(STDERR, "FEHLER: device_pub unbrauchbar\n");
    exit(1);
}
$replayChecked = 0;
$replayValid = 0;
foreach ($replay['cases'] as $case) {
    $nonce = hex2bin((string) $case['nonce']);
    $binHash = hex2bin((string) $case['binhash']);
    $signature = hex2bin((string) $case['sig']);
    if ($nonce === false || $binHash === false || $signature === false
        || strlen($nonce) !== 32 || strlen($binHash) !== 32
        || strlen($signature) !== 64) {
        fail('replay: eine Laenge passt nicht - ' . (string) $case['name']);
        continue;
    }
    // Dieselbe Bytefolge, die die Bibliothek baut.
    $signed = 'osk-guard/attest/1' . chr(0) . $nonce . $binHash;
    $got = sodium_crypto_sign_verify_detached($signature, $signed, $devicePub);
    $wanted = (bool) $case['valid'];
    if ($got !== $wanted) {
        fail(sprintf('%s: %s (%s)', (string) $case['name'],
            $wanted ? 'haette gelten muessen'
                    : 'haette abgewiesen werden muessen',
            (string) $case['why']));
    }
    if ($wanted) {
        $replayValid++;
    }
    $replayChecked++;
}
want('aufgezeichnete Antworten', $replayChecked, WANT_REPLAY_CASES);
want('davon gueltig', $replayValid, WANT_REPLAY_VALID);
printf("aufgezeichnete antworten: %d geprueft, %d gueltig\n",
    $replayChecked, $replayValid);

if ($errors > 0) {
    fwrite(STDERR, sprintf("\n%d Fehler.\n", $errors));
    exit(1);
}
echo "ergebnis: alles gruen\n";
exit(0);
