// Ed25519: nur PRUEFEN, niemals unterschreiben.
//
// Das Rahmenwerk von .NET bringt kein Ed25519 mit, und ein Paket aus dem Netz
// ist hier nicht erlaubt. Darum stehen die Formeln aus RFC 8032 (Abschnitt
// 5.1) hier ausgeschrieben, gerechnet mit System.Numerics.BigInteger.
//
// Pruefen ist der harmlose Teil: es beruehrt kein Geheimnis. Es gibt also
// keinen Seitenkanal zu verlieren, und ein Rechenfehler endet in einer
// abgelehnten Unterschrift, nicht in einem verratenen Schluessel. Das
// Unterschreiben bleibt im Chip, wo es hingehoert.
//
// Geprueft wird die Gleichung  [S]B == R + [k]A  mit
// k = SHA-512(R || A || Nachricht) mod L. Eine Unterschrift mit S >= L
// (Gruppenordnung) wird abgewiesen, ebenso ein Punkt, der sich nicht
// kanonisch entschluesseln laesst.
using System;
using System.Numerics;
using System.Security.Cryptography;

namespace OskGuard;

/// <summary>Pruefung einer Ed25519-Unterschrift. Kein Unterschreiben.</summary>
public static class Ed25519Verify
{
    /// <summary>Laenge eines oeffentlichen Schluessels in Byte.</summary>
    public const int PublicKeyLen = 32;

    /// <summary>Laenge einer Unterschrift in Byte.</summary>
    public const int SignatureLen = 64;

    // Der Primkoerper der Kurve: p = 2^255 - 19.
    private static readonly BigInteger FieldPrime =
        (BigInteger.One << 255) - 19;

    // Die Ordnung der Hauptgruppe: L = 2^252 + 27742317777372353535851937790883648493.
    private static readonly BigInteger GroupOrder =
        (BigInteger.One << 252) +
        BigInteger.Parse("27742317777372353535851937790883648493");

    // Der Kurvenparameter d = -121665/121666 mod p (edwards25519, a = -1).
    private static readonly BigInteger CurveD =
        Reduce(-121665 * Inverse(121666));

    // 2*d, nur zur Abkuerzung in der Additionsformel.
    private static readonly BigInteger CurveD2 = Reduce(2 * CurveD);

    // Eine Wurzel aus -1 mod p, gebraucht beim Wurzelziehen.
    private static readonly BigInteger SqrtMinusOne =
        BigInteger.ModPow(2, (FieldPrime - 1) / 4, FieldPrime);

    // Der Grundpunkt B der Kurve.
    private static readonly Point BasePoint = new Point(
        BigInteger.Parse(
            "15112221349535400772501151409588531511454012693041857206046113283949847762202"),
        BigInteger.Parse(
            "46316835694926478169428394003475163141307993866256225615783033603165251855960"));

    /// <summary>
    /// Prueft eine Unterschrift ueber die Nachricht selbst (kein Vorhash).
    /// Gibt true nur zurueck, wenn alles stimmt.
    /// </summary>
    public static bool Verify(
        ReadOnlySpan<byte> publicKey,
        ReadOnlySpan<byte> message,
        ReadOnlySpan<byte> signature)
    {
        // Laengen zuerst - alles andere waere ein Griff ins Leere.
        if (publicKey.Length != PublicKeyLen || signature.Length != SignatureLen)
        {
            return false;
        }

        // S ist eine Zahl im Bereich [0, L). Eine Unterschrift mit S >= L ist
        // nicht kanonisch und gilt nicht, auch wenn die Gleichung aufgeht.
        BigInteger s = LittleEndianToNumber(signature.Slice(32, 32));
        if (s >= GroupOrder)
        {
            return false;
        }

        Point? publicPoint = Decode(publicKey);
        if (publicPoint is null)
        {
            return false;
        }

        Point? commitPoint = Decode(signature.Slice(0, 32));
        if (commitPoint is null)
        {
            return false;
        }

        // k = SHA-512(R || A || Nachricht), als kleine Zahl zuerst gelesen
        // und auf die Gruppenordnung heruntergerechnet.
        byte[] digest = HashParts(signature.Slice(0, 32), publicKey, message);
        BigInteger k = Reduce(LittleEndianToNumber(digest), GroupOrder);
        CryptographicOperations.ZeroMemory(digest);

        Point left = Multiply(s, BasePoint);
        Point right = Add(commitPoint.Value, Multiply(k, publicPoint.Value));

        // Der Vergleich laeuft ueber die kanonische Kodierung: zwei Punkte
        // sind gleich, wenn ihre 32 Byte gleich sind.
        return CryptographicOperations.FixedTimeEquals(Encode(left), Encode(right));
    }

    /// <summary>SHA-512 ueber drei Teile hintereinander.</summary>
    private static byte[] HashParts(
        ReadOnlySpan<byte> first,
        ReadOnlySpan<byte> second,
        ReadOnlySpan<byte> third)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA512);
        hash.AppendData(first);
        hash.AppendData(second);
        hash.AppendData(third);
        return hash.GetHashAndReset();
    }

    // ---- Punkte der Kurve -------------------------------------------------

    // Erweiterte Koordinaten (X : Y : Z : T) mit x = X/Z, y = Y/Z und
    // T = X*Y/Z. Die Additionsformel fuer a = -1 ist vollstaendig, weil d
    // kein Quadrat ist: dasselbe Verfahren taugt auch zum Verdoppeln.
    private readonly struct Point
    {
        public readonly BigInteger X;
        public readonly BigInteger Y;
        public readonly BigInteger Z;
        public readonly BigInteger T;

        public Point(BigInteger x, BigInteger y, BigInteger z, BigInteger t)
        {
            X = x;
            Y = y;
            Z = z;
            T = t;
        }

        /// <summary>Punkt aus affinen Koordinaten.</summary>
        public Point(BigInteger x, BigInteger y)
            : this(x, y, BigInteger.One, Reduce(x * y))
        {
        }
    }

    /// <summary>Das neutrale Element der Gruppe.</summary>
    private static Point Neutral()
    {
        return new Point(BigInteger.Zero, BigInteger.One, BigInteger.One, BigInteger.Zero);
    }

    /// <summary>Addiert zwei Punkte (taugt auch zum Verdoppeln).</summary>
    private static Point Add(Point one, Point two)
    {
        BigInteger a = Reduce((one.Y - one.X) * (two.Y - two.X));
        BigInteger b = Reduce((one.Y + one.X) * (two.Y + two.X));
        BigInteger c = Reduce(one.T * CurveD2 * two.T);
        BigInteger d = Reduce(one.Z * 2 * two.Z);
        BigInteger e = Reduce(b - a);
        BigInteger f = Reduce(d - c);
        BigInteger g = Reduce(d + c);
        BigInteger h = Reduce(b + a);
        return new Point(
            Reduce(e * f),
            Reduce(g * h),
            Reduce(f * g),
            Reduce(e * h));
    }

    /// <summary>Vielfaches eines Punktes: verdoppeln und addieren, von oben.</summary>
    private static Point Multiply(BigInteger factor, Point point)
    {
        Point result = Neutral();
        if (factor.Sign <= 0)
        {
            return result;
        }

        for (int bit = (int)factor.GetBitLength() - 1; bit >= 0; bit--)
        {
            result = Add(result, result);
            if (((factor >> bit) & BigInteger.One) == BigInteger.One)
            {
                result = Add(result, point);
            }
        }

        return result;
    }

    /// <summary>
    /// Liest einen Punkt aus 32 Byte. Gibt null zurueck, wenn die Bytes
    /// keinen kanonischen Punkt der Kurve bezeichnen.
    /// </summary>
    private static Point? Decode(ReadOnlySpan<byte> raw)
    {
        if (raw.Length != 32)
        {
            return null;
        }

        // Das oberste Bit traegt das Vorzeichen von x, der Rest ist y.
        Span<byte> body = stackalloc byte[32];
        raw.CopyTo(body);
        int wantOdd = (body[31] >> 7) & 1;
        body[31] &= 0x7F;
        BigInteger y = LittleEndianToNumber(body);
        if (y >= FieldPrime)
        {
            // Nicht kanonisch: y muss kleiner als p sein.
            return null;
        }

        // x^2 = (y^2 - 1) / (d*y^2 + 1)
        BigInteger ySquared = Reduce(y * y);
        BigInteger numerator = Reduce(ySquared - 1);
        BigInteger denominator = Reduce(CurveD * ySquared + 1);
        if (denominator.IsZero)
        {
            return null;
        }

        BigInteger? root = SquareRoot(Reduce(numerator * Inverse(denominator)));
        if (root is null)
        {
            return null;
        }

        BigInteger x = root.Value;
        if (x.IsZero && wantOdd == 1)
        {
            // x = 0 hat kein negatives Gegenstueck: -0 waere eine zweite
            // Schreibweise desselben Punktes und gilt nicht.
            return null;
        }

        if ((int)(x & BigInteger.One) != wantOdd)
        {
            x = Reduce(FieldPrime - x);
        }

        return new Point(x, y);
    }

    /// <summary>Schreibt einen Punkt in die kanonischen 32 Byte.</summary>
    private static byte[] Encode(Point point)
    {
        BigInteger inverseZ = Inverse(point.Z);
        BigInteger x = Reduce(point.X * inverseZ);
        BigInteger y = Reduce(point.Y * inverseZ);
        byte[] out32 = NumberToLittleEndian(y, 32);
        out32[31] |= (byte)((int)(x & BigInteger.One) << 7);
        return out32;
    }

    // ---- Rechnen im Koerper ----------------------------------------------

    /// <summary>
    /// Quadratwurzel mod p, oder null, wenn die Zahl kein Quadrat ist.
    /// p = 5 mod 8, darum der Umweg ueber eine Wurzel aus -1.
    /// </summary>
    private static BigInteger? SquareRoot(BigInteger value)
    {
        BigInteger candidate =
            BigInteger.ModPow(value, (FieldPrime + 3) / 8, FieldPrime);
        if (Reduce(candidate * candidate) == Reduce(value))
        {
            return candidate;
        }

        candidate = Reduce(candidate * SqrtMinusOne);
        if (Reduce(candidate * candidate) == Reduce(value))
        {
            return candidate;
        }

        return null;
    }

    /// <summary>Kehrwert mod p (p ist prim, darum ueber den kleinen Fermat).</summary>
    private static BigInteger Inverse(BigInteger value)
    {
        return BigInteger.ModPow(Reduce(value), FieldPrime - 2, FieldPrime);
    }

    /// <summary>Rest mod p, immer nicht negativ.</summary>
    private static BigInteger Reduce(BigInteger value)
    {
        return Reduce(value, FieldPrime);
    }

    /// <summary>Rest mod einem Modul, immer nicht negativ.</summary>
    private static BigInteger Reduce(BigInteger value, BigInteger modulus)
    {
        BigInteger rest = value % modulus;
        return rest.Sign < 0 ? rest + modulus : rest;
    }

    /// <summary>Liest Bytes als Zahl, kleinstes Byte zuerst, ohne Vorzeichen.</summary>
    private static BigInteger LittleEndianToNumber(ReadOnlySpan<byte> raw)
    {
        return new BigInteger(raw, isUnsigned: true, isBigEndian: false);
    }

    /// <summary>Schreibt eine Zahl in feste Breite, kleinstes Byte zuerst.</summary>
    private static byte[] NumberToLittleEndian(BigInteger value, int width)
    {
        byte[] out32 = new byte[width];
        if (!value.TryWriteBytes(out32, out _, isUnsigned: true, isBigEndian: false))
        {
            // Kann bei einem Wert < p nicht vorkommen; lieber nachsehen als
            // stillschweigend etwas Falsches liefern.
            throw new InvalidOperationException("Zahl passt nicht in die Breite");
        }

        return out32;
    }
}
