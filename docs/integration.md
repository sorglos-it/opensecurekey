# How other programs reach the key

There is no standard that lets any program read a password out of a security key.
So the device offers every route that exists, and each program uses whichever it
already understands.

Status: design. None of these has been built yet.

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
