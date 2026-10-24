<?php
// osk-guard fuer PHP: ein geschuetztes Programm in wenigen Zeilen.
//
//     $guard = OskGuard::open('127.0.0.1:8300', $deviceKey);
//     $guard->present(null);   // wirft, wenn der Stick nicht steckt
//
// Die Bibliothek haengt am lokalen Dienst des Programms `osk` und prueft die
// Unterschrift SELBST. Dem Dienst wird kein Ja geglaubt - sonst waere der
// Schutz die Aussage eines Programms ueber sich selbst.
//
// Gebraucht wird allein, was in PHP steckt: sodium (seit PHP 7.2 im Kern)
// fuer Ed25519 und hash() fuer SHA-256. Kein composer, kein Fremdcode.

declare(strict_types=1);

// Ein Fehler des Dongle-Schutzes. getCode() gibt es schon, darum heisst der
// Schluessel hier $errorKey - derselbe Schluessel wie in den anderen vier
// Bibliotheken, und in lang/<sprache>.json steht der Satz dazu.
final class OskGuardError extends RuntimeException
{
    private string $errorKey;

    public function __construct(string $errorKey)
    {
        parent::__construct($errorKey);
        $this->errorKey = $errorKey;
    }

    public function errorKey(): string
    {
        return $this->errorKey;
    }

    // Kann der gelieferte Chip keinen Schluesselaustausch? Dann faellt der
    // Serverbetrieb mit Schluessel im Chip weg - das Programm sollte den
    // Anwendungsfall abschalten und es sagen, nicht abbrechen.
    public function isNoEcdh(): bool
    {
        return $this->errorKey === 'error.guard.noecdh';
    }
}

// Eine gepruefte Lizenz aus dem Tresor.
final class OskGuardLicense
{
    public function __construct(
        public readonly string $product,
        public readonly string $holder,
        public readonly string $number,
        /** @var string[] */
        public readonly array $tiers,
        public readonly int $seats,
        public readonly int $issuedUtc,
        public readonly int $expiresUtc,
    ) {
    }

    public function expired(): bool
    {
        return $this->expiresUtc !== 0 && time() > $this->expiresUtc;
    }

    public function hasTier(string $name): bool
    {
        foreach ($this->tiers as $have) {
            if (strcasecmp($have, $name) === 0) {
                return true;
            }
        }
        return false;
    }
}

final class OskGuard
{
    public const NONCE_LEN = 32;
    public const HASH_LEN = 32;
    public const KEY_LEN = 32;
    public const SIG_LEN = 64;
    public const SECRET_LEN = 32;

    private const ATTEST_DOMAIN = 'osk-guard/attest/1';
    private const SIGN_DOMAIN = 'osk-guard/sign/1';
    private const MAX_BINARY = 536870912; // 512 MiB
    private const MAX_LINE = 4194304;     // 4 MiB

    private string $address;
    private string $devicePublicKey;
    private int $timeout;

    private function __construct(string $address, string $devicePublicKey, int $timeout)
    {
        $this->address = $address;
        $this->devicePublicKey = $devicePublicKey;
        $this->timeout = $timeout;
    }

    // open prueft die Angaben und sieht nach, ob der Dienst antwortet.
    // $devicePublicKey sind die 32 rohen Byte, die der Hersteller einmal mit
    // `osk guard-key` aus seinem Stick gelesen und eingetragen hat.
    public static function open(string $address, string $devicePublicKey,
                               int $timeout = 30): self
    {
        if (!extension_loaded('sodium')) {
            // Ohne sodium kann diese Bibliothek die Unterschrift nicht
            // pruefen - und dann laeuft sie nicht los und behauptet nichts.
            throw new OskGuardError('error.guard.internal');
        }
        if (strlen($devicePublicKey) !== self::KEY_LEN) {
            throw new OskGuardError('error.guard.key');
        }
        $at = strrpos($address, ':');
        if ($at === false || $at === strlen($address) - 1) {
            throw new OskGuardError('error.guard.request');
        }
        $host = substr($address, 0, $at);
        // Nur die Rueckschleife: der Stick steckt hier, nicht im Netz.
        if ($host !== 'localhost' && $host !== '127.0.0.1' && $host !== '::1'
            && $host !== '[::1]') {
            throw new OskGuardError('error.guard.request');
        }
        $guard = new self($address, $devicePublicKey, max(1, $timeout));
        $guard->ask('ping');
        return $guard;
    }

    // ask schickt eine Zeile und liest die Antwort. Je Anfrage eine
    // Verbindung: langsamer, aber ein Programm kann so nie die Antwort einer
    // fremden Anfrage lesen.
    /** @return array<string,string> */
    private function ask(string $line): array
    {
        $at = strrpos($this->address, ':');
        $host = substr($this->address, 0, (int) $at);
        $port = (int) substr($this->address, (int) $at + 1);
        $socket = @fsockopen($host, $port, $code, $text, 3.0);
        if ($socket === false) {
            throw new OskGuardError('error.guard.nodevice');
        }
        try {
            stream_set_timeout($socket, $this->timeout);
            if (@fwrite($socket, $line . "\n") === false) {
                throw new OskGuardError('error.guard.nodevice');
            }
            $answer = @fgets($socket, self::MAX_LINE);
            $info = stream_get_meta_data($socket);
            if ($answer === false || !empty($info['timed_out'])) {
                throw new OskGuardError('error.guard.nodevice');
            }
        } finally {
            fclose($socket);
        }
        return self::parseAnswer(rtrim($answer, "\r\n"));
    }

    /** @return array<string,string> */
    private static function parseAnswer(string $line): array
    {
        $fields = preg_split('/\s+/', trim($line), -1, PREG_SPLIT_NO_EMPTY);
        if ($fields === false || count($fields) === 0) {
            throw new OskGuardError('error.guard.answer');
        }
        if ($fields[0] === 'ERR') {
            throw new OskGuardError($fields[1] ?? 'error.guard.internal');
        }
        if ($fields[0] !== 'OK') {
            throw new OskGuardError('error.guard.answer');
        }
        $out = [];
        foreach (array_slice($fields, 1) as $field) {
            $split = strpos($field, '=');
            if ($split !== false) {
                $out[substr($field, 0, $split)] = substr($field, $split + 1);
            }
        }
        return $out;
    }

    // value holt einen Hexwert aus der Antwort und prueft seine Laenge.
    /** @param array<string,string> $answer */
    private static function value(array $answer, string $key, int $want): string
    {
        $text = $answer[$key] ?? '';
        if ($text === '' || $text === '-') {
            throw new OskGuardError('error.guard.answer');
        }
        $raw = self::fromHex($text);
        if ($want > 0 && strlen($raw) !== $want) {
            throw new OskGuardError('error.guard.answer');
        }
        return $raw;
    }

    private static function fromHex(string $text): string
    {
        if ($text === '' || strlen($text) % 2 !== 0
            || preg_match('/\A[0-9a-f]*\z/', $text) !== 1) {
            throw new OskGuardError('error.guard.answer');
        }
        $raw = hex2bin($text);
        if ($raw === false) {
            throw new OskGuardError('error.guard.answer');
        }
        return $raw;
    }

    // binaryHash ist die Pruefsumme einer Programmdatei. null: die Datei,
    // die gerade laeuft.
    public static function binaryHash(?string $path): string
    {
        if ($path === null || $path === '') {
            // Bei PHP ist "die laufende Datei" das Skript, nicht der
            // Interpreter - genau das will ein geschuetztes Programm auch
            // gebunden haben.
            $path = $_SERVER['SCRIPT_FILENAME'] ?? '';
            if ($path === '') {
                throw new OskGuardError('error.guard.binary');
            }
        }
        if (!is_file($path)) {
            throw new OskGuardError('error.guard.binary');
        }
        $size = filesize($path);
        if ($size === false || $size > self::MAX_BINARY) {
            throw new OskGuardError('error.guard.binary');
        }
        $hash = hash_file('sha256', $path, true);
        if ($hash === false) {
            throw new OskGuardError('error.guard.binary');
        }
        return $hash;
    }

    // message baut die Bytefolge, die das Geraet unterschreibt.
    private static function message(string $domain, string $nonce, string $binHash): string
    {
        return $domain . "\0" . $nonce . $binHash;
    }

    // present ist eine Runde Frage und Antwort: der Zufallswert kommt von
    // hier, die Unterschrift wird hier geprueft. Kommt keine Ausnahme,
    // steckte in diesem Augenblick der richtige Stick, und die
    // Programmdatei ist unveraendert.
    //
    // Was diese Stufe wert ist, steht in docs/dongle.md - sie haelt das
    // Mitnehmen einer Kopie auf und nichts darueber hinaus. Echter Schutz
    // ist seal/unseal.
    public function present(?string $binaryPath = null): void
    {
        $binHash = self::binaryHash($binaryPath);
        $nonce = random_bytes(self::NONCE_LEN);
        $answer = $this->ask('attest ' . bin2hex($nonce) . ' ' . bin2hex($binHash));
        $public = self::value($answer, 'pub', self::KEY_LEN);
        $signature = self::value($answer, 'sig', self::SIG_LEN);
        if (!hash_equals($this->devicePublicKey, $public)) {
            throw new OskGuardError('error.guard.wrongdevice');
        }
        $signed = self::message(self::ATTEST_DOMAIN, $nonce, $binHash);
        if (!sodium_crypto_sign_verify_detached($signature, $signed,
                                                $this->devicePublicKey)) {
            throw new OskGuardError('error.guard.signature');
        }
    }

    // seal verschliesst Daten mit einem Schluessel, der den Stick nie
    // verlaesst. $extra darf leer sein; ist er gesetzt, muss er beim
    // Oeffnen gleich sein.
    public function seal(string $label, string $extra, string $plain): string
    {
        if ($plain === '') {
            throw new OskGuardError('error.guard.size');
        }
        $answer = $this->ask('seal ' . bin2hex($label) . ' '
            . self::hexOrDash($extra) . ' ' . bin2hex($plain));
        return self::value($answer, 'blob', 0);
    }

    // unseal macht wieder auf. Ohne Stick gibt es den Schluessel nicht -
    // dann kommt eine Ausnahme und keine halben Daten.
    public function unseal(string $label, string $extra, string $blob): string
    {
        if ($blob === '') {
            throw new OskGuardError('error.guard.size');
        }
        $answer = $this->ask('unseal ' . bin2hex($label) . ' '
            . self::hexOrDash($extra) . ' ' . bin2hex($blob));
        return self::value($answer, 'plain', 0);
    }

    // license holt die Lizenz fuer ein Produkt und prueft ihre Unterschrift.
    public function license(string $product): OskGuardLicense
    {
        $answer = $this->ask('license ' . bin2hex($product));
        $tiers = [];
        $list = $answer['tiers'] ?? '-';
        if ($list !== '' && $list !== '-') {
            foreach (explode(',', $list) as $part) {
                $tiers[] = self::fromHex($part);
            }
        }
        return new OskGuardLicense(
            self::value($answer, 'product', 0),
            self::optional($answer['holder'] ?? '-'),
            self::optional($answer['number'] ?? '-'),
            $tiers,
            self::number($answer, 'seats'),
            self::number($answer, 'issued'),
            self::number($answer, 'expires'),
        );
    }

    // nodePublic holt den oeffentlichen Teil eines Knotenschluessels.
    // $curve ist 'ed25519' zum Unterschreiben oder 'x25519' zum Austausch.
    public function nodePublic(string $label, string $curve): string
    {
        $answer = $this->ask('nodepub ' . bin2hex($label) . ' ' . bin2hex($curve));
        return self::value($answer, 'pub', self::KEY_LEN);
    }

    // nodeSign laesst den Knoten einen Auftrag unterschreiben.
    public function nodeSign(string $label, string $job): string
    {
        if ($job === '') {
            throw new OskGuardError('error.guard.size');
        }
        $answer = $this->ask('nodesign ' . bin2hex($label) . ' ' . bin2hex($job));
        return self::value($answer, 'sig', self::SIG_LEN);
    }

    // nodeVerify prueft die Unterschrift eines anderen Knotens. Das braucht
    // keinen Stick - nur den oeffentlichen Schluessel der Gegenseite.
    public static function nodeVerify(string $publicKey, string $label,
                                      string $job, string $signature): bool
    {
        if (strlen($publicKey) !== self::KEY_LEN
            || strlen($signature) !== self::SIG_LEN
            || $label === '' || $job === '') {
            return false;
        }
        $signed = self::SIGN_DOMAIN . "\0" . $label . "\0"
            . hash('sha256', $job, true);
        return sodium_crypto_sign_verify_detached($signature, $signed, $publicKey);
    }

    // nodeAgree handelt ein gemeinsames Geheimnis mit der Gegenseite aus.
    // Kann der gelieferte Chip kein ECDH, kommt error.guard.noecdh.
    public function nodeAgree(string $label, string $peerPublic): string
    {
        if (strlen($peerPublic) !== self::SECRET_LEN) {
            throw new OskGuardError('error.guard.peerkey');
        }
        $answer = $this->ask('nodeagree ' . bin2hex($label) . ' '
            . bin2hex($peerPublic));
        return self::value($answer, 'sec', self::SECRET_LEN);
    }

    /** @param array<string,string> $answer */
    private static function number(array $answer, string $key): int
    {
        $text = $answer[$key] ?? '';
        if (preg_match('/\A-?[0-9]{1,19}\z/', $text) !== 1) {
            throw new OskGuardError('error.guard.answer');
        }
        return (int) $text;
    }

    private static function hexOrDash(string $raw): string
    {
        return $raw === '' ? '-' : bin2hex($raw);
    }

    private static function optional(string $text): string
    {
        if ($text === '' || $text === '-') {
            return '';
        }
        return self::fromHex($text);
    }
}
