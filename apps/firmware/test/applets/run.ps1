# Prueflauf der Kartenanwendungen starten - von Windows aus.
#
# DIESES SKRIPT BAUT NICHTS SELBST. Es ruft das Makefile daneben in der
# WSL auf, und zwar genau so, wie es dort steht. Das ist Absicht:
#
#   Am 28.10.2026 hatte der SE050-Prueflauf ZWEI Baubeschreibungen - ein
#   Makefile und eine build.ps1 -, und die build.ps1 fuehrte eine
#   Quelldatei nicht. Der Windows-Weg band deshalb nie, und es sah aus
#   wie ein Fehler des Aufrufers statt wie eine fehlende Quelle. Zwei
#   Beschreibungen derselben Pruefung laufen auseinander; eine kann es
#   nicht.
#
# Es kann sein, dass dabei an einer ANDEREN STELLE gebaut wird als dort,
# wo das Repo liegt - siehe "Netzlaufwerke". Das aendert WO gebaut wird,
# nicht WIE: gerufen wird dasselbe Makefile mit denselben Quellen. Eine
# zweite Baubeschreibung entsteht dadurch nicht.
#
# Warum WSL und nicht die Visual Studio Build Tools: der Pruefsatz laeuft
# unter AddressSanitizer und UndefinedBehaviorSanitizer, und das
# Pruefgeruest (test/ctap/test_util.h) benutzt GCC-Erweiterungen. Beides
# gibt es mit cl nicht. Am Geraet uebersetzt ohnehin die Werkzeugkette des
# nRF Connect SDK, nicht die hier.
#
# ---------------------------------------------------------------------
# ZWEI DINGE, DIE DIESES SKRIPT SCHON ZWEIMAL GEBROCHEN HABEN, und beide
# sind dasselbe Muster: zwei Wege, die gleich heissen und sich verschieden
# verhalten.
#
# 1. NETZLAUFWERKE. Das Repo liegt auf Z:, und Z: ist ein Netzlaufwerk.
#    `wslpath` scheitert dort mit "Failed to translate 'Z:\...'" - die WSL
#    laeuft, sie kommt nur an den Pfad nicht heran. Darum spiegelt dieses
#    Skript den Baum apps/firmware in ein lokales Verzeichnis unter
#    $env:TEMP und ruft das Makefile von dort. Gespiegelt wird der GANZE
#    Baum und keine Dateiliste - eine Liste waere genau die zweite
#    Baubeschreibung, die hier vermieden werden soll.
#
#    Gefunden am 29.10.2026 von der Leitsitzung. Mir war es nicht
#    aufgefallen, weil mein Arbeitsbaum unter D: lag und es dort geht.
#
# 2. WINDOWS POWERSHELL 5.1 GEGEN POWERSHELL 7. Fremde Programme, die auf
#    die Fehlerausgabe schreiben, behandeln die beiden verschieden: in 5.1
#    macht `$ErrorActionPreference = 'Stop'` daraus einen ABBRECHENDEN
#    NativeCommandError, und zwar BEVOR das Skript die Rueckgabe ansehen
#    kann. `2>$null` faengt das dort NICHT ab. In 7 passiert nichts
#    davon.
#
#    Folge: die Spiegelung aus Punkt 1 wurde in 5.1 nie erreicht - das
#    Skript brach genau an der Stelle ab, an der es den Netzlaufwerkfall
#    erkennen wollte. Und 5.1 ist der Fall, der zaehlt: ein Doppelklick
#    startet 5.1, nicht 7.
#
#    Darum laeuft JEDER Aufruf eines fremden Programms hier durch
#    Invoke-Native (siehe unten). Nichts in diesem Skript ruft wsl.exe
#    oder robocopy direkt.
#
#    Gefunden am 29.10.2026 von der Leitsitzung, nachdem Punkt 1 behoben
#    war. Ich hatte in 7 gemessen.
# ---------------------------------------------------------------------
#
#   .\run.ps1 -Openssl  - dazu den Beleg mit einem FREMDEN Programm:
#                         openssl liest die Zertifikatsanfrage und
#                         bestaetigt ihre Unterschrift (docs/csr.md).
#                         Braucht openssl und python3 in der WSL.
#
#   .\run.ps1           - die Fassung MIT RSA
#   .\run.ps1 -NoRsa    - die Fassung OHNE RSA (CONFIG_OSK_RSA nicht gesetzt)
#   .\run.ps1 -Both      - beide hintereinander
#   .\run.ps1 -Fuzz      - dazu den Fuzzlauf
#   .\run.ps1 -Cc clang-19 - mit dem zweiten Uebersetzer
#
# Laeuft in Windows PowerShell 5.1 und in PowerShell 7; geprueft wird in
# BEIDEN, weil sie sich eben nicht gleich verhalten.
#
# BEIDE FIRMWARE-FASSUNGEN muessen gebaut und geprueft werden: eine
# Option, die nur in einer Richtung geprueft ist, ist keine
# (docs/plan.md, Abschnitt 8).

param(
    [switch]$NoRsa,
    [switch]$Both,
    [switch]$Fuzz,
    [switch]$Openssl,
    [string]$Cc = 'gcc',
    [int]$FuzzSecs = 20
)

$ErrorActionPreference = 'Stop'

# Ein fremdes Programm rufen, ohne dass seine Fehlerausgabe das Skript
# abbricht. Das ist der Kern der Nachbesserung von Punkt 2 oben.
#
# Drei Vorkehrungen, und keine davon ist entbehrlich:
#   - $ErrorActionPreference steht hier auf 'Continue'. Die Zuweisung ist
#     funktionsweit, wirkt also nur hier und nicht im ganzen Skript.
#   - 2>&1 fuehrt die Fehlerausgabe in die normale Ausgabe. Damit entsteht
#     in 5.1 kein Fehlersatz mehr, den 'Stop' zum Abbruch machen koennte.
#   - try/catch faengt, was trotzdem noch fliegt. Ein Fehlschlag ist hier
#     ein ERGEBNIS und kein Unfall: "die WSL erreicht den Pfad nicht" ist
#     genau die Antwort, auf die das Skript wartet.
#
# Rueckgabe: Zeilen als Text, und der Ausstiegscode.
function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$Exe,
        [string[]]$Arguments = @()
    )

    $ErrorActionPreference = 'Continue'
    $lines = @()
    $code = 0
    try {
        $raw = & $Exe @Arguments 2>&1
        $code = $LASTEXITCODE
        foreach ($r in $raw) {
            if ($null -ne $r) { $lines += $r.ToString() }
        }
    } catch {
        $lines += $_.Exception.Message
        if ($LASTEXITCODE) { $code = $LASTEXITCODE } else { $code = 1 }
    }
    return [pscustomobject]@{ Lines = $lines; Code = $code }
}

# Einen Windows-Pfad in den Pfad uebersetzen, unter dem die WSL dieselbe
# Datei sieht. Rueckgabe: der Pfad, oder $null wenn die WSL ihn nicht
# erreicht.
#
# Gesucht wird eine Zeile, die mit / anfaengt - und zwar unter ALLEN
# Zeilen. Zwei Gruende: bei einem Netzlaufwerk gibt wslpath den
# Windows-Pfad unveraendert zurueck (der faengt nicht mit / an), und durch
# 2>&1 stehen Fehlermeldungen in derselben Ausgabe. Wer nur "leer oder
# nicht" prueft oder nur die erste Zeile ansieht, haelt eine Absage fuer
# ein Ergebnis.
function Get-WslPath([string]$winPath) {
    $r = Invoke-Native -Exe 'wsl.exe' -Arguments @('-e', 'wslpath', '-a', $winPath)
    if ($r.Code -ne 0) { return $null }
    foreach ($line in $r.Lines) {
        $p = $line.Trim()
        if ($p.StartsWith('/')) { return $p }
    }
    return $null
}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path

# Ab hier wird aus einem LOKALEN Verzeichnis gearbeitet. Grund: wsl.exe
# uebersetzt beim Start sein eigenes Arbeitsverzeichnis, und steht das auf
# Z:, schreibt es bei JEDEM Aufruf "Failed to translate 'Z:\...'" auf die
# Fehlerausgabe. Am Ergebnis aendert das nichts - die Meldung liest sich
# aber wie ein Fehlschlag, und wer sie sieht, sucht an der falschen
# Stelle. Das Verzeichnis wird am Ende zurueckgesetzt.
$startedIn = (Get-Location).Path
Set-Location ([System.IO.Path]::GetTempPath())
try {

# Laeuft die WSL ueberhaupt? Das wird getrennt gefragt und getrennt
# gemeldet, damit die Meldung hinterher sagen kann, was wirklich los ist.
# Die erste Fassung schickte den Leser bei einem Netzlaufwerk mit
# "laeuft die WSL?" in die falsche Richtung - sie laeuft ja.
$r = Invoke-Native -Exe 'wsl.exe' -Arguments @('-e', 'bash', '-lc', 'echo ja')
if (($r.Lines -join '').Trim() -ne 'ja') {
    throw ('Die WSL antwortet nicht. Von Hand versuchen: ' +
           'wsl.exe -e bash -lc "echo ja"' + [Environment]::NewLine +
           'Antwort war: ' + ($r.Lines -join ' | '))
}

$r = Invoke-Native -Exe 'wsl.exe' -Arguments @('-e', 'bash', '-lc',
    "command -v $Cc >/dev/null && command -v make >/dev/null && echo ja")
if (($r.Lines -join '').Trim() -ne 'ja') {
    throw "In der WSL fehlt $Cc oder make. Nachinstallieren: sudo apt install build-essential"
}

# Der Beleg mit dem fremden Programm braucht zwei weitere Werkzeuge. Hier
# wird ABGEBROCHEN und nicht uebersprungen: wer -Openssl angibt, will den
# Beleg, und ein stillschweigend uebersprungener Beleg sieht aus wie ein
# erbrachter.
if ($Openssl) {
    $r = Invoke-Native -Exe 'wsl.exe' -Arguments @('-e', 'bash', '-lc',
        'command -v openssl >/dev/null && command -v python3 >/dev/null && echo ja')
    if (($r.Lines -join '').Trim() -ne 'ja') {
        throw ('In der WSL fehlt openssl oder python3 - ohne die ist der Beleg ' +
               'nicht zu erbringen. Nachinstallieren: sudo apt install openssl python3')
    }
}

$wslHere = Get-WslPath $here
$mirrored = $false

if (-not $wslHere) {
    # Der uebliche Fall auf Z:. Nicht abbrechen, sondern spiegeln.
    $firmware = (Resolve-Path (Join-Path $here '..\..')).Path
    $mirror = Join-Path $env:TEMP 'osk-applets-pruefung'
    $mirrorFw = Join-Path $mirror 'firmware'

    if (-not (Get-WslPath $env:TEMP)) {
        throw ("Die WSL erreicht weder '$here' noch '$env:TEMP'. Beide liegen " +
               'vermutlich auf einem Netzlaufwerk oder einer UNC-Freigabe, und die ' +
               'kann die WSL nicht uebersetzen ("Failed to translate"). Die WSL ' +
               'selbst laeuft. Wenigstens TEMP muss auf einem lokalen Laufwerk liegen.')
    }

    Write-Host "Der Pfad '$here' ist aus der WSL nicht erreichbar - er liegt auf einem"
    Write-Host 'Laufwerk, das die WSL nicht uebersetzen kann (Netzlaufwerk oder UNC).'
    Write-Host 'Der Baum wird dafuer in ein lokales Verzeichnis gespiegelt:'
    Write-Host "  $mirrorFw"

    New-Item -ItemType Directory -Force -Path $mirror | Out-Null
    # /MIR raeumt mit: was in der Quelle fehlt, verschwindet im Ziel. So
    # bleiben keine Objekte eines alten Laufs stehen. build-Verzeichnisse
    # werden gar nicht erst mitgenommen.
    $r = Invoke-Native -Exe 'robocopy' -Arguments @($firmware, $mirrorFw, '/MIR',
        '/XD', 'build', '/NFL', '/NDL', '/NJH', '/NJS', '/NP')
    # robocopy meldet 0 bis 7 als Erfolg, ab 8 ist es ein Fehler. Auch das
    # geht durch Invoke-Native: robocopy schreibt bei Kleinigkeiten auf die
    # Fehlerausgabe, und in 5.1 waere das sonst wieder ein Abbruch.
    if ($r.Code -ge 8) {
        throw ("Das Spiegeln nach '$mirrorFw' ist fehlgeschlagen (robocopy $($r.Code))." +
               [Environment]::NewLine + ($r.Lines -join [Environment]::NewLine))
    }

    $here = Join-Path $mirrorFw 'test\applets'
    $wslHere = Get-WslPath $here
    if (-not $wslHere) {
        throw "Auch '$here' ist aus der WSL nicht erreichbar. Liegt TEMP auf einem Netzlaufwerk?"
    }
    $mirrored = $true
}

function Invoke-Run([string]$rsa, [string]$what) {
    Write-Host ''
    Write-Host "=== $what ==="
    # Jeder Lauf raeumt vorher auf: sonst bleiben Objekte der anderen
    # Fassung liegen, und dann ist nicht mehr klar, WAS gelaufen ist.
    $cmd = "cd '$wslHere' && make clean >/dev/null && make CC=$Cc RSA=$rsa"
    if ($Fuzz) {
        $cmd += " && make CC=$Cc RSA=$rsa fuzz FUZZ_SECS=$FuzzSecs"
    }
    if ($Openssl) {
        $cmd += " && make CC=$Cc RSA=$rsa openssl"
    }
    # Der Ausstiegscode der WSL ist nicht verlaesslich durchgereicht -
    # darum wird er auf der Linux-Seite in eine Zeile geschrieben und
    # hier gelesen. Ein gruener Lauf ohne diese Zeile ist nicht
    # durchgelaufen.
    $cmd += "; echo OSK_EXIT=`$?"
    $r = Invoke-Native -Exe 'wsl.exe' -Arguments @('-e', 'bash', '-lc', $cmd)
    $r.Lines | ForEach-Object { Write-Host $_ }
    $line = $r.Lines | Where-Object { $_ -match '^OSK_EXIT=' } | Select-Object -Last 1
    if (-not $line) {
        throw "$what : der Lauf hat keinen Ausstiegscode gemeldet."
    }
    $code = [int]($line -replace '^OSK_EXIT=', '')
    if ($code -ne 0) {
        throw "$what : rot (Code $code)."
    }
    Write-Host "$what : gruen."
}

if ($Both) {
    Invoke-Run '1' 'Fassung MIT RSA'
    Invoke-Run '0' 'Fassung OHNE RSA'
} elseif ($NoRsa) {
    Invoke-Run '0' 'Fassung OHNE RSA'
} else {
    Invoke-Run '1' 'Fassung MIT RSA'
}

Write-Host ''
Write-Host ("Gelaufen mit PowerShell $($PSVersionTable.PSVersion) " +
            "($($PSVersionTable.PSEdition)).")
if ($mirrored) {
    Write-Host "Gebaut wurde in der Spiegelung unter '$here'."
    Write-Host 'Dasselbe Makefile, dieselben Quellen - nur ein anderer Ort. Das Repo'
    Write-Host 'ist dabei nicht angefasst worden.'
}
Write-Host 'Das war Stufe 2 von vier: uebersetzt und am PC gelaufen.'
Write-Host 'Im Abbild und am Geraet ist nichts davon gelaufen.'

} finally {
    Set-Location $startedIn
}
