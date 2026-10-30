// Pruefprogramm fuer die .NET-Fassung: kein Testrahmen aus dem Netz, nur
// das Rahmenwerk. Es laeuft die Pruefwerte aus tests/vectors.json durch -
// jeden einzelnen Fall, und am Ende steht die Fallzahl da.
//
//     TestGuard [--vectors <pfad zu vectors.json>]
//
// Null gelesene Faelle sind ein Fehler, kein Erfolg: eine Pruefung, die
// nichts geprueft hat, darf nicht gruen aussehen. Ausstiegscode 0 nur, wenn
// alle Faelle genau so beurteilt wurden wie in der Datei vermerkt.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using OskGuard;

namespace OskGuard.Tests;

/// <summary>Das Pruefprogramm.</summary>
internal static class TestGuard
{
    // So viele Faelle muessen in der Datei stehen. Wird es irgendwann mehr,
    // faellt das hier auf, statt stillschweigend durchzulaufen.
    private const int WantVerify = 21;
    private const int WantValid = 5;
    private const int WantInvalid = 16;
    private const int WantHash = 9;
    private const int WantLicense = 24;
    private const int WantLicenseValid = 3;
    private const int WantLicenseInvalid = 21;
    private const int WantReplay = 6;
    private const int WantReplayValid = 1;

    /// <summary>Einstieg. 0 heisst: alles gruen.</summary>
    internal static int Main(string[] args)
    {
        string? path = null;
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--vectors" && i + 1 < args.Length)
            {
                path = args[i + 1];
                i++;
            }
        }

        path ??= FindVectors();
        if (path is null || !File.Exists(path))
        {
            Console.Error.WriteLine("FEHLER: vectors.json nicht gefunden");
            return 1;
        }

        Console.WriteLine("pruefwerte: " + Path.GetFullPath(path));

        int failed;
        try
        {
            using JsonDocument document = JsonDocument.Parse(File.ReadAllBytes(path));
            failed = CheckHash(document.RootElement)
                + CheckVerify(document.RootElement)
                + CheckLicenseForm()
                + CheckLicenseVectors(Path.Combine(
                    Path.GetDirectoryName(Path.GetFullPath(path)) ?? ".",
                    "license-vectors.json"))
                + CheckReplay(Path.Combine(
                    Path.GetDirectoryName(Path.GetFullPath(path)) ?? ".",
                    "replay-vectors.json"));
        }
        catch (JsonException error)
        {
            Console.Error.WriteLine("FEHLER: vectors.json unlesbar: " + error.Message);
            return 1;
        }

        if (failed != 0)
        {
            Console.Error.WriteLine(
                "FEHLER: " + failed.ToString(CultureInfo.InvariantCulture) +
                " Faelle falsch beurteilt");
            return 1;
        }

        Console.WriteLine("ergebnis: alles gruen");
        return 0;
    }

    /// <summary>
    /// Die eine Pruefung, um die es bei Stufe 1 geht: eine AUFGEZEICHNETE
    /// Antwort darf nicht wiederverwendbar sein. "Jede Frage ist neu" ist
    /// nicht dasselbe.
    ///
    /// Gelesen wird tests/replay-vectors.json - dieselben Bytes pruefen
    /// alle fuenf Sprachen. Die Bytefolge wird hier gebaut, genau wie die
    /// Bibliothek sie baut. Der erste Fall MUSS gelten; ohne ihn beweist
    /// kein abgewiesener Fall etwas.
    /// </summary>
    private static int CheckReplay(string path)
    {
        if (!File.Exists(path))
        {
            Console.Error.WriteLine("FEHLER: replay-vectors.json fehlt: " + path);
            return 1;
        }

        int failed = 0;
        int seen = 0;
        int valid = 0;
        using JsonDocument document = JsonDocument.Parse(File.ReadAllBytes(path));
        JsonElement root = document.RootElement;
        if (!root.TryGetProperty("device_pub", out JsonElement pubText) ||
            !root.TryGetProperty("cases", out JsonElement list) ||
            list.ValueKind != JsonValueKind.Array)
        {
            Console.Error.WriteLine("FEHLER: replay-vectors.json unbrauchbar");
            return 1;
        }

        byte[] devicePub = Hex(pubText.GetString() ?? string.Empty);
        foreach (JsonElement item in list.EnumerateArray())
        {
            string name = item.GetProperty("name").GetString() ?? "?";
            byte[] nonce = Hex(item.GetProperty("nonce").GetString() ?? string.Empty);
            byte[] binHash = Hex(item.GetProperty("binhash").GetString() ?? string.Empty);
            byte[] signature = Hex(item.GetProperty("sig").GetString() ?? string.Empty);
            bool wanted = item.GetProperty("valid").GetBoolean();

            byte[] domain = Encoding.ASCII.GetBytes("osk-guard/attest/1");
            byte[] signed = new byte[domain.Length + 1 + nonce.Length + binHash.Length];
            domain.CopyTo(signed, 0);
            signed[domain.Length] = 0;
            nonce.CopyTo(signed, domain.Length + 1);
            binHash.CopyTo(signed, domain.Length + 1 + nonce.Length);

            bool got = Ed25519Verify.Verify(devicePub, signed, signature);
            if (got != wanted)
            {
                Console.Error.WriteLine("FEHLER: " + name + ": " +
                    (wanted ? "haette gelten muessen"
                            : "haette abgewiesen werden muessen"));
                failed++;
            }

            if (wanted)
            {
                valid++;
            }

            seen++;
        }

        Console.WriteLine("aufgezeichnete antworten: " +
            seen.ToString(CultureInfo.InvariantCulture) + " geprueft, " +
            valid.ToString(CultureInfo.InvariantCulture) + " gueltig");
        if (seen != WantReplay || valid != WantReplayValid)
        {
            Console.Error.WriteLine("FEHLER: erwartet " +
                WantReplay.ToString(CultureInfo.InvariantCulture) +
                " Faelle, davon " +
                WantReplayValid.ToString(CultureInfo.InvariantCulture) +
                " gueltig");
            failed++;
        }

        return failed;
    }

    /// <summary>Prueft die Hashwerte: SHA-256 und SHA-512 je Fall.</summary>
    private static int CheckHash(JsonElement root)
    {
        if (!root.TryGetProperty("hash", out JsonElement list) ||
            list.ValueKind != JsonValueKind.Array)
        {
            Console.Error.WriteLine("FEHLER: Abschnitt hash fehlt");
            return 1;
        }

        int seen = 0;
        int failed = 0;
        foreach (JsonElement entry in list.EnumerateArray())
        {
            seen++;
            byte[] input = Hex(Text(entry, "input"));
            string want256 = Text(entry, "sha256");
            string want512 = Text(entry, "sha512");

            string got256 = Convert.ToHexString(SHA256.HashData(input)).ToLowerInvariant();
            string got512 = Convert.ToHexString(SHA512.HashData(input)).ToLowerInvariant();

            if (got256 != want256)
            {
                failed++;
                Console.Error.WriteLine("sha256 falsch bei Fall " +
                    seen.ToString(CultureInfo.InvariantCulture));
            }

            if (got512 != want512)
            {
                failed++;
                Console.Error.WriteLine("sha512 falsch bei Fall " +
                    seen.ToString(CultureInfo.InvariantCulture));
            }
        }

        Console.WriteLine("hashwerte: " + seen.ToString(CultureInfo.InvariantCulture) +
            " von " + WantHash.ToString(CultureInfo.InvariantCulture) + " gelesen");
        if (seen != WantHash)
        {
            Console.Error.WriteLine("FEHLER: falsche Zahl Hashwerte");
            failed++;
        }

        return failed;
    }

    /// <summary>Prueft die Unterschriften: jeder Fall genau wie vermerkt.</summary>
    private static int CheckVerify(JsonElement root)
    {
        if (!root.TryGetProperty("verify", out JsonElement list) ||
            list.ValueKind != JsonValueKind.Array)
        {
            Console.Error.WriteLine("FEHLER: Abschnitt verify fehlt");
            return 1;
        }

        int seen = 0;
        int valid = 0;
        int invalid = 0;
        int failed = 0;
        var names = new List<string>();

        foreach (JsonElement entry in list.EnumerateArray())
        {
            seen++;
            string name = Text(entry, "name");
            byte[] publicKey = Hex(Text(entry, "public"));
            byte[] message = Hex(Text(entry, "message"));
            byte[] signature = Hex(Text(entry, "sig"));
            bool want = entry.GetProperty("valid").GetBoolean();

            if (want)
            {
                valid++;
            }
            else
            {
                invalid++;
            }

            bool got = Ed25519Verify.Verify(publicKey, message, signature);
            if (got != want)
            {
                failed++;
                names.Add(name);
                Console.Error.WriteLine("verify falsch bei " + name +
                    ": erwartet " + want.ToString() + ", bekommen " + got.ToString());
            }

            // Derselbe Fall noch einmal ueber NodeVerify, soweit die Form
            // passt: eine falsche Laenge muss schon vorher auffallen.
            CheckLengthGuard(publicKey, signature);
        }

        Console.WriteLine("unterschriften: " + seen.ToString(CultureInfo.InvariantCulture) +
            " von " + WantVerify.ToString(CultureInfo.InvariantCulture) + " gelesen, " +
            valid.ToString(CultureInfo.InvariantCulture) + " gueltig, " +
            invalid.ToString(CultureInfo.InvariantCulture) + " ungueltig");

        if (seen != WantVerify || valid != WantValid || invalid != WantInvalid)
        {
            Console.Error.WriteLine("FEHLER: falsche Zahl Pruefwerte (erwartet " +
                WantVerify.ToString(CultureInfo.InvariantCulture) + " / " +
                WantValid.ToString(CultureInfo.InvariantCulture) + " / " +
                WantInvalid.ToString(CultureInfo.InvariantCulture) + ")");
            failed++;
        }

        return failed;
    }

    /// <summary>
    /// NodeVerify muss eine falsche Laenge mit einem Fehlerschluessel
    /// abweisen, nicht mit einem stillen false.
    /// </summary>
    private static void CheckLengthGuard(byte[] publicKey, byte[] signature)
    {
        try
        {
            Guard.NodeVerify(publicKey.AsSpan(0, publicKey.Length - 1),
                "pruefung", new byte[] { 1 }, signature);
            throw new InvalidOperationException("kurzer Schluessel ging durch");
        }
        catch (OskGuardException error) when (error.Code == "error.guard.key")
        {
            // So soll es sein.
        }
    }

    /// <summary>
    /// Laesst die fertig unterschriebenen Lizenzen aus
    /// tests/license-vectors.json durch den eigenen Leser laufen. Bei den
    /// gueltigen werden alle sieben Feldwerte verglichen, nicht nur
    /// "kein Fehler".
    /// </summary>
    private static int CheckLicenseVectors(string path)
    {
        if (!File.Exists(path))
        {
            Console.Error.WriteLine("FEHLER: license-vectors.json nicht gefunden: " + path);
            return 1;
        }

        Console.WriteLine("lizenzwerte: " + path);
        using JsonDocument document = JsonDocument.Parse(File.ReadAllBytes(path));
        JsonElement root = document.RootElement;
        byte[] issuer = Convert.FromHexString(
            root.GetProperty("issuerPublicKey").GetString() ?? string.Empty);

        int seen = 0;
        int valid = 0;
        int invalid = 0;
        int failed = 0;
        var good = new List<byte[]>();

        foreach (JsonElement entry in root.GetProperty("cases").EnumerateArray())
        {
            seen++;
            string name = Text(entry, "name");
            byte[] blob = Hex(Text(entry, "blob"));
            bool want = entry.GetProperty("valid").GetBoolean();

            if (!want)
            {
                invalid++;
                string wantCode = Text(entry, "error");
                string got = Caught(() => Guard.ReadLicense(blob, issuer));
                if (got != wantCode)
                {
                    failed++;
                    Console.Error.WriteLine("lizenz falsch bei " + name +
                        ": erwartet " + wantCode + ", bekommen " + got);
                }

                continue;
            }

            valid++;
            good.Add(blob);
            try
            {
                License license = Guard.ReadLicense(blob, issuer);
                failed += CompareFields(name, license, entry.GetProperty("want"));
            }
            catch (OskGuardException error)
            {
                failed++;
                Console.Error.WriteLine("lizenz falsch bei " + name +
                    ": erwartet gueltig, bekommen " + error.Code);
            }
        }

        Console.WriteLine("lizenzen: " + seen.ToString(CultureInfo.InvariantCulture) +
            " von " + WantLicense.ToString(CultureInfo.InvariantCulture) + " gelesen, " +
            valid.ToString(CultureInfo.InvariantCulture) + " gueltig, " +
            invalid.ToString(CultureInfo.InvariantCulture) + " abzuweisen");

        if (seen != WantLicense || valid != WantLicenseValid || invalid != WantLicenseInvalid)
        {
            Console.Error.WriteLine("FEHLER: falsche Zahl Lizenzfaelle (erwartet " +
                WantLicense.ToString(CultureInfo.InvariantCulture) + " / " +
                WantLicenseValid.ToString(CultureInfo.InvariantCulture) + " / " +
                WantLicenseInvalid.ToString(CultureInfo.InvariantCulture) + ")");
            failed++;
        }

        // Gegenprobe: ein gedrehtes Bit im Herstellerschluessel muss jede
        // gueltige Lizenz zu Fall bringen. Sonst waere nicht gemessen, dass
        // die Unterschrift wirklich gegen diesen Schluessel geprueft wird.
        var bentIssuer = (byte[])issuer.Clone();
        bentIssuer[0] ^= 1;
        foreach (byte[] blob in good)
        {
            string got = Caught(() => Guard.ReadLicense(blob, bentIssuer));
            if (got != "error.license.signature")
            {
                failed++;
                Console.Error.WriteLine(
                    "gedrehtes Bit im Herstellerschluessel: bekommen " + got);
            }
        }

        Console.WriteLine("gegenprobe: " + good.Count.ToString(CultureInfo.InvariantCulture) +
            " gueltige mit verbogenem Herstellerschluessel abgewiesen");
        return failed;
    }

    /// <summary>Vergleicht alle sieben Felder einer gueltigen Lizenz.</summary>
    private static int CompareFields(string name, License license, JsonElement want)
    {
        int failed = 0;

        failed += Same(name, "product", license.Product, want.GetProperty("product").GetString());
        failed += Same(name, "holder", license.Holder, want.GetProperty("holder").GetString());
        failed += Same(name, "number", license.Number, want.GetProperty("number").GetString());
        failed += Same(name, "seats",
            license.Seats.ToString(CultureInfo.InvariantCulture),
            want.GetProperty("seats").GetUInt32().ToString(CultureInfo.InvariantCulture));
        failed += Same(name, "issued",
            license.IssuedUtc.ToString(CultureInfo.InvariantCulture),
            want.GetProperty("issued").GetInt64().ToString(CultureInfo.InvariantCulture));
        failed += Same(name, "expires",
            license.ExpiresUtc.ToString(CultureInfo.InvariantCulture),
            want.GetProperty("expires").GetInt64().ToString(CultureInfo.InvariantCulture));

        var tiers = new List<string>();
        foreach (JsonElement tier in want.GetProperty("tiers").EnumerateArray())
        {
            tiers.Add(tier.GetString() ?? string.Empty);
        }

        failed += Same(name, "tiers",
            string.Join(",", license.Tiers), string.Join(",", tiers));
        return failed;
    }

    /// <summary>Vergleicht ein Feld und meldet den Unterschied.</summary>
    private static int Same(string name, string field, string got, string? want)
    {
        if (got == (want ?? string.Empty))
        {
            return 0;
        }

        Console.Error.WriteLine("lizenz " + name + ", feld " + field +
            ": erwartet \"" + want + "\", bekommen \"" + got + "\"");
        return 1;
    }

    /// <summary>
    /// Prueft, dass ein kaputter Lizenzblock mit dem richtigen Schluessel
    /// abgewiesen wird. Von Hand gebaut, ohne gueltige Unterschrift - deckt
    /// den Kopf ab, noch bevor die Pruefwerte gelesen werden.
    /// </summary>
    private static int CheckLicenseForm()
    {
        var issuer = new byte[32];
        issuer[0] = 1;

        // Ein Block mit richtigem Aufbau, aber erfundener Unterschrift.
        byte[] payload = new byte[] { 0xA1, 0x61, 0x70, 0x61, 0x78 };
        byte[] good = BuildLicense(payload, version: 1, payloadLen: payload.Length);

        var cases = new (string Name, byte[] Blob, string Code)[]
        {
            ("zu kurz", new byte[20], "error.license.format"),
            ("falsche Kennung", Bend(good, 0, 0x58), "error.license.format"),
            ("falsche Fassung", BuildLicense(payload, 2, payload.Length),
                "error.license.version"),
            ("falsche Laenge", BuildLicense(payload, 1, payload.Length + 1),
                "error.license.format"),
            ("falsche Unterschrift", good, "error.license.signature"),
        };

        int failed = 0;
        foreach ((string name, byte[] blob, string code) in cases)
        {
            string got = Caught(() => Guard.ReadLicense(blob, issuer));
            if (got != code)
            {
                failed++;
                Console.Error.WriteLine("lizenz falsch bei " + name +
                    ": erwartet " + code + ", bekommen " + got);
            }
        }

        // Ohne Herstellerschluessel gibt es kein ungeprueftes Ergebnis.
        string noIssuer = Caught(() => Guard.ReadLicense(good, Array.Empty<byte>()));
        if (noIssuer != "error.guard.issuer")
        {
            failed++;
            Console.Error.WriteLine("lizenz ohne Herstellerschluessel: bekommen " + noIssuer);
        }

        Console.WriteLine("lizenzaufbau: " +
            (cases.Length + 1).ToString(CultureInfo.InvariantCulture) + " faelle geprueft");
        return failed;
    }

    /// <summary>Baut einen OSKLIC1-Block mit erfundener Unterschrift.</summary>
    private static byte[] BuildLicense(byte[] payload, byte version, int payloadLen)
    {
        var blob = new byte[7 + 1 + 2 + payload.Length + 64];
        Encoding.ASCII.GetBytes("OSKLIC1").CopyTo(blob, 0);
        blob[7] = version;
        blob[8] = (byte)(payloadLen >> 8);
        blob[9] = (byte)(payloadLen & 0xFF);
        payload.CopyTo(blob, 10);
        return blob;
    }

    /// <summary>Verbiegt ein Byte einer Kopie.</summary>
    private static byte[] Bend(byte[] blob, int at, byte value)
    {
        var copy = (byte[])blob.Clone();
        copy[at] = value;
        return copy;
    }

    /// <summary>Nennt den Fehlerschluessel, den ein Aufruf geworfen hat.</summary>
    private static string Caught(Action call)
    {
        try
        {
            call();
            return "(kein Fehler)";
        }
        catch (OskGuardException error)
        {
            return error.Code;
        }
    }

    /// <summary>Ein Textfeld aus dem Fall.</summary>
    private static string Text(JsonElement entry, string name)
    {
        return entry.GetProperty(name).GetString() ?? string.Empty;
    }

    /// <summary>Hex zu Bytes; leer bleibt leer.</summary>
    private static byte[] Hex(string text)
    {
        return text.Length == 0 ? Array.Empty<byte>() : Convert.FromHexString(text);
    }

    /// <summary>
    /// Sucht tests/vectors.json, indem sie vom Programmordner nach oben
    /// geht. So laeuft das Pruefprogramm aus jedem Bauordner.
    /// </summary>
    private static string? FindVectors()
    {
        var starts = new[] { AppContext.BaseDirectory, Directory.GetCurrentDirectory() };
        foreach (string start in starts)
        {
            DirectoryInfo? at = new DirectoryInfo(start);
            for (int step = 0; step < 12 && at is not null; step++)
            {
                string candidate = Path.Combine(at.FullName, "tests", "vectors.json");
                if (File.Exists(candidate))
                {
                    return candidate;
                }

                at = at.Parent;
            }
        }

        return null;
    }
}
