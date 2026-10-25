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
- A battery would only have bought NFC and Bluetooth without a computer. Against that stood 4 to 5 mm more length and a part that wears out in a few years, in a key that should last decades.

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
- **Mac:** never tried.
- **Memory space in the firmware** is tight. Whether all functions fit is open.

## Program and key talk to each other

- The program `osk` talks to the key over its own USB channel.
- Every answer from the key is treated as hostile: strict formats, hard limits for size and depth, and the sequence number must match.
- The local interface listens only on your computer. Every request must come from its own address. A foreign page in the same browser cannot get at it.
- Every request names the program. The key writes it into the log. It stays a name, not proof.
