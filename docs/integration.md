# How other programs reach the key

There is no standard that lets any program read a password out of a security key.
So the device offers every route that exists, and each program uses whichever it
already understands.

Status as of 24.10.2026: **five of these are built and each is proven by one
real operation** — against the device simulator, because no hardware exists yet.

| Route | Proven by |
| --- | --- |
| ssh-agent | `git push` through it, on Windows (named pipe) and Linux (socket) |
| Command line | `osk run` hands the secret only to the child process — measured |
| PKCS#11 | `openssl` reports "Verified OK"; a flipped byte is rejected. `ssh -I ./osk-pkcs11.so` signed a `git push` with no agent and no key file |
| Browser extension | a login field filled in a real Chromium 156; an entry for another address was refused |
| Local socket | Python fetched a secret after "yes" and got nothing after "no" — the refused request never reached the device |

Still to come, and all of them need admin rights to install, which is operating
system business rather than ours: Windows minidriver and CNG, disk unlock, PAM,
macOS CryptoTokenKit. macOS is cross-compiled but untested.

| Route | Reaches |
| --- | --- |
| Device types as a USB keyboard | anything, including login screens and BIOS |
| PKCS#11 library (dll / so / dylib) | Firefox, Thunderbird, OpenSSL, OpenSSH, Java, VPN clients |
| Smart card minidriver + CNG (Windows) | every Windows program through the certificate store |
| CryptoTokenKit (macOS, iOS) | Safari, Mail |
| ssh-agent offered by our program | ssh, git, scp, Ansible |
| FIDO-SSH (`sk-ecdsa`) | OpenSSH with no extra program at all |
| Browser extension | fills login forms, only on the stored address |
| Command line `osk get <name>` | scripts, CI, environment variables |
| Local socket, approved per access | your own programs |
| Fills the OS keychain | Windows Credential Manager, Gnome Keyring |

## Needs admin rights once

Smart card minidriver, disk unlock and PAM login are operating system
integrations and need admin rights to install. Command line, interface,
ssh-agent, PKCS#11 and the browser extension do not — that is what makes the
programs on the device's own drive useful on a strange computer.
