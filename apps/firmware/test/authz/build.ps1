# Prueflauf der Rechteschicht bauen und starten - am PC, ohne Hardware.
#
# Braucht nur die Visual Studio Build Tools; keine Fremdbibliothek. SHA-256
# und HMAC kommen aus src/classes/ctap/osk_crypto.c (eigener Code, kein
# Fremdcode, keine Systembibliothek) - darum braucht dieser Lauf weder
# bcrypt noch OpenSSL.
#
# Aufruf aus dem Repo-Stamm oder von hier - beides geht.
#
# WARUM ES ZWEI BAUBESCHREIBUNGEN GIBT (diese und das Makefile): Windows
# hat kein gcc, Linux kein cl.exe. Das ist die Stelle, an der dieses
# Projekt schon einmal gestolpert ist - das Makefile von test/se050/ fuehrte
# osk_mem.c, build.ps1 nicht, und der Windows-Weg band nie. Darum stehen
# die Quelldateien hier und dort in DERSELBEN Reihenfolge, damit ein
# Vergleich der beiden Listen mit dem Auge moeglich ist.

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$firmware = Resolve-Path (Join-Path $here '..\..')
$src = Join-Path $firmware 'src\classes\authz'
$ctap = Join-Path $firmware 'src\classes\ctap'
$inc = Join-Path $firmware 'include'
$se050test = Join-Path $firmware 'test\se050'
$out = Join-Path $here 'build'

# Die Build Tools bringen ihre Umgebung selbst mit; der Pfad kann je
# Rechner anders sein, darum wird gesucht statt geraten. Das Muster
# "Program Files*" fasst auch "Program Files (x86)" - dort liegen die
# Build Tools 2022 ohne Visual Studio.
$vcvars = Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat' -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $vcvars) {
    throw 'vcvars64.bat nicht gefunden - Visual Studio Build Tools fehlen.'
}

New-Item -ItemType Directory -Force -Path $out | Out-Null

$sources = @(
    (Join-Path $src 'authz_der.c'),
    (Join-Path $src 'authz_core.c'),
    (Join-Path $src 'authz_keys.c'),
    (Join-Path $src 'authz_cred.c'),
    (Join-Path $src 'authz_tsa.c'),
    # SHA-256 und HMAC. NICHT zum zweiten Mal geschrieben: osk_wipe() lag
    # in diesem Projekt schon einmal doppelt vor und brach das Binden des
    # Abbilds (siehe include/osk_mem.h).
    (Join-Path $ctap 'osk_crypto.c'),
    (Join-Path $firmware 'src\osk_mem.c'),
    (Join-Path $here 'fake_se050.c'),
    (Join-Path $here 'test_der.c'),
    (Join-Path $here 'test_realm.c'),
    (Join-Path $here 'test_keys.c'),
    (Join-Path $here 'test_cred.c'),
    (Join-Path $here 'test_tsa.c'),
    (Join-Path $here 'test_main.c')
)

foreach ($s in $sources) {
    if (-not (Test-Path $s)) { throw "Quelldatei fehlt: $s" }
}

# /W4 /WX: jede Warnung ist ein Fehler. Diese Schicht entscheidet ueber
# jede Freigabe und liest Puffer vom Draht - eine uebergangene Warnung ist
# hier keine Kleinigkeit.
# tiny_test.h kommt aus test\se050\ und wird NICHT kopiert: zwei
# Pruefrahmen wuerden still auseinanderlaufen.
$quoted = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$exe = Join-Path $out 'authz_tests.exe'
$cmd = "`"$vcvars`" >nul 2>&1 && cl /nologo /W4 /WX /std:c17 /Od /Zi " +
       "/I `"$inc`" /I `"$src`" /I `"$ctap`" /I `"$se050test`" /I `"$here`" $quoted " +
       "/Fo:`"$out\\`" /Fd:`"$out\\vc.pdb`" /Fe:`"$exe`""

Write-Host 'Uebersetze...'
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "Uebersetzen fehlgeschlagen (Code $LASTEXITCODE)." }

Write-Host ''
& $exe
exit $LASTEXITCODE
