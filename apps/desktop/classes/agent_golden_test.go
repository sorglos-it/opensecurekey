// Pruefschluessel, erzeugt mit dem echten ssh-keygen (OpenSSH_10.5p1) am
// 24.10.2026. Sie sind der Sinn der Sache: wuerden die Pruefungen ihre
// Schluessel selbst zusammenbauen, pruefte unser Leser nur unseren eigenen
// Schreiber - und beide koennten denselben Irrtum teilen.
//
// Jeder Schluessel kommt mit der Zeile, die ssh-keygen selbst fuer seinen
// oeffentlichen Teil ausgibt. Genau diese Zeile muss unser Leser errechnen.
//
// Dass hier private Schluessel im Quelltext stehen, ist Absicht und harmlos:
// sie sind fuer diese Pruefungen erzeugt worden und stehen in einem
// oeffentlichen Repo - sie sichern nichts.
package classes

// goldenEd25519Private ist der private Teil.
const goldenEd25519Private = `-----BEGIN OPENSSH PRIVATE KEY-----
b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAMwAAAAtzc2gtZW
QyNTUxOQAAACCVcf5ZPDwWWFLR8OcWk3CD/BnkwDo53NCLq2bAF5nnigAAAJDLIUqZyyFK
mQAAAAtzc2gtZWQyNTUxOQAAACCVcf5ZPDwWWFLR8OcWk3CD/BnkwDo53NCLq2bAF5nnig
AAAEDB07e/V0BFFynNnMZCvRF3qM91XxIiJp1bqPs1iKtMjJVx/lk8PBZYUtHw5xaTcIP8
GeTAOjnc0IurZsAXmeeKAAAACWVkLWdvbGRlbgECAwQ=
-----END OPENSSH PRIVATE KEY-----
`

// goldenEd25519Public ist die Zeile, die ssh-keygen dazu ausgibt.
const goldenEd25519Public = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJVx/lk8PBZYUtHw5xaTcIP8GeTAOjnc0IurZsAXmeeK ed-golden"

// goldenEcdsaP256Private ist der private Teil.
const goldenEcdsaP256Private = `-----BEGIN OPENSSH PRIVATE KEY-----
b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAaAAAABNlY2RzYS
1zaGEyLW5pc3RwMjU2AAAACG5pc3RwMjU2AAAAQQSaE7Fsp+5nsa8UB2pBYRyjQYA1F5YX
k1UL80SfqaaOt7Bzbi6YwpXfbQhtRUO9nhdYCSsfqVezF/SuxASbbaSjAAAAqAg9+ukIPf
rpAAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBJoTsWyn7mexrxQH
akFhHKNBgDUXlheTVQvzRJ+ppo63sHNuLpjCld9tCG1FQ72eF1gJKx+pV7MX9K7EBJttpK
MAAAAgZSlHzvZ0//Z63NKuR5kRrQIKMZoubxslYzKQNWdvnDsAAAAJZWMtZ29sZGVuAQID
BAUGBw==
-----END OPENSSH PRIVATE KEY-----
`

// goldenEcdsaP256Public ist die Zeile, die ssh-keygen dazu ausgibt.
const goldenEcdsaP256Public = "ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBJoTsWyn7mexrxQHakFhHKNBgDUXlheTVQvzRJ+ppo63sHNuLpjCld9tCG1FQ72eF1gJKx+pV7MX9K7EBJttpKM= ec-golden"

// goldenRsa2048Private ist der private Teil.
const goldenRsa2048Private = `-----BEGIN OPENSSH PRIVATE KEY-----
b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAABFwAAAAdzc2gtcn
NhAAAAAwEAAQAAAQEAiqlutwGSGeGv7vR0zIIiiCz3LFK/whhQXvx23e4AEMunHbc+F9XG
fNTvMTe5/ltmHJNdxGlcUDcrSlVdp/AvMCLvUQtUctY9Cd4/MnsSrJv+Ge6lo6at+6s1hP
OfHeinLxGWl02emCYPxscLjS4PGSoxi06P6+Igcs5KcfhRFQjSwQ41AkyAoQnOBe3nUWQy
BJJid0qeoa4I61xpvH18d1z2v2y2U9XzLv0cOZO7TxILBJxYk/h4revsqZBmbx30xyMpQw
44EJ0y4OLquLY8v5dL0kDP+2F7X2Bd3vufrPhEo0iY8b6n6BQaw8I/bc+QliBCGfi2eogL
yy0t5S7rrQAAA8D7tO4B+7TuAQAAAAdzc2gtcnNhAAABAQCKqW63AZIZ4a/u9HTMgiKILP
csUr/CGFBe/Hbd7gAQy6cdtz4X1cZ81O8xN7n+W2Yck13EaVxQNytKVV2n8C8wIu9RC1Ry
1j0J3j8yexKsm/4Z7qWjpq37qzWE858d6KcvEZaXTZ6YJg/GxwuNLg8ZKjGLTo/r4iByzk
px+FEVCNLBDjUCTIChCc4F7edRZDIEkmJ3Sp6hrgjrXGm8fXx3XPa/bLZT1fMu/Rw5k7tP
EgsEnFiT+Hit6+ypkGZvHfTHIylDDjgQnTLg4uq4tjy/l0vSQM/7YXtfYF3e+5+s+ESjSJ
jxvqfoFBrDwj9tz5CWIEIZ+LZ6iAvLLS3lLuutAAAAAwEAAQAAAQAGVWXmuw4i24GvPwBE
xR7xrGgf4Km8omKtbAe77F6ZZcv82f1elleqJQnF7D81JJSd8odCbPOYAGUcUJjmDl0BbF
8VCxjw4+NG2510X1XDh56MLa/oMTlaOQBJUd1vPszAJCQGRjfbyEVJNZdL/BIZeqlnrQ96
3UnRJKYW3ZpKhRrxD/VZaK6Ezq34PzbTLZ996DHbXYbUFaOS9uKwY4kdu1Yoa7XDNz6zGC
Y5r72CmHRbnb0FWQBmpacz6NiwElLR0KHZck+O/kaFf6FhNhMwtomZcYyP2COnMoLw3z6W
0/r8i6tBt/P+RWg7PXC/61b5A+ATJ9mNMWJEmP8r+Vg7AAAAgE4BuSjanT768kn/M8funV
mIkFw/xJyHz8gEXJ7Xt2PAFBO0n1+XL2g+Hew0/3NtrurljkIxrc3GsYeMrptih8s5oQsH
ZiwO0AjGrya4ER0V8hFu51O+97G0j/9R4J8fbob5eAhlEbG6RQq2IU/QBI9LsaJFVGODZF
2dtyFX+Wp8AAAAgQC8+f7YFXPgWfsc/iLsyy1fYy1LtW6GPvNlT+2aDqwqLOkzLrqpaqsN
BnTFzfUWGcwiv2XskeHWk4X60hDVuZ1PAVESfohvh+C6Ra2FN/EHw/f1oxheMYAlzydior
oA6JAWohVxgZuKEjJQ5MLLYZsPZjFyQBShSzTuTpQcJNEsBwAAAIEAu9clZD+VM2nNwlqv
/ch+25CIIQOtIQN2zyuJAnQdu17PRz3svkhkVQEUIhjXMvghWiHimhhoJ2n3+weZK3ZCst
hh39ukRTG8NprwZLRBpR2uniZWPT8b6ip77j8a+BXQkjACCl8O6oFQBZff2uVUSLbv34qJ
PoeK61/XTcm6pasAAAAKcnNhLWdvbGRlbgE=
-----END OPENSSH PRIVATE KEY-----
`

// goldenRsa2048Public ist die Zeile, die ssh-keygen dazu ausgibt.
const goldenRsa2048Public = "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCKqW63AZIZ4a/u9HTMgiKILPcsUr/CGFBe/Hbd7gAQy6cdtz4X1cZ81O8xN7n+W2Yck13EaVxQNytKVV2n8C8wIu9RC1Ry1j0J3j8yexKsm/4Z7qWjpq37qzWE858d6KcvEZaXTZ6YJg/GxwuNLg8ZKjGLTo/r4iByzkpx+FEVCNLBDjUCTIChCc4F7edRZDIEkmJ3Sp6hrgjrXGm8fXx3XPa/bLZT1fMu/Rw5k7tPEgsEnFiT+Hit6+ypkGZvHfTHIylDDjgQnTLg4uq4tjy/l0vSQM/7YXtfYF3e+5+s+ESjSJjxvqfoFBrDwj9tz5CWIEIZ+LZ6iAvLLS3lLuut rsa-golden"

// goldenLockedPrivate ist der private Teil.
const goldenLockedPrivate = `-----BEGIN OPENSSH PRIVATE KEY-----
b3BlbnNzaC1rZXktdjEAAAAACmFlczI1Ni1jdHIAAAAGYmNyeXB0AAAAGAAAABDSTBAbAG
x0szfUn6C4nSElAAAAGAAAAAEAAAAzAAAAC3NzaC1lZDI1NTE5AAAAIP/PgJnrdaLUDLyS
S2FeQCD8plWSu8uKVUZhx64u0P7mAAAAkGiQSjOS3fzUyCRhHyFpqPYtHW9VupHshNU2BC
HatZXpedfYWtDlwgBdaAGjAKEWeVOgapWvYUhTWOZnjHEyel32DMsXXiDeWavnv4cTZVnQ
Sqem1quINx3xzzE385zoKjy5zIa8Jz6NCtx5sh11CPkjaLubko8iOE0ewWCzLO9jrewF+A
lVzM+PUuv/7WCuvw==
-----END OPENSSH PRIVATE KEY-----
`

// goldenLockedPublic ist die Zeile, die ssh-keygen dazu ausgibt.
const goldenLockedPublic = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIP/PgJnrdaLUDLySS2FeQCD8plWSu8uKVUZhx64u0P7m lock-golden"

// goldenPkcs8Ed25519Private ist der private Teil.
const goldenPkcs8Ed25519Private = `-----BEGIN PRIVATE KEY-----
MC4CAQAwBQYDK2VwBCIEIMHTt79XQEUXKc2cxkK9EXeoz3VfEiImnVuo+zWIq0yM
-----END PRIVATE KEY-----
`

// goldenPkcs8Ed25519Public ist die Zeile, die ssh-keygen dazu ausgibt.
const goldenPkcs8Ed25519Public = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJVx/lk8PBZYUtHw5xaTcIP8GeTAOjnc0IurZsAXmeeK ed-golden"
