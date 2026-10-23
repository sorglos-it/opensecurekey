# The vault

The vault is the part of the key that remembers things: passwords, one-time
code seeds, SSH keys, certificates, API keys, recovery codes, wallet seed
phrases, licences, cards, identities, notes and small files.

Status: built and tested on a PC. Nothing has run on hardware — the parts have
not been ordered yet.

## What it promises

**Nothing comes out without you.** Every secret needs a press on the device, and
the display names what is being released. A program that asks is logged by the
name it claims, and that name is shown as a claim, not as proof.

Asking with too small a buffer does not reveal anything either: the device asks
you first and only then complains about the buffer. Otherwise a program could
learn how long a password is by trying, without you ever agreeing.

**A power cut cannot break it.** Every change is written in a fixed order: the
new secret first, then the affected pieces of the index, then the one record
that makes all of it count, and only then is the old place freed. Whenever the
power goes, the device comes back with either the old state or the new one, and
everything in it reads. This is tested by cutting the power at every single
write position of every kind of change — including the one case that writes two
pieces of the index at once, which is deleting an entry near the front of a long
list.

**A password is never written twice in the same place.** The device spreads its
writes over the whole chip, so the flash does not wear out at one spot. Measured
over 400 changes to the same entry: 400 different places, none used twice.

**Several people, several PINs.** A device can hold up to four profiles, each
with its own PIN. The PIN of one profile cannot open another, and a locked
profile shows nothing at all — not even how its entries are called.

**Room for 768 entries per profile, 3072 in a device.** A browser full of logins
holds 100 to 300 of them, so there is room for a household. What limits it now
is the flash chip, not the memory of the key: the list of entries is read in
pieces of 64 instead of all at once, so a device with 3072 entries needs no more
memory than one with 64 did. The arithmetic behind the number is in
`hardware.md`.

**A firmware update does not lose data.** The vault says which version it is,
and an older one is read as it is and rewritten as you go. Nothing has to be
exported and imported. A vault from a *newer* firmware is not opened at all
rather than read wrongly.

## Finding without opening

Some entries carry a short mark that says which service they belong to. The
device can look through those marks without decrypting anything and without
asking you — that is what makes a passkey login quick even with hundreds of
entries.

The mark is stored encrypted like everything else, so somebody who reads the
chip cannot see which services you have an account with. Only the device itself,
after you have unlocked it, can look through them.

Because the list is read in pieces, a search has to go through every piece — 12
of them in a full profile, 240 KiB of decryption, and 48 pieces or 962 KiB if
all four profiles are open at once. On a PC with stand-in crypto that takes
about 1.2 to 1.45 milliseconds for one profile and 4.8 to 5.9 for all four;
**what it takes on the real chip is not measured, because there is no chip
yet.** If it turns out too slow for a
passkey login, there is a known way to make one piece enough instead of twelve.
It is written down in `../shared/vault-format.md` and deliberately not built, so
that nobody pays for it before a measurement says it is needed.

## Entry types

Login, one-time code (TOTP and HOTP), SSH key, certificate, PGP key, API key,
recovery codes, wallet seed, licence, card, identity, Wi-Fi, note, file.

Each entry carries three marks you set yourself:

| Mark | What it does |
| --- | --- |
| may be mirrored | goes to your second device, and only then |
| paired computers only | handed out only to a computer you have paired |
| read-only | cannot be changed or deleted, not even by you — until you lift it |

## Saving to a file

The device can write its whole content into one encrypted file. Put it on a
disk or on your NAS; it is useless to anyone without the master passphrase.
From that file a brand new device can be set up — it is not tied to the device
that wrote it.

The master passphrase must be at least twelve **characters** long. The device
offers you a phrase of eight words, and taking it is the better choice: eight
words out of a list of 256 are harder to guess than anything most people invent,
and easier to write on a sheet of paper.

Keep the file and the passphrase apart. Both together open everything.

## Envelopes for your family

If nobody can get at the passphrase, the vault is lost with you. So the device
can split the key of the saved file into several parts, for example five, of
which three are enough. Each part is one line of text to print and put into a
sealed envelope, one per person.

- One envelope on its own reveals nothing. Not a letter of a password.
- Three of five together open the file, without the passphrase.
- A part that was mistyped is noticed, instead of quietly producing nonsense.

## A second device

Two devices can be paired, and then the second one is a real spare.

Both show a six-digit number. If the numbers match, press on both; if they do
not, somebody is sitting between them and nothing is transferred. The computer
or the radio link in between only ever sees encrypted data — it can record
everything and understand nothing.

What goes over: the entries you marked as "may be mirrored". What never goes
over: everything else, and anything a fingerprint sensor would produce.
Fingerprint data is never in the vault, so it cannot be transferred either -
and stage 1 of the device has no sensor at all (see
`threat-model.md`), so the PIN is the way in.

## The device log

The device keeps its own log of what was released when and to which program. It
is in the device, not on the computer, and it is encrypted — the list of
services you use is worth something by itself. It holds the last few hundred
events; older ones drop out.

Secrets are never in the log. That is tested.

## What it does not promise

- **Losing everything at once.** If the device, the second device, the saved
  file and the envelopes are all gone, the content is gone. Nobody can help,
  and no backdoor exists to be stolen.
- **A forced press.** If somebody stands next to you and makes you press, the
  device cannot tell. No device can.
- **A changed firmware.** The vault protects against a lost or stolen device,
  not against a firmware that lies to you. That is what the signed bootloader is
  for.
- **Someone who can rewrite the flash directly.** They cannot read anything,
  but they can put an older state back. Closing that needs a counter in the
  security chip and is still open — see `../shared/vault-format.md`.

## For developers

- Interfaces: `apps/firmware/include/osk_store.h`, `osk_log.h`, and
  `osk_store_profiles.h`, `osk_backup.h`, `osk_shamir.h`, `osk_transfer.h` in
  `apps/firmware/src/classes/store/`
- Format and reasoning: `../shared/vault-format.md`
- Tests on the PC, no hardware needed:

```powershell
powershell -ExecutionPolicy Bypass -File apps\firmware\test\store\build_and_run.ps1
```

The crypto comes from the nRF Connect SDK through PSA (`osk_crypto_psa.c`) and
is never written by hand. The PC tests use doubles for flash, crypto, the SE050
and the display, so they prove the structure of the vault — not that AES-GCM is
correct.
