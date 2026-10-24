// Beispiel in .NET: ein Programm, das nur mit Stick laeuft, eine Datei
// verschliesst, die ohne Stick unlesbar bleibt, seine Lizenz prueft und
// einen Auftrag unterschreibt.
//
//     Example --stand ..\..\stand.json
//
// Jede Zeile, die mit "ergebnis:" anfaengt, liest der Pruefstand mit. Bricht
// das Beispiel ab, ist der Ausstiegscode nicht 0.
using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using OskGuard;

namespace OskGuard.Example;

/// <summary>Das Beispiel.</summary>
internal static class Example
{
    /// <summary>Einstieg. 0 nur bei Erfolg.</summary>
    internal static int Main(string[] args)
    {
        string path = "stand.json";
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--stand" && i + 1 < args.Length)
            {
                path = args[i + 1];
                i++;
            }
        }

        try
        {
            Run(path);
            return 0;
        }
        catch (OskGuardException error)
        {
            Console.Error.WriteLine("FEHLER: " + error.Code);
            return 1;
        }
        catch (Exception error) when (error is IOException or JsonException
            or FormatException or InvalidOperationException)
        {
            Console.Error.WriteLine("FEHLER: " + error.Message);
            return 1;
        }
    }

    /// <summary>Die Angaben des Pruefstands.</summary>
    private sealed class Stand
    {
        public string Address { get; set; } = string.Empty;

        public string DeviceKey { get; set; } = string.Empty;

        public string IssuerKey { get; set; } = string.Empty;

        public string Product { get; set; } = string.Empty;

        public string Label { get; set; } = string.Empty;

        public string NodeKey { get; set; } = string.Empty;
    }

    private static void Run(string path)
    {
        Stand stand = ReadStand(path);
        byte[] deviceKey = Convert.FromHexString(stand.DeviceKey);

        // Der Schluessel des Herstellers: mit ihm prueft die Bibliothek die
        // Lizenz selbst, statt dem lokalen Dienst zu glauben.
        byte[] issuerKey = stand.IssuerKey.Length == 0
            ? Array.Empty<byte>()
            : Convert.FromHexString(stand.IssuerKey);

        // 1. Ohne Stick laeuft das Programm nicht los.
        Guard guard = Guard.Open(stand.Address, deviceKey, issuerKey);
        guard.Present(string.Empty);
        Console.WriteLine("ergebnis: praesenz ok");

        // 2. Eine Datei, die ohne Stick unlesbar bleibt.
        byte[] plain = Encoding.UTF8.GetBytes(
            "die Tabelle, ohne die das Programm nichts kann");
        byte[] extra = Encoding.UTF8.GetBytes("version-1");
        byte[] wrongExtra = Encoding.UTF8.GetBytes("version-2");

        byte[] blob = guard.Seal(stand.Label, extra, plain);
        byte[] back = guard.Unseal(stand.Label, extra, blob);
        if (!back.SequenceEqual(plain))
        {
            throw new InvalidOperationException("heraus kam etwas anderes");
        }

        // Mit dem falschen Zusatz geht sie nicht auf - das muss so sein.
        bool opened = true;
        try
        {
            byte[] wrong = guard.Unseal(stand.Label, wrongExtra, blob);
            CryptographicOperations.ZeroMemory(wrong);
        }
        catch (OskGuardException)
        {
            opened = false;
        }

        if (opened)
        {
            throw new InvalidOperationException("mit falschem Zusatz aufgegangen");
        }

        Console.WriteLine("ergebnis: verschlossen " +
            blob.Length.ToString(CultureInfo.InvariantCulture) +
            " byte, wieder offen");

        // Die Klartexte werden nicht mehr gebraucht: ueberschreiben.
        CryptographicOperations.ZeroMemory(back);
        CryptographicOperations.ZeroMemory(plain);

        // 3. Die Lizenz - Aufbau und Unterschrift hier geprueft, die Werte
        // kommen aus der geprueften Nutzlast.
        License license = guard.License(stand.Product);
        if (license.Expired)
        {
            throw new InvalidOperationException("Lizenz abgelaufen");
        }

        if (!license.HasTier("berichte"))
        {
            throw new InvalidOperationException("Ausbaustufe fehlt");
        }

        Console.WriteLine("ergebnis: lizenz " + license.Product + ", " +
            license.Seats.ToString(CultureInfo.InvariantCulture) + " plaetze, stufen [" +
            string.Join(" ", license.Tiers) + "]");

        // 4. Ein Auftrag, unterschrieben vom Schluessel im Chip.
        byte[] nodePublic = guard.NodePublic(stand.NodeKey, "ed25519");
        byte[] job = Encoding.UTF8.GetBytes(
            "{\"tat\":\"abbild-holen\",\"wann\":1700000000}");
        byte[] signature = guard.NodeSign(stand.NodeKey, job);
        Guard.NodeVerify(nodePublic, stand.NodeKey, job, signature);

        // Ein einziges verbogenes Byte muss auffallen.
        byte[] bent = (byte[])job.Clone();
        bent[3] ^= 1;
        bool held = true;
        try
        {
            Guard.NodeVerify(nodePublic, stand.NodeKey, bent, signature);
        }
        catch (OskGuardException)
        {
            held = false;
        }

        if (held)
        {
            throw new InvalidOperationException("ein veraenderter Auftrag galt");
        }

        Console.WriteLine("ergebnis: auftrag unterschrieben und geprueft");

        // 5. Der Schluesselaustausch - und der Fall, dass der Chip ihn nicht
        // kann. Dann faellt ein Anwendungsfall weg; das Programm sagt es und
        // laeuft weiter, statt abzubrechen.
        Agree(guard, stand.NodeKey);

        // 6. Weiterlaufen und beim Abziehen abbrechen - hier nur eine Runde,
        // damit das Beispiel von selbst endet.
        WatchOnce(guard);
        Console.WriteLine("ergebnis: ueberwachung laeuft");
    }

    /// <summary>Holt ein gemeinsames Geheimnis, oder nennt, was wegfaellt.</summary>
    private static void Agree(Guard guard, string label)
    {
        byte[]? peer = null;
        byte[]? secret = null;
        try
        {
            peer = guard.NodePublic(label, "x25519");
            secret = guard.NodeAgree(label, peer);
            Console.WriteLine("ergebnis: gemeinsames geheimnis " +
                secret.Length.ToString(CultureInfo.InvariantCulture) + " byte");
        }
        catch (OskGuardException error) when (error.IsNoEcdh)
        {
            Console.WriteLine("ergebnis: kein schluesselaustausch - " +
                "serverbetrieb mit schluessel im chip faellt weg");
        }
        finally
        {
            // Das Geheimnis wird nicht mehr gebraucht.
            if (secret is not null)
            {
                CryptographicOperations.ZeroMemory(secret);
            }

            if (peer is not null)
            {
                CryptographicOperations.ZeroMemory(peer);
            }
        }
    }

    /// <summary>
    /// Laesst die Ueberwachung kurz laufen. Meldet sie einen Verlust, ist das
    /// hier ein Fehler - der Stick steckt ja.
    /// </summary>
    private static void WatchOnce(Guard guard)
    {
        OskGuardException? lost = null;
        using var stop = new CancellationTokenSource();
        Task watcher = Task.Run(() => guard.Watch(
            TimeSpan.FromMilliseconds(200),
            string.Empty,
            error => lost = error,
            stop.Token));

        if (watcher.Wait(TimeSpan.FromMilliseconds(600)))
        {
            throw new InvalidOperationException(
                "abgebrochen: " + (lost?.Code ?? "unbekannt"));
        }

        stop.Cancel();
        watcher.Wait(TimeSpan.FromSeconds(5));
    }

    /// <summary>Liest stand.json.</summary>
    private static Stand ReadStand(string path)
    {
        var options = new JsonSerializerOptions
        {
            PropertyNameCaseInsensitive = true,
        };
        Stand? stand = JsonSerializer.Deserialize<Stand>(
            File.ReadAllBytes(path), options);
        if (stand is null || stand.Address.Length == 0 || stand.DeviceKey.Length == 0)
        {
            throw new InvalidOperationException("stand.json unvollstaendig");
        }

        return stand;
    }
}
