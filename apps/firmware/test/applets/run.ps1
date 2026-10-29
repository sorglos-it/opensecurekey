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
# wo das Repo liegt - siehe unten. Das aendert WO gebaut wird, nicht WIE:
# gerufen wird dasselbe Makefile mit denselben Quellen. Eine zweite
# Baubeschreibung entsteht dadurch nicht.
#
# Warum WSL und nicht die Visual Studio Build Tools: der Pruefsatz laeuft
# unter AddressSanitizer und UndefinedBehaviorSanitizer, und das
# Pruefgeruest (test/ctap/test_util.h) benutzt GCC-Erweiterungen. Beides
# gibt es mit cl nicht. Am Geraet uebersetzt ohnehin die Werkzeugkette des
# nRF Connect SDK, nicht die hier.
#
# ---------------------------------------------------------------------
# NETZLAUFWERKE: die WSL kann sie nicht uebersetzen.
#
# Das Repo liegt auf Z:, und Z: ist ein Netzlaufwerk. `wslpath` scheitert
# dort mit "Failed to translate 'Z:\...'" - die WSL laeuft, sie kommt nur
# an den Pfad nicht heran. Gemeldet am 29.10.2026 von der Leitsitzung; mir
# war es nicht aufgefallen, weil mein Arbeitsbaum unter D: lag und es dort
# geht. Das Repo liegt aber auf Z:, und dort liegt es auch bei jedem, der
# es nachprueft.
#
# Darum: laesst sich der Pfad nicht uebersetzen, spiegelt dieses Skript
# den Baum apps/firmware in ein lokales Verzeichnis unter $env:TEMP und
# ruft das Makefile von dort. Gespiegelt wird der GANZE Baum und keine
# Dateiliste - eine Liste waere genau die zweite Baubeschreibung, die hier
# vermieden werden soll.
# ---------------------------------------------------------------------
#
#   .\run.ps1           - die Fassung MIT RSA
#   .\run.ps1 -NoRsa    - die Fassung OHNE RSA (CONFIG_OSK_RSA nicht gesetzt)
#   .\run.ps1 -Both      - beide hintereinander
#   .\run.ps1 -Fuzz      - dazu den Fuzzlauf
#   .\run.ps1 -Cc clang-19 - mit dem zweiten Uebersetzer
#
# BEIDE FASSUNGEN muessen gebaut und geprueft werden: eine Option, die nur
# in einer Richtung geprueft ist, ist keine (docs/plan.md, Abschnitt 8).

param(
    [switch]$NoRsa,
    [switch]$Both,
    [switch]$Fuzz,
    [string]$Cc = 'gcc',
    [int]$FuzzSecs = 20
)

$ErrorActionPreference = 'Stop'

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

# Einen Windows-Pfad in den Pfad uebersetzen, unter dem die WSL dieselbe
# Datei sieht. Rueckgabe: der Pfad, oder $null wenn die WSL ihn nicht
# erreicht.
#
# Geprueft wird BEIDES - der Ausstiegscode und die Form der Antwort. Bei
# einem Netzlaufwerk schreibt wslpath den Windows-Pfad unveraendert
# zurueck, und der faengt nicht mit / an; wer nur auf "leer oder nicht"
# prueft, haelt diese Antwort fuer brauchbar.
function Get-WslPath([string]$winPath) {
    $out = & wsl.exe -e wslpath -a $winPath 2>$null
    if ($LASTEXITCODE -ne 0) { return $null }
    $p = ($out | Out-String).Trim()
    if (-not $p -or -not $p.StartsWith('/')) { return $null }
    return $p
}

# Laeuft die WSL ueberhaupt? Das wird getrennt gefragt, damit die Meldung
# hinterher sagen kann, was wirklich los ist. Die alte Fassung schickte
# den Leser bei einem Netzlaufwerk mit "laeuft die WSL?" in die falsche
# Richtung - sie laeuft ja.
$alive = & wsl.exe -e bash -lc 'echo ja' 2>$null
if (($alive | Out-String).Trim() -ne 'ja') {
    throw 'Die WSL antwortet nicht. Von Hand versuchen: wsl.exe -e bash -lc "echo ja"'
}

$have = & wsl.exe -e bash -lc "command -v $Cc >/dev/null && command -v make >/dev/null && echo ja" 2>$null
if (($have | Out-String).Trim() -ne 'ja') {
    throw "In der WSL fehlt $Cc oder make. Nachinstallieren: sudo apt install build-essential"
}

$wslHere = Get-WslPath $here
$mirrored = $false

if (-not $wslHere) {
    # Der uebliche Fall auf Z:. Nicht abbrechen, sondern spiegeln.
    $firmware = Resolve-Path (Join-Path $here '..\..')
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
    & robocopy $firmware $mirrorFw /MIR /XD build /NFL /NDL /NJH /NJS /NP | Out-Null
    # robocopy meldet 0 bis 7 als Erfolg, ab 8 ist es ein Fehler.
    if ($LASTEXITCODE -ge 8) {
        throw "Das Spiegeln nach '$mirrorFw' ist fehlgeschlagen (robocopy $LASTEXITCODE)."
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
    # Der Ausstiegscode der WSL ist nicht verlaesslich durchgereicht -
    # darum wird er auf der Linux-Seite in eine Zeile geschrieben und
    # hier gelesen. Ein gruener Lauf ohne diese Zeile ist nicht
    # durchgelaufen.
    $cmd += "; echo OSK_EXIT=`$?"
    $out = & wsl.exe -e bash -lc $cmd 2>&1
    $out | ForEach-Object { Write-Host $_ }
    $line = $out | Where-Object { $_ -match '^OSK_EXIT=' } | Select-Object -Last 1
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
