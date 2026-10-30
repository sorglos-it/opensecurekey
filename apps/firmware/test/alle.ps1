# Laesst JEDEN C-Pruefsatz der Firmware laufen, in beiden Richtungen der
# RSA-Option. Das ist der eine Befehl, mit dem man feststellen kann, ob die
# Firmware gruen ist.
#
# Aufruf:
#   powershell -ExecutionPolicy Bypass -File alle.ps1
#   powershell -ExecutionPolicy Bypass -File alle.ps1 RSA=1
#   powershell -ExecutionPolicy Bypass -File alle.ps1 ctap ui
#
# Diese Datei ist ABSICHTLICH nur ein Mantel: sie ruft alle.sh in der WSL auf
# und gibt deren Ausstiegscode weiter. make, gcc und cmake gibt es unter
# Windows nicht, und zwei Baubeschreibungen, die sich widersprechen, haben
# den Prueflauf des SE050-Treibers am 28.10.2026 schon einmal still
# gebrochen. Die Logik steht genau einmal, naemlich in alle.sh.
#
# Die WSL erreicht KEINE Netzlaufwerke. Liegt der Arbeitsbaum auf Z:, bricht
# dieser Aufruf ab - dann wird ein Arbeitsbaum unter D:\claude\working
# gebraucht.

$here = Split-Path -Parent $MyInvocation.MyCommand.Path

if ($here -notmatch '^[A-Za-z]:') {
    Write-Error "alle.ps1: unerwarteter Pfad: $here"
    exit 2
}
$laufwerk = $here.Substring(0, 1).ToLower()
$rest = $here.Substring(2).Replace('\', '/')
$wslpfad = "/mnt/$laufwerk$rest"

$netz = (Get-PSDrive -PSProvider FileSystem |
    Where-Object { $_.Name -eq $here.Substring(0, 1) -and $_.DisplayRoot })
if ($netz) {
    Write-Error ("alle.ps1: $here liegt auf einem Netzlaufwerk. " +
        "Die WSL kann dort nicht uebersetzen - bitte einen Arbeitsbaum " +
        "unter D:\claude\working benutzen.")
    exit 2
}

$argumente = $args -join ' '
wsl -e bash -lc "cd '$wslpfad' && ./alle.sh $argumente"
exit $LASTEXITCODE
