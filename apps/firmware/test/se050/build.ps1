# Prueflauf des SE050-Treibers bauen und starten - am PC, ohne Hardware.
#
# Braucht nur die Visual Studio Build Tools; keine Fremdbibliothek. AES und
# AES-CMAC kommen aus Windows selbst (bcrypt.dll), auf dem Geraet statt
# dessen aus PSA Crypto im nRF Connect SDK.
#
# Aufruf aus dem Repo-Stamm oder von hier - beides geht.

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$firmware = Resolve-Path (Join-Path $here '..\..')
$src = Join-Path $firmware 'src\classes\se050'
$inc = Join-Path $firmware 'include'
$out = Join-Path $here 'build'

# Die Build Tools bringen ihre Umgebung selbst mit; der Pfad kann je
# Rechner anders sein, darum wird gesucht statt geraten.
$vcvars = Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat' -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $vcvars) {
    throw 'vcvars64.bat nicht gefunden - Visual Studio Build Tools fehlen.'
}

New-Item -ItemType Directory -Force -Path $out | Out-Null

$sources = @(
    (Join-Path $src 'se050_crc.c'),
    (Join-Path $src 'se050_tlv.c'),
    (Join-Path $src 'se050_t1.c'),
    (Join-Path $src 'se050_scp03.c'),
    (Join-Path $src 'se050_apdu.c'),
    (Join-Path $src 'se050.c'),
    (Join-Path $src 'se050_rsa.c'),
    (Join-Path $src 'se050_counter.c'),
    (Join-Path $src 'se050_crypto_common.c'),
    (Join-Path $src 'se050_crypto_cng.c'),
    # osk_mem.c gehoert dazu: osk_wipe() liegt dort, und der Treiber ruft es
    # an sechs Stellen. Die Datei fehlte hier, waehrend das Makefile sie
    # fuehrt - zwei Baubeschreibungen derselben Pruefung, die sich
    # widersprachen. Der Windows-Weg brach deshalb erst beim Binden ab,
    # nicht beim Uebersetzen, und das sieht aus wie ein Fehler des
    # Aufrufers statt wie eine fehlende Quelle.
    (Join-Path $firmware 'src\osk_mem.c'),
    (Join-Path $here 'fake_bus.c'),
    (Join-Path $here 'se050_sim.c'),
    (Join-Path $here 'test_crc.c'),
    (Join-Path $here 'test_tlv.c'),
    (Join-Path $here 'test_t1.c'),
    (Join-Path $here 'test_scp03.c'),
    (Join-Path $here 'test_errors.c'),
    (Join-Path $here 'test_contract.c'),
    (Join-Path $here 'test_rsa.c'),
    (Join-Path $here 'test_counter.c'),
    (Join-Path $here 'test_main.c')
)

# /W4 /WX: jede Warnung ist ein Fehler. Bei einem Treiber, der Puffer vom
# Draht entgegennimmt, ist eine uebergangene Warnung keine Kleinigkeit.
# se050_crypto_psa.c ist absichtlich NICHT dabei: das braucht das nRF
# Connect SDK, das es am PC nicht gibt. Diese eine Datei ist deshalb
# geschrieben, aber nirgends uebersetzt - siehe docs/se050.md.
# Die Platinenhaken aus se050_platform.h erfuellt test_contract.c.
$quoted = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$exe = Join-Path $out 'se050_tests.exe'
$cmd = "`"$vcvars`" >nul 2>&1 && cl /nologo /W4 /WX /std:c17 /Od /Zi " +
       "/I `"$inc`" /I `"$src`" /I `"$here`" $quoted " +
       "/Fo:`"$out\\`" /Fd:`"$out\\vc.pdb`" /Fe:`"$exe`" /link bcrypt.lib"

Write-Host 'Uebersetze...'
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "Uebersetzen fehlgeschlagen (Code $LASTEXITCODE)." }

Write-Host ''
& $exe
exit $LASTEXITCODE
