# Baut die Pruefsaetze des Tresors am PC und laesst sie laufen.
# Kein Geraet, keine Hardware - nur der Uebersetzer dieses Rechners.
#
# Aufruf:
#   powershell -ExecutionPolicy Bypass -File build_and_run.ps1
#
# Es wird genommen, was da ist: zuerst gcc oder clang, sonst der Uebersetzer
# der Visual Studio Build Tools.

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $here '..\..\src\classes\store'
$inc = Join-Path $here '..\..\include'
$out = Join-Path $here 'build'

New-Item -ItemType Directory -Force -Path $out | Out-Null

$sources = @(
    (Join-Path $src 'store_core.c'),
    (Join-Path $src 'store_space.c'),
    (Join-Path $src 'store_items.c'),
    (Join-Path $src 'store_log.c'),
    (Join-Path $src 'store_migrate.c'),
    (Join-Path $src 'store_chunk.c'),
    (Join-Path $src 'osk_backup.c'),
    (Join-Path $src 'osk_shamir.c'),
    (Join-Path $src 'osk_transfer.c'),
    (Join-Path $src 'osk_wordlist.c'),
    (Join-Path $here 'fake_flash.c'),
    (Join-Path $here 'fake_crypto.c'),
    (Join-Path $here 'fake_se050.c'),
    (Join-Path $here 'fake_ui.c'),
    (Join-Path $here 'test_util.c'),
    (Join-Path $here 'test_core.c'),
    (Join-Path $here 'test_tag.c'),
    (Join-Path $here 'test_paging.c'),
    (Join-Path $here 'test_power.c'),
    (Join-Path $here 'test_migrate.c'),
    (Join-Path $here 'test_backup.c'),
    (Join-Path $here 'test_shamir.c'),
    (Join-Path $here 'test_transfer.c'),
    (Join-Path $here 'test_wear.c'),
    (Join-Path $here 'test_wordlist.c'),
    (Join-Path $here 'test_main.c')
)

$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gcc) { $gcc = Get-Command clang -ErrorAction SilentlyContinue }

if ($gcc) {
    $exe = Join-Path $out 'store_tests.exe'
    $args = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-g', '-O1',
              "-I$src", "-I$inc", "-I$here", '-o', $exe) + $sources
    & $gcc.Source @args
    if ($LASTEXITCODE -ne 0) { throw 'Uebersetzen fehlgeschlagen' }
    & $exe
    exit $LASTEXITCODE
}

# Visual Studio Build Tools
$vs = Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat' -ErrorAction SilentlyContinue |
      Select-Object -First 1
if (-not $vs) { throw 'Kein Uebersetzer gefunden: gcc, clang oder Visual Studio Build Tools noetig' }

$list = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$cmd = "call `"$($vs.FullName)`" >nul && cl /nologo /std:c11 /W3 /WX /Od /Zi /D_CRT_SECURE_NO_WARNINGS " +
       "/I`"$src`" /I`"$inc`" /I`"$here`" /Fe:`"$out\store_tests.exe`" " +
       "/Fo:`"$out\\`" /Fd:`"$out\store_tests.pdb`" $list"

& cmd.exe /c $cmd
if ($LASTEXITCODE -ne 0) { throw 'Uebersetzen fehlgeschlagen' }
& "$out\store_tests.exe"
exit $LASTEXITCODE
