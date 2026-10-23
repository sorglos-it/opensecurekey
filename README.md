# opensecurekey

[![Release](https://img.shields.io/github/v/release/sorglos-it/opensecurekey)](https://github.com/sorglos-it/opensecurekey/releases)
[![License](https://img.shields.io/github/license/sorglos-it/opensecurekey)](LICENSE)
[![Donate](https://img.shields.io/badge/Donate-PayPal-blue)](https://www.paypal.com/donate/?hosted_button_id=6CDEVZGJWTNQQ)

An open security key that also stores your passwords. FIDO2 and passkeys, smart card
(PIV, OpenPGP), 2FA codes, SSH keys, certificates, a crypto wallet and a software
dongle — all in one device, with every secret kept inside a separate security chip.

**Status: early development. Nothing has run on hardware yet.**

| Folder | Purpose | Language | Start | Build |
| --- | --- | --- | --- | --- |
| `apps/firmware` | Device firmware (nRF52840 + SE050) | C / Zephyr | – | `python tools/build.py firmware` |
| `apps/desktop` | Management program `osk` with local web interface | Go | `osk` | `python tools/build.py desktop` |
| `apps/web` | Browser extension | JS | – | `python tools/build.py web` |

Hardware design lives in `hardware/`, the case in `cad/`, full documentation in
[`docs/`](docs/).

## Start in 3 steps

Not available yet — no release has been built. Until then:

1. Read [`docs/architecture.md`](docs/architecture.md) for how the device is built.
2. Read [`docs/threat-model.md`](docs/threat-model.md) for what it protects against.
3. Read [`docs/hardware.md`](docs/hardware.md) for the parts needed to build one.

## Known limits

- NFC and Bluetooth need the device to have power — the nRF52840 cannot draw energy
  from an NFC field, so the device carries a small battery.
- There is no standard that lets any program read passwords from a key. The device
  offers every available route instead (keyboard, PKCS#11, ssh-agent, browser
  extension, command line); see [`docs/integration.md`](docs/integration.md).

## License

MIT – Copyright (c) 2026 Sorglos Thomas Weirich

## Donate via PayPal

If this project helps you: [Donate via PayPal](https://www.paypal.com/donate/?hosted_button_id=6CDEVZGJWTNQQ)
