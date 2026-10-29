# Prueflauf und Fuzzlauf des Strangs E bauen und starten - am PC, ohne
# Hardware.
#
# Braucht nur die Visual Studio Build Tools; keine Fremdbibliothek. Der
# Weg ueber das Makefile geht auf diesem Rechner NICHT: es gibt kein gcc
# und kein make (gemessen mit "which gcc cc clang make" - kein Treffer).
# Darum ist dies der Weg, auf dem die Pruefungen dieses Strangs wirklich
# gelaufen sind.
#
# Die Quellen stehen in sources_yk_lib.txt und sources_yk_test.txt und
# NICHT hier - siehe die Begruendung in sources_yk_lib.txt.
#
#   .\build_yk.ps1              bauen und Pruefungen laufen lassen
#   .\build_yk.ps1 -Fuzz        danach den Fuzzlauf, 20 Sekunden
#   .\build_yk.ps1 -Fuzz -FuzzSeconds 300 -Seed 7
#   .\build_yk.ps1 -Asan        mit Adress-Pruefer (braucht VS 2019 16.9+)
#
# Aufruf aus dem Repo-Stamm oder von hier - beides geht.

param(
    [switch]$Fuzz,
    [int]$FuzzSeconds = 20,
    [string]$Seed = '',
    [switch]$Asan
)

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$firmware = Resolve-Path (Join-Path $here '..\..')
$src = Join-Path $firmware 'src\classes\transport'
$inc = Join-Path $firmware 'include'
# Unter build\, weil .gitignore genau diesen Namen aussperrt. Ein
# eigener Ordner daneben waere im Baum gelandet.
$out = Join-Path $here 'build\yk'

# Die Build Tools bringen ihre Umgebung selbst mit; der Pfad kann je
# Rechner anders sein, darum wird gesucht statt geraten.
$vcvars = Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat' -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $vcvars) {
    throw 'vcvars64.bat nicht gefunden - Visual Studio Build Tools fehlen.'
}

New-Item -ItemType Directory -Force -Path $out | Out-Null

# Liest eine Quellenliste: Bemerkungen und leere Zeilen heraus, Pfade
# aufloesen. Fehlt eine Datei, bricht es HIER ab und nicht erst beim
# Binden - eine fehlende Quelle soll wie eine fehlende Quelle aussehen.
function Read-Sources([string]$listFile) {
    $path = Join-Path $here $listFile
    if (-not (Test-Path $path)) { throw "Quellenliste fehlt: $listFile" }
    $files = @()
    foreach ($line in Get-Content $path) {
        $t = $line.Trim()
        if ($t -eq '' -or $t.StartsWith('#')) { continue }
        $full = Join-Path $here $t
        if (-not (Test-Path $full)) {
            throw "In $listFile steht eine Datei, die es nicht gibt: $t"
        }
        $files += (Resolve-Path $full).Path
    }
    if ($files.Count -eq 0) { throw "Quellenliste $listFile ist leer." }
    return $files
}

$lib = Read-Sources 'sources_yk_lib.txt'
$tests = Read-Sources 'sources_yk_test.txt'

# /W4 /WX: jede Warnung ist ein Fehler. Bei einem Parser, den jede
# Software auf dem Rechner erreicht, ist eine uebergangene Warnung keine
# Kleinigkeit.
#
# /RTC1 statt eines Adress-Pruefers: auf diesem Rechner gibt es KEINEN.
# Gemessen am 29.10.2026 - /fsanitize=address bricht beim Binden ab
# ("clang_rt.asan_static_runtime_thunk-x86_64.lib kann nicht geoeffnet
# werden"), die Laufzeitbibliothek ist mit diesen Build Tools nicht
# installiert, und clang-cl, clang und gcc sind nicht da.
# /RTC1 prueft Stapelueberlaeufe und nicht gesetzte Variablen zur
# Laufzeit. Das ist WENIGER als ein Adress-Pruefer: ein Lesen neben
# einen Puffer im BSS findet es nicht. Der Fuzzer legt darum selbst
# Wachbytes um den Bericht (siehe fuzz_ykhid.c). Der vollstaendige Lauf
# mit Adress- und Verhaltenspruefer geht ueber das Makefile auf einem
# Rechner mit gcc - siehe dort das Ziel fuzz-yk.
$flags = '/nologo /W4 /WX /std:c17 /Od /Zi /RTC1'
$link = ''
if ($Asan) {
    $flags = $flags.Replace(' /RTC1', '') + ' /fsanitize=address'
    Write-Host 'Mit Adress-Pruefer (braucht die ASAN-Laufzeit; siehe Kommentar oben).'
}

function Build([string]$exeName, [string[]]$sources) {
    $quoted = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $exe = Join-Path $out $exeName
    $obj = Join-Path $out ([System.IO.Path]::GetFileNameWithoutExtension($exeName))
    New-Item -ItemType Directory -Force -Path $obj | Out-Null
    # Der Schraegstrich am Ende muss doppelt stehen: ein einzelner
    # entwertet das Anfuehrungszeichen danach, und cl sieht dann einen
    # Ordner mit Anfuehrungszeichen im Namen.
    $cmd = "`"$vcvars`" >nul 2>&1 && cl $flags " +
           "/I `"$inc`" /I `"$src`" /I `"$here`" $quoted " +
           "/Fo:`"$obj\\`" /Fd:`"$obj\\vc.pdb`" /Fe:`"$exe`" $link"
    # Die Ausgabe von cl wird mitgeschrieben: unter manchen Aufrufern
    # kommt sie sonst nicht an, und ein Uebersetzungsfehler ohne Text
    # ist kein Befund, sondern ein Raten.
    $log = Join-Path $out ([System.IO.Path]::GetFileNameWithoutExtension($exeName) + '.log')
    cmd /c "$cmd > `"$log`" 2>&1"
    if ($LASTEXITCODE -ne 0) {
        Get-Content $log | Write-Host
        throw "Uebersetzen von $exeName fehlgeschlagen (Code $LASTEXITCODE). Text in $log"
    }
    return $exe
}

Write-Host 'Uebersetze Pruefungen...'
$testExe = Build 'yk_tests.exe' ($lib + $tests)

Write-Host 'Uebersetze Fuzzer...'
$fuzzExe = Build 'fuzz_ykhid.exe' ($lib + @((Resolve-Path (Join-Path $here 'fuzz_ykhid.c')).Path))

Write-Host ''
& $testExe
$rc = $LASTEXITCODE
if ($rc -ne 0) { exit $rc }

if ($Fuzz) {
    Write-Host ''
    Write-Host "Fuzzlauf, $FuzzSeconds Sekunden..."
    if ($Seed -ne '') { & $fuzzExe $FuzzSeconds $Seed } else { & $fuzzExe $FuzzSeconds }
    $rc = $LASTEXITCODE
}
exit $rc
