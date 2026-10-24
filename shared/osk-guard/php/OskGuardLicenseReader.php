<?php
// Lizenzen pruefen - und zwar hier, nicht beim lokalen Dienst.
//
// Frueher gab der Dienst die fertig gelesenen Felder heraus. Dann haette
// eine falsche Attrappe auf der Rueckschleife Lizenzen erfinden koennen,
// ohne das geschuetzte Programm anzufassen - eine niedrigere Huerde als ein
// Disassembler. Jetzt gibt der Dienst den unterschriebenen Block heraus,
// und geprueft wird hier.
//
// Dazu gehoert ein winziger CBOR-Leser: nicht der ganze Standard, sondern
// genau die vier Sorten, die in einer Lizenz vorkommen. Alles andere wird
// abgewiesen statt geraten.

declare(strict_types=1);

final class OskGuardLicenseReader
{
    private const MAGIC = 'OSKLIC1';
    private const VERSION = 1;
    private const HEAD_LEN = 10;   // 7 + 1 + 2
    private const SIG_LEN = 64;
    private const MAX_LOAD = 1024;
    private const MAX_DEPTH = 4;

    // verify prueft Aufbau und Unterschrift und liest ERST DANACH die
    // Felder aus der geprueften Nutzlast.
    public static function verify(string $issuerPublicKey, string $blob): OskGuardLicense
    {
        if (strlen($issuerPublicKey) !== 32) {
            throw new OskGuardError('error.guard.issuer');
        }
        if (strlen($blob) < self::HEAD_LEN + self::SIG_LEN
            || substr($blob, 0, strlen(self::MAGIC)) !== self::MAGIC) {
            throw new OskGuardError('error.license.format');
        }
        if (ord($blob[strlen(self::MAGIC)]) !== self::VERSION) {
            throw new OskGuardError('error.license.version');
        }
        $size = (ord($blob[8]) << 8) | ord($blob[9]);
        if ($size === 0 || $size > self::MAX_LOAD
            || self::HEAD_LEN + $size + self::SIG_LEN !== strlen($blob)) {
            throw new OskGuardError('error.license.format');
        }
        $load = substr($blob, self::HEAD_LEN, $size);
        $signature = substr($blob, self::HEAD_LEN + $size);
        $signed = self::MAGIC . "\0" . $load;
        if (!sodium_crypto_sign_verify_detached($signature, $signed, $issuerPublicKey)) {
            throw new OskGuardError('error.license.signature');
        }
        return self::fields($load);
    }

    private static function fields(string $load): OskGuardLicense
    {
        $at = 0;
        $map = self::value($load, $at, 0);
        if ($at !== strlen($load) || !is_array($map) || !self::isMap($map)) {
            // Angehaengte Bytes werden nicht stillschweigend uebergangen.
            throw new OskGuardError('error.license.format');
        }
        $product = $map['p'] ?? '';
        if (!is_string($product) || $product === '') {
            throw new OskGuardError('error.license.product');
        }
        $tiers = [];
        if (isset($map['t'])) {
            if (!is_array($map['t'])) {
                throw new OskGuardError('error.license.tier');
            }
            foreach ($map['t'] as $tier) {
                if (!is_string($tier) || $tier === '') {
                    throw new OskGuardError('error.license.tier');
                }
                $tiers[] = $tier;
            }
        }
        return new OskGuardLicense(
            $product,
            is_string($map['h'] ?? null) ? $map['h'] : '',
            is_string($map['nr'] ?? null) ? $map['nr'] : '',
            $tiers,
            self::number($map, 's'),
            self::number($map, 'i'),
            self::number($map, 'e'),
        );
    }

    /** @param array<mixed> $map */
    private static function isMap(array $map): bool
    {
        foreach (array_keys($map) as $key) {
            if (!is_string($key)) {
                return false;
            }
        }
        return true;
    }

    /** @param array<string,mixed> $map */
    private static function number(array $map, string $key): int
    {
        $value = $map[$key] ?? 0;
        if (!is_int($value) || $value < 0) {
            throw new OskGuardError('error.license.format');
        }
        return $value;
    }

    // value liest einen Wert ab $at und schiebt $at weiter. Jede Laenge
    // wird gegen das Ende geprueft, bevor gelesen wird.
    private static function value(string $data, int &$at, int $depth): mixed
    {
        if ($depth > self::MAX_DEPTH || $at >= strlen($data)) {
            throw new OskGuardError('error.license.format');
        }
        $major = ord($data[$at]) >> 5;
        $count = self::head($data, $at);
        switch ($major) {
            case 0: // vorzeichenlose Zahl
                return $count;
            case 3: // Text
                if ($count > strlen($data) - $at) {
                    throw new OskGuardError('error.license.format');
                }
                $text = substr($data, $at, $count);
                $at += $count;
                return $text;
            case 4: // Feld
                $out = [];
                for ($i = 0; $i < $count; $i++) {
                    $out[] = self::value($data, $at, $depth + 1);
                }
                return $out;
            case 5: // Abbildung
                $out = [];
                for ($i = 0; $i < $count; $i++) {
                    $key = self::value($data, $at, $depth + 1);
                    if (!is_string($key) || array_key_exists($key, $out)) {
                        // Zweimal derselbe Schluessel: dann waere nicht
                        // entschieden, welcher gilt.
                        throw new OskGuardError('error.license.format');
                    }
                    $out[$key] = self::value($data, $at, $depth + 1);
                }
                return $out;
        }
        throw new OskGuardError('error.license.format');
    }

    // head liest Hauptsorte und Anzahl. Nur die drei kurzen Laengenformen,
    // und nur in der kuerzesten Schreibweise - mehr braucht eine Lizenz
    // nicht, und was wir nicht brauchen, lesen wir nicht.
    private static function head(string $data, int &$at): int
    {
        $low = ord($data[$at]) & 0x1f;
        $at++;
        if ($low < 24) {
            return $low;
        }
        if ($low === 24) {
            if ($at + 1 > strlen($data)) {
                throw new OskGuardError('error.license.format');
            }
            $value = ord($data[$at]);
            $at++;
            if ($value < 24) {
                throw new OskGuardError('error.license.format');
            }
            return $value;
        }
        if ($low === 25) {
            if ($at + 2 > strlen($data)) {
                throw new OskGuardError('error.license.format');
            }
            $value = (ord($data[$at]) << 8) | ord($data[$at + 1]);
            $at += 2;
            if ($value < 256) {
                throw new OskGuardError('error.license.format');
            }
            return $value;
        }
        if ($low === 26) {
            // Vier Byte. Die braucht eine Lizenz wirklich: ein Zeitstempel
            // in Sekunden passt nicht in zwei Byte. Das war beim ersten
            // Versuch vergessen, und die Pruefung hat es gefunden.
            if ($at + 4 > strlen($data)) {
                throw new OskGuardError('error.license.format');
            }
            $value = 0;
            for ($i = 0; $i < 4; $i++) {
                $value = ($value << 8) | ord($data[$at + $i]);
            }
            $at += 4;
            if ($value <= 0xffff) {
                throw new OskGuardError('error.license.format');
            }
            return $value;
        }
        if ($low === 27) {
            if ($at + 8 > strlen($data)) {
                throw new OskGuardError('error.license.format');
            }
            $value = 0;
            for ($i = 0; $i < 8; $i++) {
                $byte = ord($data[$at + $i]);
                // PHP hat vorzeichenbehaftete 64-Bit-Zahlen: alles ab 2^63
                // waere negativ, und das ist keine Anzahl.
                if ($value > (PHP_INT_MAX >> 8)) {
                    throw new OskGuardError('error.license.format');
                }
                $value = ($value << 8) | $byte;
            }
            $at += 8;
            if ($value <= 0xffffffff) {
                throw new OskGuardError('error.license.format');
            }
            return $value;
        }
        throw new OskGuardError('error.license.format');
    }
}
