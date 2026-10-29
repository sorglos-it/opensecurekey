# Prueflauf des CTAP-Kerns bauen und starten - am PC, ohne Hardware.
#
# Zweiter Weg neben dem Makefile. Grund: auf diesem Rechner gibt es weder
# gcc noch make; `python tools/check.py` meldet firmware/tests darum als
# UEBERSPRUNGEN und fuehrt diese Pruefungen gar nicht aus. Ein gruenes
# check.py ist fuer den CTAP-Teil also keine Aussage. Dieses Skript ist der
# Weg, auf dem hier wirklich gemessen wurde.
#
# Braucht nur die Visual Studio Build Tools; keine Fremdbibliothek.
#
# Vorbild: test/se050/build.ps1. Dort fehlte osk_mem.c, das das Makefile
# fuehrte - zwei Baubeschreibungen derselben Pruefung, die sich
# widersprachen. Darum steht die Quellenliste hier Zeile fuer Zeile neben
# der des Makefiles, und `-Vergleich` prueft sie gegeneinander.

param(
    [switch]$Vergleich,   # nur pruefen, ob Quellenliste und Makefile zusammenpassen
    [switch]$Fuzz,        # statt der Pruefungen die beiden Fuzzlaeufe
    [int]$FuzzSekunden = 20,
    [string]$Nur = ''     # nur eine Pruefung bauen und starten, z.B. -Nur test_sigcount
)

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$firmware = (Resolve-Path (Join-Path $here '..\..')).Path
$src = Join-Path $firmware 'src\classes\ctap'
$trans = Join-Path $firmware 'src\classes\transport'
$inc = Join-Path $firmware 'include'
# Unter build\: der Ordner steht schon in .gitignore des Repos. Ein eigener
# Name daneben waere der erste erzeugte Ordner, der versehentlich mit
# eingecheckt wird.
$out = Join-Path $here 'build\msvc'

# Der Kern, genau die Liste CORE aus dem Makefile.
$core = @(
    'osk_cbor.c', 'osk_crypto.c', 'osk_credid.c', 'osk_ctap_key.c',
    'osk_ctap_ext.c', 'osk_ctap_core.c', 'osk_ctap_u2f.c', 'osk_pin.c',
    'osk_ctap_cm.c', 'osk_ctap_config.c', 'osk_sigcount.c'
) | ForEach-Object { Join-Path $src $_ }
$core += (Join-Path $trans 'usb_hid.c')
$core += (Join-Path $firmware 'src\osk_mem.c')

$tests = @('test_crypto', 'test_cbor', 'test_credid', 'test_pin', 'test_ctap',
           'test_u2f', 'test_hid', 'test_sigcount', 'test_credmgmt', 'test_config')

if ($Vergleich) {
    # Eine Zahl nur an einer Stelle: die Quellenliste steht zweimal (hier und
    # im Makefile), also wird sie verglichen statt geglaubt.
    $mk = Get-Content (Join-Path $here 'Makefile') -Raw
    $mkCore = [regex]::Matches($mk, 'osk_[a-z0-9_]+\.c|usb_hid\.c') |
        ForEach-Object { $_.Value } | Sort-Object -Unique
    $psCore = $core | ForEach-Object { Split-Path $_ -Leaf } | Sort-Object -Unique
    $nurMk = $mkCore | Where-Object { $psCore -notcontains $_ }
    $nurPs = $psCore | Where-Object { $mkCore -notcontains $_ }
    if ($nurMk) { Write-Host "nur im Makefile: $($nurMk -join ', ')" }
    if ($nurPs) { Write-Host "nur in build.ps1: $($nurPs -join ', ')" }
    if (-not $nurMk -and -not $nurPs) { Write-Host 'Quellenlisten gleich: ok' }
    # Fortsetzungszeilen zuerst zusammenziehen: ohne das sieht der Vergleich
    # nur die erste Zeile und meldet die uebrigen Pruefungen als fehlend.
    $mkJoined = $mk -replace "\\\r?\n", ' '
    $mkTests = [regex]::Match($mkJoined, 'TESTS\s*:=\s*(.+)').Groups[1].Value -split '\s+' |
        Where-Object { $_ }
    $nurT = @($mkTests | Where-Object { $tests -notcontains $_ }) +
            @($tests | Where-Object { $mkTests -notcontains $_ })
    if ($nurT) { Write-Host "Pruefungslisten weichen ab: $($nurT -join ', ')" }
    else { Write-Host 'Pruefungslisten gleich: ok' }
    exit 0
}

$vcvars = Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat' -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $vcvars) {
    throw 'vcvars64.bat nicht gefunden - Visual Studio Build Tools fehlen.'
}

New-Item -ItemType Directory -Force -Path $out | Out-Null

if ($Fuzz) { $tests = @('fuzz_cbor', 'fuzz_hid') }
if ($Nur) { $tests = @($Nur) }

# /W3 /WX: jede Warnung ist ein Fehler. Der CBOR-Leser nimmt Bytes vom Draht
# entgegen; eine uebergangene Warnung ist da keine Kleinigkeit.
# /analyze waere schoen, dauert aber Minuten - das Makefile hat dafuer die
# Sanitizer, die es hier nicht gibt. Das ist der Unterschied der beiden Wege
# und steht so im Bericht.
$flags = '/nologo /std:c11 /W3 /WX /Od /Zi /DOSK_CTAP_TEST_CLOCK /D_CRT_SECURE_NO_WARNINGS'
$incs = "/I`"$inc`" /I`"$src`" /I`"$trans`" /I`"$here`""

$fail = 0
foreach ($t in $tests) {
    $srcs = @((Join-Path $here "$t.c"))
    if ($t -eq 'test_crypto') {
        $srcs += (Join-Path $src 'osk_crypto.c'); $srcs += (Join-Path $firmware 'src\osk_mem.c')
    } elseif ($t -eq 'test_cbor') {
        $srcs += (Join-Path $src 'osk_cbor.c')
    } else {
        $srcs += $core; $srcs += (Join-Path $here 'mocks.c')
    }
    # Die Fuzzlaeufe bringen ihre eigene Hauptfunktion mit, brauchen aber
    # denselben Kern und dieselben Attrappen.
    $objdir = Join-Path $out $t
    New-Item -ItemType Directory -Force -Path $objdir | Out-Null
    $list = ($srcs | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $exe = Join-Path $out "$t.exe"
    # Ueber eine Stapeldatei statt ueber `cmd /c "..."`: die Anfuehrungszeichen
    # um Pfade mit Rueckstrich am Ende ueberleben den Weg durch cmd sonst
    # nicht, und cl bekommt alles als EIN Argument.
    $bat = Join-Path $objdir 'bau.bat'
    # Jede Zeile einzeln: in PowerShell bindet das Komma STAERKER als das
    # Plus. Ohne eigene Variablen wird aus einer Befehlszeile ein Haufen
    # Einzelteile, und die Stapeldatei ruft dann "C:\Program" auf.
    $l1 = '@echo off'
    # 2>&1 nach nul: vcvars64.bat beklagt auf diesem Rechner ein fehlendes
    # vswhere.exe auf der Fehlerausgabe, obwohl es sonst durchlaeuft. Ohne
    # das Umleiten haelt PowerShell diese Zeile fuer einen Abbruch.
    $l2 = ('call "' + $vcvars + '" >nul 2>&1')
    $l3 = ('cl ' + $flags + ' ' + $incs + ' /Fo"' + $objdir + '\\" /Fd"' + $objdir + '\vc.pdb" /Fe"' + $exe + '" ' + $list)
    Set-Content -Path $bat -Value @($l1, $l2, $l3) -Encoding ASCII
    Write-Host "== $t bauen"
    & cmd /c $bat
    if ($LASTEXITCODE -ne 0) { Write-Host "BAU ROT: $t"; $fail = 1; continue }
    Write-Host "== $t"
    if ($t -like 'fuzz_*') { & $exe $FuzzSekunden } else { & $exe }
    if ($LASTEXITCODE -ne 0) { $fail = 1 }
}

if ($fail -eq 0) { Write-Host 'alle pruefungen: ok' } else { Write-Host 'PRUEFUNG ROT' }
exit $fail
