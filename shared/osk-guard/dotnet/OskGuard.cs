// osk-guard fuer .NET: ein geschuetztes Programm in wenigen Zeilen.
//
//     using var guard = Guard.Open("127.0.0.1:8300", deviceKey);
//     guard.Present("");            // wirft, wenn der Stick nicht steckt
//
// Die Bibliothek haengt am lokalen Dienst des Programms `osk` und prueft die
// Unterschrift SELBST. Dem Dienst wird kein Ja geglaubt - sonst waere der
// Schutz die Aussage eines Programms ueber sich selbst. Jedes Programm auf
// dem Rechner kann auf der Rueckschleife lauschen und "OK" antworten; nur
// eine Unterschrift unter einem frischen Zufallswert kann es nicht
// erfinden.
//
// Nur das Rahmenwerk, kein Fremdcode und kein Paket aus dem Netz. Ed25519
// fehlt im Rahmenwerk, darum steht die Pruefung in Ed25519Verify.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Threading;

namespace OskGuard;

/// <summary>
/// Fehler des Dongle-Schutzes. <see cref="Code"/> ist der Schluessel, zu dem
/// in lang/&lt;sprache&gt;.json ein Satz steht - derselbe Schluessel in allen
/// fuenf Bibliotheken.
/// </summary>
public sealed class OskGuardException : Exception
{
    /// <summary>Legt einen Fehler mit seinem Schluessel an.</summary>
    public OskGuardException(string code)
        : base(code)
    {
        Code = code;
    }

    /// <summary>Der Fehlerschluessel, zum Beispiel error.guard.nodevice.</summary>
    public string Code { get; }

    /// <summary>
    /// Sagt, ob der gelieferte Chip keinen Schluesselaustausch kann. Dann
    /// faellt der Serverbetrieb mit Schluessel im Chip weg - das Programm
    /// sollte den Anwendungsfall abschalten und es sagen, nicht abbrechen.
    /// </summary>
    public bool IsNoEcdh => Code == Guard.NoEcdhCode;
}

/// <summary>Eine gepruefte Lizenz aus dem Tresor des Sticks.</summary>
public sealed class License
{
    /// <summary>Name des Produkts.</summary>
    public string Product { get; init; } = string.Empty;

    /// <summary>Auf wen die Lizenz laeuft, oder leer.</summary>
    public string Holder { get; init; } = string.Empty;

    /// <summary>Lizenznummer, oder leer.</summary>
    public string Number { get; init; } = string.Empty;

    /// <summary>Die enthaltenen Ausbaustufen.</summary>
    public IReadOnlyList<string> Tiers { get; init; } = Array.Empty<string>();

    /// <summary>Zahl der Plaetze.</summary>
    public uint Seats { get; init; }

    /// <summary>Ausgestellt am, als Sekunden seit 1970 in UTC.</summary>
    public long IssuedUtc { get; init; }

    /// <summary>Laeuft ab am, als Sekunden seit 1970 in UTC; 0 = nie.</summary>
    public long ExpiresUtc { get; init; }

    /// <summary>Sagt, ob die Lizenz jetzt abgelaufen ist.</summary>
    public bool Expired =>
        ExpiresUtc != 0 && DateTimeOffset.UtcNow.ToUnixTimeSeconds() > ExpiresUtc;

    /// <summary>Sagt, ob eine Ausbaustufe enthalten ist (Gross-/Kleinschreibung egal).</summary>
    public bool HasTier(string name)
    {
        foreach (string have in Tiers)
        {
            if (string.Equals(have, name, StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }
        }

        return false;
    }
}

/// <summary>Die Verbindung zum lokalen Dienst von osk.</summary>
public sealed class Guard
{
    /// <summary>Laenge des Zufallswertes in Byte.</summary>
    public const int NonceLen = 32;

    /// <summary>Laenge einer Pruefsumme in Byte.</summary>
    public const int HashLen = 32;

    /// <summary>Laenge eines Schluessels in Byte.</summary>
    public const int KeyLen = 32;

    /// <summary>Laenge einer Unterschrift in Byte.</summary>
    public const int SigLen = 64;

    /// <summary>Laenge eines gemeinsamen Geheimnisses in Byte.</summary>
    public const int SecretLen = 32;

    /// <summary>Der Schluessel, den ein Chip ohne Schluesselaustausch meldet.</summary>
    public const string NoEcdhCode = "error.guard.noecdh";

    // Der Zweck steht vor der Nachricht, damit eine Auftragsunterschrift und
    // eine Praesenzantwort nie dieselben Bytes sind.
    private const string AttestDomain = "osk-guard/attest/1";
    private const string SignDomain = "osk-guard/sign/1";

    // Grenzen des Verfahrens. Die Zeile gilt bis 4 MiB, und Nutzdaten gehen
    // als Hex hinein - darum hoechstens 1 MiB je Richtung.
    private const int MaxLine = 4 << 20;
    private const int MaxPayload = 1 << 20;

    // Ein verschlossener Block ist groesser als sein Klartext: Kennung,
    // Fassung, Salz, Nonce und das Siegel von AES-GCM kommen dazu (nach
    // README 8 + 1 + 16 + 12 + 16 = 53 Byte). Beim Oeffnen muss darum mehr
    // hereindarfen als 1 MiB, sonst laesst sich genau das nicht mehr
    // oeffnen, was seal gerade geliefert hat.
    private const int MaxBlob = MaxPayload + 1024;
    private const long MaxBinary = 512L << 20;

    // Aufbau einer Lizenz: "OSKLIC1" | Fassung(1) | Laenge(2, grosses Byte
    // zuerst) | Nutzlast | Unterschrift(64).
    private const string LicenseMagic = "OSKLIC1";
    private const byte LicenseVersion = 1;
    private const int LicenseHeadLen = 7 + 1 + 2;

    private readonly string _host;
    private readonly int _port;
    private readonly byte[] _devicePublicKey;
    private readonly byte[] _issuerPublicKey;

    private Guard(string host, int port, byte[] devicePublicKey, byte[] issuerPublicKey)
    {
        _host = host;
        _port = port;
        _devicePublicKey = devicePublicKey;
        _issuerPublicKey = issuerPublicKey;
    }

    /// <summary>Zeitlimit fuer eine Antwort des Dienstes.</summary>
    public TimeSpan Timeout { get; set; } = TimeSpan.FromSeconds(30);

    /// <summary>Zeitlimit fuer den Verbindungsaufbau.</summary>
    public TimeSpan ConnectTimeout { get; set; } = TimeSpan.FromSeconds(3);

    /// <summary>
    /// Prueft die Angaben und sieht nach, ob der Dienst antwortet.
    /// devicePublicKey sind die 32 Byte, die der Hersteller einmal mit
    /// `osk guard-key` aus seinem Stick gelesen und ins Programm
    /// eingetragen hat. issuerPublicKey ist der oeffentliche Schluessel des
    /// Herstellers, mit dem Lizenzen unterschrieben sind; er darf leer
    /// bleiben, dann scheitert <see cref="License"/> mit
    /// error.guard.issuer, statt ungeprueft etwas zu liefern.
    /// </summary>
    public static Guard Open(
        string address,
        ReadOnlySpan<byte> devicePublicKey,
        ReadOnlySpan<byte> issuerPublicKey = default)
    {
        if (devicePublicKey.Length != KeyLen)
        {
            throw Fail("error.guard.key");
        }

        if (issuerPublicKey.Length != 0 && issuerPublicKey.Length != KeyLen)
        {
            throw Fail("error.guard.issuer");
        }

        (string host, int port) = SplitAddress(address);
        var guard = new Guard(
            host, port, devicePublicKey.ToArray(), issuerPublicKey.ToArray());

        // Eine Runde ping: antwortet gar nichts, lohnt der Rest nicht.
        guard.Ask("ping");
        return guard;
    }

    /// <summary>
    /// Eine Runde Frage und Antwort: der Zufallswert kommt von hier, die
    /// Unterschrift wird hier geprueft. Kommt kein Fehler, steckte in diesem
    /// Augenblick der richtige Stick, und die Programmdatei ist unveraendert.
    /// Leerer Pfad heisst: die laufende Datei.
    /// </summary>
    public void Present(string binaryPath)
    {
        byte[] binaryHash = BinaryHash(binaryPath);
        byte[] nonce = RandomNumberGenerator.GetBytes(NonceLen);

        Dictionary<string, string> answer = Ask(
            "attest " + ToHex(nonce) + " " + ToHex(binaryHash));

        byte[] reported = Value(answer, "pub", KeyLen);
        byte[] signature = Value(answer, "sig", SigLen);

        if (!CryptographicOperations.FixedTimeEquals(reported, _devicePublicKey))
        {
            throw Fail("error.guard.wrongdevice");
        }

        byte[] signed = AttestMessage(nonce, binaryHash);
        if (!Ed25519Verify.Verify(_devicePublicKey, signed, signature))
        {
            throw Fail("error.guard.signature");
        }
    }

    /// <summary>
    /// Fragt in festem Abstand weiter und ruft <paramref name="lost"/> auf,
    /// sobald eine Runde scheitert - beim Abziehen also beim naechsten
    /// Durchgang. Danach kehrt die Methode zurueck. Ein geschuetztes
    /// Programm beendet sich in <paramref name="lost"/>.
    /// </summary>
    public void Watch(
        TimeSpan every,
        string binaryPath,
        Action<OskGuardException> lost,
        CancellationToken cancel = default)
    {
        if (every <= TimeSpan.Zero)
        {
            every = TimeSpan.FromSeconds(2);
        }

        while (!cancel.IsCancellationRequested)
        {
            if (cancel.WaitHandle.WaitOne(every))
            {
                return;
            }

            try
            {
                Present(binaryPath);
            }
            catch (OskGuardException error)
            {
                lost?.Invoke(error);
                return;
            }
        }
    }

    /// <summary>
    /// Verschliesst Daten mit einem Schluessel, der den Stick nie verlaesst.
    /// extra darf leer sein; ist es gesetzt, muss es beim Oeffnen gleich sein.
    /// </summary>
    public byte[] Seal(string label, ReadOnlySpan<byte> extra, ReadOnlySpan<byte> plain)
    {
        CheckPayload(plain.Length);
        Dictionary<string, string> answer = Ask(
            "seal " + TextAsHex(label) + " " + HexOrDash(extra) + " " + ToHex(plain));
        return Value(answer, "blob", 0);
    }

    /// <summary>
    /// Macht wieder auf. Ohne Stick gibt es den Schluessel nicht - dann kommt
    /// ein Fehler und keine halben Daten.
    /// </summary>
    public byte[] Unseal(string label, ReadOnlySpan<byte> extra, ReadOnlySpan<byte> blob)
    {
        CheckSize(blob.Length, MaxBlob);
        Dictionary<string, string> answer = Ask(
            "unseal " + TextAsHex(label) + " " + HexOrDash(extra) + " " + ToHex(blob));
        return Value(answer, "plain", 0);
    }

    /// <summary>
    /// Holt die Lizenz fuer ein Produkt und prueft ihre Unterschrift SELBST.
    ///
    /// Der Dienst liefert nur den ganzen unterschriebenen Block; fertige
    /// Felder gibt es bewusst nicht mehr. Sonst muesste die Bibliothek dem
    /// Dienst glauben - und eine falsche Attrappe auf der Rueckschleife
    /// koennte Lizenzen erfinden, ohne das Programm anzufassen. Gelesen wird
    /// erst, nachdem die Unterschrift des Herstellers gestimmt hat.
    /// </summary>
    public OskGuard.License License(string product)
    {
        if (_issuerPublicKey.Length != KeyLen)
        {
            // Ohne Herstellerschluessel ist nichts zu pruefen - dann lieber
            // ein Fehler als ein ungeprueftes Ergebnis.
            throw Fail("error.guard.issuer");
        }

        Dictionary<string, string> answer = Ask("license " + TextAsHex(product));
        byte[] blob = Value(answer, "lic", 0);
        return ReadLicense(blob, _issuerPublicKey);
    }

    /// <summary>
    /// Prueft einen OSKLIC1-Block und liest danach seine Felder. Steht hier
    /// offen, damit eine Lizenz auch aus einer Datei geprueft werden kann.
    /// </summary>
    public static OskGuard.License ReadLicense(
        ReadOnlySpan<byte> blob,
        ReadOnlySpan<byte> issuerPublicKey)
    {
        if (issuerPublicKey.Length != KeyLen)
        {
            throw Fail("error.guard.issuer");
        }

        // 1. Aufbau. Zuerst muss ueberhaupt ein Kopf da sein.
        if (blob.Length < LicenseHeadLen + Ed25519Verify.SignatureLen)
        {
            throw Fail("error.license.format");
        }

        for (int i = 0; i < LicenseMagic.Length; i++)
        {
            if (blob[i] != (byte)LicenseMagic[i])
            {
                throw Fail("error.license.format");
            }
        }

        if (blob[7] != LicenseVersion)
        {
            throw Fail("error.license.version");
        }

        // Laenge der Nutzlast, grosses Byte zuerst.
        int payloadLen = (blob[8] << 8) | blob[9];

        // Die Gesamtlaenge muss genau stimmen - kein Byte zu viel, keines zu
        // wenig. Angehaengter Unrat waere sonst Teil nichts Geprueftem.
        if (blob.Length != LicenseHeadLen + payloadLen + Ed25519Verify.SignatureLen)
        {
            throw Fail("error.license.format");
        }

        ReadOnlySpan<byte> payload = blob.Slice(LicenseHeadLen, payloadLen);
        ReadOnlySpan<byte> signature = blob.Slice(LicenseHeadLen + payloadLen);

        // 2. Unterschrift ueber "OSKLIC1" 0x00 <nutzlast>.
        byte[] signed = new byte[LicenseMagic.Length + 1 + payloadLen];
        Encoding.ASCII.GetBytes(LicenseMagic).CopyTo(signed, 0);
        signed[LicenseMagic.Length] = 0;
        payload.CopyTo(signed.AsSpan(LicenseMagic.Length + 1));

        if (!Ed25519Verify.Verify(issuerPublicKey, signed, signature))
        {
            throw Fail("error.license.signature");
        }

        // 3. Erst jetzt die Felder - aus der GEPRUEFTEN Nutzlast.
        return ReadPayload(payload);
    }

    /// <summary>Liest die CBOR-Abbildung der Nutzlast.</summary>
    private static OskGuard.License ReadPayload(ReadOnlySpan<byte> payload)
    {
        var reader = new CborReader(payload);
        int pairs = reader.ReadMapHeader();

        // Derselbe Schluessel darf nicht zweimal vorkommen: sonst gaebe es
        // zwei Lesarten derselben unterschriebenen Lizenz, je nachdem,
        // welcher Wert gewinnt.
        var seen = new HashSet<string>(StringComparer.Ordinal);

        string name = string.Empty;
        string holder = string.Empty;
        string number = string.Empty;
        var tiers = new List<string>();
        uint seats = 0;
        long issued = 0;
        long expires = 0;

        for (int i = 0; i < pairs; i++)
        {
            string key = reader.ReadTextString();
            if (!seen.Add(key))
            {
                throw Fail("error.license.format");
            }

            switch (key)
            {
                case "p":
                    name = reader.ReadTextString();
                    break;
                case "h":
                    holder = reader.ReadTextString();
                    break;
                case "nr":
                    number = reader.ReadTextString();
                    break;
                case "t":
                    int count = reader.ReadArrayHeader();
                    for (int at = 0; at < count; at++)
                    {
                        // Eine Ausbaustufe, die kein Text ist, und eine
                        // leere Ausbaustufe sind ein Fehler der Stufe
                        // selbst - nicht des Aufbaus.
                        if (reader.PeekMajor() != 3)
                        {
                            throw Fail("error.license.tier");
                        }

                        string tier = reader.ReadTextString();
                        if (tier.Length == 0)
                        {
                            throw Fail("error.license.tier");
                        }

                        tiers.Add(tier);
                    }

                    break;
                case "s":
                    seats = (uint)Limit(reader.ReadUnsigned(), uint.MaxValue);
                    break;
                case "i":
                    issued = (long)Limit(reader.ReadUnsigned(), long.MaxValue);
                    break;
                case "e":
                    expires = (long)Limit(reader.ReadUnsigned(), long.MaxValue);
                    break;
                default:
                    // Ein Schluessel, den diese Nutzlast nicht haben darf.
                    throw Fail("error.license.format");
            }
        }

        if (!reader.AtEnd)
        {
            // Hinter der Abbildung darf nichts mehr stehen.
            throw Fail("error.license.format");
        }

        if (name.Length == 0)
        {
            throw Fail("error.license.product");
        }

        return new OskGuard.License
        {
            Product = name,
            Holder = holder,
            Number = number,
            Tiers = tiers,
            Seats = seats,
            IssuedUtc = issued,
            ExpiresUtc = expires,
        };
    }

    /// <summary>
    /// Holt den oeffentlichen Teil eines Knotenschluessels. curve ist
    /// "ed25519" zum Unterschreiben oder "x25519" zum Schluesselaustausch.
    /// </summary>
    public byte[] NodePublic(string label, string curve)
    {
        Dictionary<string, string> answer = Ask(
            "nodepub " + TextAsHex(label) + " " + TextAsHex(curve));
        return Value(answer, "pub", KeyLen);
    }

    /// <summary>Laesst den Knoten einen Auftrag unterschreiben.</summary>
    public byte[] NodeSign(string label, ReadOnlySpan<byte> job)
    {
        CheckPayload(job.Length);
        Dictionary<string, string> answer = Ask(
            "nodesign " + TextAsHex(label) + " " + ToHex(job));
        return Value(answer, "sig", SigLen);
    }

    /// <summary>
    /// Prueft die Unterschrift eines anderen Knotens. Das braucht keinen
    /// Stick - nur den oeffentlichen Schluessel der Gegenseite.
    /// </summary>
    public static void NodeVerify(
        ReadOnlySpan<byte> publicKey,
        string label,
        ReadOnlySpan<byte> job,
        ReadOnlySpan<byte> signature)
    {
        if (publicKey.Length != KeyLen)
        {
            throw Fail("error.guard.key");
        }

        if (signature.Length != SigLen)
        {
            throw Fail("error.guard.signature");
        }

        if (string.IsNullOrEmpty(label) || job.Length == 0)
        {
            throw Fail("error.guard.size");
        }

        byte[] signed = SignMessage(label, job);
        if (!Ed25519Verify.Verify(publicKey, signed, signature))
        {
            throw Fail("error.guard.signature");
        }
    }

    /// <summary>
    /// Handelt ein gemeinsames Geheimnis mit der Gegenseite aus. Kann der
    /// gelieferte Chip kein ECDH, kommt error.guard.noecdh - siehe
    /// <see cref="OskGuardException.IsNoEcdh"/>.
    /// </summary>
    public byte[] NodeAgree(string label, ReadOnlySpan<byte> peerPublicKey)
    {
        if (peerPublicKey.Length != SecretLen)
        {
            throw Fail("error.guard.peerkey");
        }

        Dictionary<string, string> answer = Ask(
            "nodeagree " + TextAsHex(label) + " " + ToHex(peerPublicKey));
        return Value(answer, "sec", SecretLen);
    }

    /// <summary>
    /// Pruefsumme einer Programmdatei. Leerer Pfad: die laufende Datei.
    /// </summary>
    public static byte[] BinaryHash(string binaryPath)
    {
        string path = binaryPath;
        if (string.IsNullOrEmpty(path))
        {
            path = RunningBinary();
        }

        try
        {
            var info = new FileInfo(path);
            if (!info.Exists || info.Length > MaxBinary)
            {
                throw Fail("error.guard.binary");
            }

            using FileStream file = File.OpenRead(path);
            using var digest = SHA256.Create();
            return digest.ComputeHash(file);
        }
        catch (OskGuardException)
        {
            throw;
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException
            or ArgumentException or NotSupportedException)
        {
            throw Fail("error.guard.binary");
        }
    }

    /// <summary>Der Pfad der laufenden Programmdatei.</summary>
    private static string RunningBinary()
    {
        // Bei einer eigenstaendigen EXE ist das die EXE, bei `dotnet app.dll`
        // der Host. Beides ist die Datei, die gerade laeuft - und genau die
        // soll in die Pruefsumme.
        string? path = Environment.ProcessPath;
        if (string.IsNullOrEmpty(path))
        {
            path = Process.GetCurrentProcess().MainModule?.FileName;
        }

        if (string.IsNullOrEmpty(path))
        {
            throw Fail("error.guard.binary");
        }

        return path;
    }

    // ---- Nachrichten, die das Geraet unterschreibt ------------------------

    /// <summary>"osk-guard/attest/1" 0x00 nonce(32) binhash(32).</summary>
    private static byte[] AttestMessage(ReadOnlySpan<byte> nonce, ReadOnlySpan<byte> binaryHash)
    {
        byte[] domain = Encoding.ASCII.GetBytes(AttestDomain);
        var out1 = new byte[domain.Length + 1 + nonce.Length + binaryHash.Length];
        int at = 0;
        domain.CopyTo(out1, at);
        at += domain.Length;
        out1[at++] = 0;
        nonce.CopyTo(out1.AsSpan(at));
        at += nonce.Length;
        binaryHash.CopyTo(out1.AsSpan(at));
        return out1;
    }

    /// <summary>"osk-guard/sign/1" 0x00 label 0x00 sha256(job).</summary>
    private static byte[] SignMessage(string label, ReadOnlySpan<byte> job)
    {
        byte[] domain = Encoding.ASCII.GetBytes(SignDomain);
        byte[] name = Encoding.UTF8.GetBytes(label);
        byte[] jobHash = SHA256.HashData(job);

        var out1 = new byte[domain.Length + 1 + name.Length + 1 + jobHash.Length];
        int at = 0;
        domain.CopyTo(out1, at);
        at += domain.Length;
        out1[at++] = 0;
        name.CopyTo(out1, at);
        at += name.Length;
        out1[at++] = 0;
        jobHash.CopyTo(out1, at);
        return out1;
    }

    // ---- Das Drahtprotokoll ----------------------------------------------

    /// <summary>
    /// Schickt eine Zeile und liest die Antwort. Je Anfrage eine Verbindung:
    /// das ist langsamer, aber ein Programm kann so nie die Antwort einer
    /// fremden Anfrage lesen.
    /// </summary>
    private Dictionary<string, string> Ask(string line)
    {
        if (line.Length + 1 > MaxLine)
        {
            throw Fail("error.guard.size");
        }

        using var client = new TcpClient();
        try
        {
            if (!client.ConnectAsync(_host, _port).Wait(ConnectTimeout))
            {
                throw Fail("error.guard.nodevice");
            }

            using NetworkStream stream = client.GetStream();
            int millis = (int)Math.Clamp(Timeout.TotalMilliseconds, 1000, int.MaxValue);
            stream.ReadTimeout = millis;
            stream.WriteTimeout = millis;

            byte[] request = Encoding.ASCII.GetBytes(line + "\n");
            stream.Write(request, 0, request.Length);
            stream.Flush();

            using var buffered = new BufferedStream(stream, 64 * 1024);
            return ParseAnswer(ReadLine(buffered));
        }
        catch (OskGuardException)
        {
            throw;
        }
        catch (Exception error) when (error is IOException or SocketException
            or AggregateException or ObjectDisposedException or InvalidOperationException)
        {
            throw Fail("error.guard.nodevice");
        }
    }

    /// <summary>Liest eine Zeile, hoechstens 4 MiB.</summary>
    private static string ReadLine(Stream stream)
    {
        var out1 = new MemoryStream();
        var one = new byte[1];
        while (true)
        {
            int read = stream.Read(one, 0, 1);
            if (read <= 0)
            {
                if (out1.Length == 0)
                {
                    throw Fail("error.guard.nodevice");
                }

                break;
            }

            if (one[0] == (byte)'\n')
            {
                break;
            }

            if (one[0] == (byte)'\r')
            {
                continue;
            }

            if (out1.Length >= MaxLine)
            {
                throw Fail("error.guard.size");
            }

            out1.WriteByte(one[0]);
        }

        return Encoding.ASCII.GetString(out1.GetBuffer(), 0, (int)out1.Length);
    }

    /// <summary>Zerlegt "OK name=wert ..." oder "ERR code".</summary>
    private static Dictionary<string, string> ParseAnswer(string line)
    {
        string[] fields = line.Split(
            (char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        if (fields.Length == 0)
        {
            throw Fail("error.guard.answer");
        }

        if (fields[0] == "ERR")
        {
            if (fields.Length < 2)
            {
                throw Fail("error.guard.internal");
            }

            throw Fail(fields[1]);
        }

        if (fields[0] != "OK")
        {
            throw Fail("error.guard.answer");
        }

        var out1 = new Dictionary<string, string>(StringComparer.Ordinal);
        for (int i = 1; i < fields.Length; i++)
        {
            int cut = fields[i].IndexOf('=', StringComparison.Ordinal);
            if (cut > 0)
            {
                out1[fields[i].Substring(0, cut)] = fields[i].Substring(cut + 1);
            }
        }

        return out1;
    }

    /// <summary>Holt einen Hexwert aus der Antwort und prueft seine Laenge.</summary>
    private static byte[] Value(Dictionary<string, string> answer, string key, int want)
    {
        if (!answer.TryGetValue(key, out string? text) || text == "-")
        {
            throw Fail("error.guard.answer");
        }

        byte[] raw = FromHex(text);
        if (want > 0 && raw.Length != want)
        {
            throw Fail("error.guard.answer");
        }

        return raw;
    }

    /// <summary>Haelt eine Zahl in ihren Grenzen, sonst ein Formatfehler.</summary>
    private static ulong Limit(ulong value, ulong most)
    {
        if (value > most)
        {
            throw Fail("error.license.format");
        }

        return value;
    }

    // ---- Kleinigkeiten ----------------------------------------------------

    /// <summary>
    /// Zerlegt "host:port" und laesst nur die Rueckschleife zu: der Stick
    /// steckt hier, nicht im Netz.
    /// </summary>
    private static (string Host, int Port) SplitAddress(string address)
    {
        if (string.IsNullOrEmpty(address))
        {
            throw Fail("error.guard.request");
        }

        int cut = address.LastIndexOf(':');
        if (cut <= 0 || cut == address.Length - 1)
        {
            throw Fail("error.guard.request");
        }

        string host = address.Substring(0, cut).Trim('[', ']');
        string portText = address.Substring(cut + 1);
        if (!int.TryParse(portText, NumberStyles.None,
                CultureInfo.InvariantCulture, out int port) ||
            port < 1 || port > 65535)
        {
            throw Fail("error.guard.request");
        }

        bool loopback = host == "localhost" ||
            (IPAddress.TryParse(host, out IPAddress? ip) && IPAddress.IsLoopback(ip));
        if (!loopback)
        {
            throw Fail("error.guard.request");
        }

        return (host, port);
    }

    /// <summary>Prueft die Groesse der Nutzdaten einer Anfrage.</summary>
    private static void CheckPayload(int length)
    {
        CheckSize(length, MaxPayload);
    }

    /// <summary>Prueft eine Groesse gegen ihre Obergrenze.</summary>
    private static void CheckSize(int length, int most)
    {
        if (length == 0 || length > most)
        {
            throw Fail("error.guard.size");
        }
    }

    /// <summary>Bytes als Kleinbuchstaben-Hex.</summary>
    private static string ToHex(ReadOnlySpan<byte> raw)
    {
        return Convert.ToHexString(raw).ToLowerInvariant();
    }

    /// <summary>Hex zu Bytes; ungueltiges Hex ist eine kaputte Antwort.</summary>
    private static byte[] FromHex(string text)
    {
        try
        {
            return Convert.FromHexString(text);
        }
        catch (FormatException)
        {
            throw Fail("error.guard.answer");
        }
    }

    /// <summary>Ein Text als Hex, wie das Protokoll es verlangt.</summary>
    private static string TextAsHex(string text)
    {
        return ToHex(Encoding.UTF8.GetBytes(text));
    }

    /// <summary>Hex, oder "-" fuer "nicht gesetzt".</summary>
    private static string HexOrDash(ReadOnlySpan<byte> raw)
    {
        return raw.Length == 0 ? "-" : ToHex(raw);
    }

    /// <summary>Baut einen Fehler mit seinem Schluessel.</summary>
    private static OskGuardException Fail(string code)
    {
        return new OskGuardException(code);
    }

    /// <summary>
    /// Ein kleiner CBOR-Leser, genau so viel wie die Lizenznutzlast braucht:
    /// vorzeichenlose Zahl, Text, Feld und Abbildung, und davon nur die
    /// Laengenformen "unter 24", "ein Byte" und "zwei Byte". Alles andere
    /// wird abgewiesen - System.Formats.Cbor gehoert nicht zum Rahmenwerk,
    /// und ein Paket aus dem Netz ist hier nicht erlaubt.
    ///
    /// Jede Laenge wird gegen das Pufferende geprueft, bevor gelesen wird.
    /// </summary>
    private ref struct CborReader
    {
        private readonly ReadOnlySpan<byte> _raw;
        private int _at;

        public CborReader(ReadOnlySpan<byte> raw)
        {
            _raw = raw;
            _at = 0;
        }

        /// <summary>Sagt, ob alles gelesen ist.</summary>
        public bool AtEnd => _at >= _raw.Length;

        /// <summary>
        /// Nennt die Hauptsorte des naechsten Wertes, ohne ihn zu lesen.
        /// </summary>
        public int PeekMajor()
        {
            if (_at >= _raw.Length)
            {
                throw Fail("error.license.format");
            }

            return _raw[_at] >> 5;
        }

        /// <summary>Liest den Kopf einer Abbildung und nennt die Paarzahl.</summary>
        public int ReadMapHeader()
        {
            return Count(5);
        }

        /// <summary>Liest den Kopf eines Feldes und nennt die Zahl der Glieder.</summary>
        public int ReadArrayHeader()
        {
            return Count(4);
        }

        /// <summary>
        /// Eine Zahl von Gliedern. Jedes Glied braucht mindestens ein Byte,
        /// darum kann die Zahl nie groesser sein als der Rest des Puffers -
        /// so kann eine erfundene Riesenzahl keinen Schaden anrichten.
        /// </summary>
        private int Count(int wantMajor)
        {
            ulong value = Head(wantMajor);
            if (value > (ulong)(_raw.Length - _at))
            {
                throw Fail("error.license.format");
            }

            return (int)value;
        }

        /// <summary>Liest eine vorzeichenlose Zahl.</summary>
        public ulong ReadUnsigned()
        {
            return Head(0);
        }

        /// <summary>Liest einen Text.</summary>
        public string ReadTextString()
        {
            int length = Count(3);
            string text = Encoding.UTF8.GetString(_raw.Slice(_at, length));
            _at += length;
            return text;
        }

        /// <summary>
        /// Liest einen Kopf, prueft die Hauptsorte und gibt den Wert zurueck.
        /// </summary>
        private ulong Head(int wantMajor)
        {
            if (_at >= _raw.Length)
            {
                throw Fail("error.license.format");
            }

            byte first = _raw[_at++];
            int major = first >> 5;
            int minor = first & 0x1F;
            if (major != wantMajor)
            {
                throw Fail("error.license.format");
            }

            if (minor < 24)
            {
                return (ulong)minor;
            }

            // Die Zeitstempel `i` und `e` liegen bei rund 1,8 Milliarden
            // Sekunden - die brauchen die 4-Byte-Form. Darum gehoeren 26 und
            // 27 dazu, nicht nur bis 2 Byte.
            (int width, ulong least) = minor switch
            {
                24 => (1, 24UL),
                25 => (2, 0x100UL),
                26 => (4, 0x10000UL),
                27 => (8, 0x100000000UL),

                // Unbestimmte Laenge und die Nebensorten 28..31 gibt es hier
                // nicht.
                _ => (0, 0UL),
            };

            if (width == 0 || _at + width > _raw.Length)
            {
                throw Fail("error.license.format");
            }

            ulong value = 0;
            for (int i = 0; i < width; i++)
            {
                value = (value << 8) | _raw[_at + i];
            }

            _at += width;

            // Kuerzeste Schreibweise erzwingen: in unterschriebenen Daten
            // darf dieselbe Zahl nicht zweimal verschieden geschrieben
            // werden koennen, sonst gibt es zwei Lesarten einer Lizenz.
            if (value < least)
            {
                throw Fail("error.license.format");
            }

            return value;
        }
    }
}
