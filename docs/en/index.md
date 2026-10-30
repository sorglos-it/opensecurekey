# OpenSecureKey – Manual

**You cannot buy this device yet, and nobody has used it so far.**

Everything in this manual describes what is being built. None of it has ever run on a real key. No key has been built yet. Where something was only checked on the computer, it says so.

## What is this?

A security key on a key ring. It keeps your logins safe, and nothing comes out unless you press the button on the key yourself.

In plain terms:

- It signs you in to websites without you typing a password (passkeys).
- It stores passwords, codes, keys and certificates.
- It can act like a smart card.
- It can sign payments for Bitcoin and Ethereum.
- It can let a program run only while it is plugged in (dongle).

The key has its own small screen. The screen shows **what** you are approving. The computer cannot fool you there.

## How this manual is built

| File | What it is for |
| --- | --- |
| [installation.md](installation.md) | First setup, step by step. It contains the question that can never be changed later. |
| [usage.md](usage.md) | Everyday use: sign in, get a password, get a code, sign. |
| [settings.md](settings.md) | What you can set. |
| [how-it-works.md](how-it-works.md) | How the key works inside. And what it can **not** do. |
| [troubleshooting.md](troubleshooting.md) | When something does not work. And: **when the key is gone.** |
| [development.md](development.md) | For everyone who builds the project themselves. |

## The most important things first

- **One question during setup can never be changed later:** Should a spare key be possible later? Read it calmly. See [installation.md](installation.md).
- **With no spare key and no backup, a lost key is a total loss.**
- **Whoever loses the main passphrase and all keys loses everything.** There is no back door. See [troubleshooting.md](troubleshooting.md).
- **The emergency sheet during setup cannot be skipped.** For exactly that reason. See [installation.md](installation.md).
- There is **no outside review** and **no test on real hardware.** Do not rely yet on anything you cannot replace.

## What the key cannot do

Explained in detail in [how-it-works.md](how-it-works.md). In short:

- In the first build stage it has **no fingerprint sensor.** And for none of the sensors that were checked is it shown that it detects a fake fingerprint.
- It does **not** protect you if someone forces you to press the button.
- It does **not** protect against changed firmware.
- It does **not** save you if the main passphrase and all keys are gone.
- **It hands out no private key as a file. Never.** Software that insists on a key file (`.pem`, `.pfx`) fundamentally cannot operate it. The certificate may leave, the key may not. See [usage.md](usage.md).
- It knows **no profiles for passkeys.** When you switch the profile you sign in at the services again.
- **"Plug in and start" means waiting more than a minute.** The key hands out its data slowly.
- On the planned memory card, **one of three operating modes does not encrypt.** And a signature proves that the content is unchanged, not its authorship. See [how-it-works.md](how-it-works.md).

## Status of the parts

| Part | Status |
| --- | --- |
| Device (circuit board, case) | not built yet |
| Firmware | written, tested on the computer with test devices, never on the key |
| Program `osk` on the computer | checked against a test device, never against a real key |
| Windows and Linux | compiled and tested with the test device |
| macOS | never tried |
| Browser and websites | never tried |

More technical detail (in English, for developers): [`docs/`](../) in the project folder.
