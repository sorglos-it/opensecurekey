# How the key works inside

**Status:** Everything here is a draft and checked on the computer. Nothing ran on a real key.

## Two chips, two jobs

- **The radio chip (nRF52840)** talks to the computer and the phone.
- **The security chip (SE051)** holds the secrets. It does not give out a key.

Why two? The protection of the radio chip has been beaten in the past with voltage spikes. So a broken radio chip alone does not give anything away.

In addition there are:

- **Memory (64 MB):** the vault, encrypted, and the log.
- **Screen:** shows what you are approving.
- **One button:** this is your approval.
- **No battery:** The key runs from USB only. The computer sets the clock for one-time codes when you plug the key in.

## Ways to the computer

- USB as a sign-in device (FIDO2)
- USB as a smart card reader (PIV, OpenPGP)
- USB as a keyboard (**planned, not built yet**)
- USB channel for the management program
- USB drive, read-only, with the programs on it (**planned**)
- NFC (only while USB powers the key)
- Bluetooth, only for the management program (only while USB powers the key)

## Why no battery?

The key has no battery, on purpose. It runs only while it is plugged into a USB port.

- The clock for time-based one-time codes needs no continuous power. The computer sets it when you plug the key in, and the code is made only at the moment you use it. **Draft, not built yet.**
- For the timestamp on a signature, the key asks a timestamp server. It does not use its own clock.
- A battery would only have bought NFC and Bluetooth without a computer. Against that stood more length (how much is in `hardware/layout-notes.md`) and a part that wears out in a few years, in a key that should last decades.

**What you give up for this:**

- The radio chip cannot take power from the NFC field. So NFC and Bluetooth exist only while the key is plugged into a computer.
- Holding the key loose against a phone no longer works.
- Bluetooth to your own app works only while the key is plugged into a computer. With the key on a key ring it no longer works.
- A second key is paired over USB at the computer only. Both keys are plugged into the same computer for that.

This was decided on purpose. The technology behind it is in `docs/architecture.md`.

## The two ways of setup

This is the technology behind the one question from [installation.md](installation.md).

### "This key only"

- For each login, **its own key pair** is created in the security chip. The private part exists **nowhere else.**
- There is nothing to steal and nothing to unpack.
- The price: Each login takes up space in the chip. The number of services is **finite.** How many there are depends on the chip version supplied (46 or 104 kB) and is **not measured.** The program is meant to warn in good time.
- There is no second device for these logins.

### "A spare is possible" (mirrored)

- A main secret is created **outside both chips** during pairing and written into both.
- The price: For a short time it is in the memory of the radio chip.
- After writing, the firmware can **only use it,** not read it, not overwrite it, not take it out. This is not discipline, the chip sets it that way.

### Why can it not be changed?

A secret that was created in the chip never leaves it, not even encrypted. So it can never be mirrored afterwards. A mirrored main secret has to be created in the right place **before** it exists.

Neither way is safer in general. A lost single key destroys more than a stolen one usually does.

## The PIN

- The key never gets the PIN in plain text, only a check value made from it.
- The **security chip** checks it and counts the wrong tries. Erasing the memory does not reset the counter.
- Three wrong PINs in a row lock it until you unplug the key.
- PIV and OpenPGP use the **same** PIN. The management PINs (PUK, admin PIN) are in the vault, each with its own counter.
- Every try with a management PIN asks on the screen. That is correct.

## The vault

- Each secret is encrypted on its own (AES-256-GCM).
- **Power failure:** Each change is written in a fixed order. After any failure, the old **or** the new state is there, and everything is readable. This was tested at every single write point.
- The key spreads writing over the whole memory. This way it does not wear out in one place.
- **Searching without opening:** Some entries carry a short tag. It says which service the entry belongs to. This way the key finds a passkey quickly. The tag is itself encrypted.
- The search has to go through all parts of a list. **How long that takes on the real chip is not measured.**
- A firmware update loses no data.

## The backup

- An encrypted file from which a new key can be set up.
- Its key comes from the **main passphrase,** not from the chip. Otherwise the backup would depend on the key that was lost.
- On the key, the calculation values are kept small. The strength must therefore come from the main passphrase. That is why eight words.
- The envelopes split the key of the file (Shamir).

## A second key

- Both keys create a short-lived key and exchange only the public part.
- From this the six-digit number is made. Someone sitting in between creates **two different** numbers. This was tested.
- Without a confirmed number, not one byte leaves the key.

## What protects the key from whom

| Who | What they get |
| --- | --- |
| finds the key | nothing without the PIN |
| opens the key and examines the circuit board | The line between the chips is encrypted **once the factory keys are replaced** (see below). The memory is encrypted. |
| has malware on the computer | Every approval needs the button, and the screen names the service. The program cannot press. |
| runs a fake website | Passkeys are tied to the real address. The extension fills in only on the saved address. |
| relays NFC between phone and key | still needs the press on the key |
| copies a server | The server's key stays in the chip. **Only if the supplied chip can do this.** |

## What the key cannot do

Here it is said without sugar-coating.

### It does not protect against

- **Someone who forces you to press the button.** No device can detect that.
- **Changed firmware.** The security chip protects against attacks on the chip, not against firmware that lies to you. Only the signed start helps against that. It has to be there **before** read protection is switched on.
- **Someone reading your PIN along.** You type it on the computer, so malware can read it. It cannot use it, because every approval needs the button.
- **The loss of everything.** See below.

### Fingerprint

- The first build stage has **no fingerprint sensor.** You approve with PIN and button.
- Space for a sensor is planned on the circuit board.
- With FIDO2, a fingerprint **replaces** the PIN. It is not extra protection.
- **Liveness detection is not shown for any sensor that was checked.** Whoever fears a moulded finger gains nothing from any of these sensors.
- Fingerprint data would never be in the vault and could never go to a second key.
- No browser and no website can enrol a finger. That would happen on the key itself.

### Loss

**Whoever loses the main passphrase and all keys loses everything.**

If you lose the key, the second key, the backup file and the envelopes all at once, the content is gone. Nobody can help. There is no back door that could be stolen.

### Other things worth knowing

- **No outside test.** Nobody outside the project has looked at the encryption.
- **No FIDO conformance test** has been run.
- **The chip comes with publicly known factory keys.** At the first start-up they **must** be replaced. Until then the line between the chips is not protected. This is described and planned, but **never carried out on a chip.**
- **Whoever can rewrite the memory directly** can read nothing. But they can play back an older state. A counter in the chip against this is still missing.
- The sign-in **signature counter** starts at zero at every power-on. Some sites might take that for a copied key.
- The key sends a **self-attestation,** not a certificate from the manufacturer. That is good for privacy, but some company sites demand more.
- Whether the supplied chip allows **key agreement** (ECDH) depends on its factory setting. If it does not allow it, **server operation with the key in the chip is dropped.** Signing continues to work. The program then says so honestly, instead of looking like a PIN error.
- **There is no private key as a file.** Programs that insist on a key file cannot work with the key. See [usage.md](usage.md), section "Software that insists on a key file".
- **Mac:** never tried.
- **Memory space in the firmware** is tight. Whether all functions fit is open.

## The program drive needs its time

**Status:** calculated, not measured on the device. The drive is planned and not built.

When plugged in, the key shows a small, read-only drive holding the management programs. Whoever opens it waits.

**The three numbers for this live here. Every other place in the manual points here.**

| What | Number |
| --- | ---: |
| What the radio chip gets through over USB | about **400 kB per second** |
| How big the programs are | **29.10 MB** (measured on 23.10.2026 on the finished image) |
| So how long the reading takes | about **73 seconds** |

"Plug in and start" really means: plug in, **wait more than a minute,** start. Whoever does not know this thinks the key is broken and pulls it out.

Why so slow? The radio chip sits on a USB port of the old 1.1 standard. On paper that gives 1.5 MB per second, but the key has to fetch every piece from its memory first and answer. What is left are the 400 kB. The 400 kB are **measured** on a development board; the other two numbers are calculated from them.

The programs grow. As the file grows, the waiting time grows in the same proportion.

## The encrypted data carrier

**Status: paper.** There is no chip, no board and not one line of program for it. This section is here so that nobody later believes something false — not because it exists.

Planned is a memory card on the key, 8 to 16 GB, for your own files.

**The card always needs an openSecureKey.** In an ordinary card reader there is nothing to be done with it. It carries a format of its own, not a Windows format. It is **not** a data carrier you hand to somebody who has no key.

### Three operating modes — and one of them is not secret

This is the place where it is easiest to harm yourself. So here are the three side by side:

| Mode | Name | Who can read | Content secret? |
| --- | --- | --- | --- |
| 1 | **only this key** | only the key that wrote it | **yes** |
| 2 | **any of our keys, with the card word** | any openSecureKey that has the card word | **yes** |
| 3 | **any of our keys, sealed only** | **any** openSecureKey, with no passphrase | **no** |

**Mode 3 encrypts nothing.** It only seals: you can tell whether somebody changed the content. Anybody holding any openSecureKey can read it.

**Whoever believes they encrypted and only sealed gives their secrets away.** That is the worst mistake this device can make, and it does not come from an attack but from a mix-up.

That is why this is planned:

- The display names the operating mode in plain words **before** anything is written, and your press confirms exactly that text. For mode 3 it says "**OPEN — sealed only**".
- Mode 3 looks clearly different on the display, not the same line with a different word in it.
- As long as the card is plugged in, the operating mode stays on the display.
- **Changing the mode means writing the card again.** There is no "encrypt it afterwards".

### Authenticity is not authorship

The signature on the card proves: **the content is unchanged** since it was signed.

It does **not** prove who it comes from.

That a card comes from a particular person follows from the signature **only** if you know that person's public key **beforehand** — from a meeting, a business card, some other channel. Otherwise you know only this: some openSecureKey signed here.

| Claim | covered by the signature? |
| --- | --- |
| "This content is unchanged." | **yes** |
| "An openSecureKey signed it." | yes |
| "*This* device signed it." | yes |
| "This device belongs to a particular person." | **no** |
| "The content comes from that person." | **no** |

Planned therefore: when plugged in, the display names the sender key as a short string of characters and adds "**unknown sender**" or the name you once gave that sender.

### Checking happens while reading, not when plugging in

Checking a 16 GB card **completely** means reading all of it. That takes **about an hour and a half** (calculated with the speed of the card itself, not that of the USB port from the section above — those are two different paths). Nobody waits for that when plugging in, and so the key does not do it.

Instead it checks **every block at the moment it reads that block.**

The good part: no file you open reaches you unchecked.

The bad part: **a changed block in a file nobody opens is not noticed.**

That is why the key never says "card checked". That would be a lie. It says "this block checked", and that is the truth.

## Program and key talk to each other

- The program `osk` talks to the key over its own USB channel.
- Every answer from the key is treated as hostile: strict formats, hard limits for size and depth, and the sequence number must match.
- The local interface listens only on your computer. Every request must come from its own address. A foreign page in the same browser cannot get at it.
- Every request names the program. The key writes it into the log. It stays a name, not proof.
