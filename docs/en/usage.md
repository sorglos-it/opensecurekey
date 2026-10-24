# Using the key

**Status:** None of this has run on a real key. For each function it says how far it has been checked.

## The rule for everything

- **Nothing comes out unless you press the button on the key.**
- The screen on the key says **what** you are approving: the service, the recipient, the amount.
- What the key cannot show in readable form, it does not approve.
- Malware on the computer can ask. It cannot press the button, and it cannot change what the screen says.

## The button

The key has **one** button and three ways to press it.

| Press | Length | Meaning |
| --- | --- | --- |
| short | under 0.8 seconds | next page. On the question page: **No.** |
| long | 0.8 seconds or more | go to the question page. There: **Yes.** |
| double | two short presses, quickly one after the other | read everything again from page 1 |

This is how an approval works:

1. **Read.** Each page shows the exact wording. You turn pages with a short press.
2. **Decide.** A long press shows the question page. There: long = Yes, short = No.
3. **Deleting asks twice.** Two long presses on two question pages.

A long press on a reading page approves **nothing.** It only brings the question page. Nobody approves something they have not seen.

After 30 seconds without a press it counts as "No".

Long names and addresses are **never cut off.** They run over several pages. A cut-off recipient address would be worse than no screen at all.

**Status:** Nothing has ever been shown on a real screen. Whether 0.8 seconds feels right in the hand, nobody knows.

## Signing in to a website (passkey)

**Status:** Never tried in a browser.

1. On the website choose "Sign in with security key" or "Passkey".
2. If the computer asks for the PIN: enter the PIN.
3. The screen on the key names the service.
4. Read. **Press long** to get to the question page, there **press long** for Yes.

Good to know:

- A passkey works **only for the real address** of the site. On a fake site it does not work.
- The key understands passkeys, the older U2F sign-in, and the extension that lets programs such as Bitwarden, BitLocker and LUKS derive a key (`hmac-secret`).
- **Old sites that only know U2F** may reject the key. It does not send an attestation.
- Some company sites demand an attestation certificate. The key does not have one.
- Over NFC, every approval also needs the button.

## Getting a password

**Status:** checked against the test device, never against a real key.

### In the interface

1. Start `osk` and open the address in the browser.
2. **Unlock** the key (PIN).
3. Search for the entry and open it.
4. Choose **Show**.
5. Confirm on the key.
6. With **Copy** you put it on the clipboard. It is emptied again after about **20 seconds**.

### By command

The PIN is never passed as a parameter. It comes in through standard input, because parameters can be seen by other programs.

```powershell
osk unlock
```

```powershell
osk list
```

```powershell
osk get "Mein Konto"
```

If a name matches several entries, the program does not guess. It says that the name is not unique.

### Letting the browser fill it in

There is a browser extension. It fills a login field **only on the saved address.** For any other address it refuses. This is the protection against fake sites, not a convenience bug. On an unencrypted page (without https) it gives out nothing.

**Status:** A login field was filled in a real Chromium, with the test device. Never with a key.

### Public parts without a press

The **public** part of an SSH key, certificate or PGP key comes **without** a press on the key. This is on purpose: If you pressed for every name, you would soon press without looking. Then the one press that counts would be worth nothing: the one before signing.

## A code for two-factor sign-in (2FA)

**Status: open.**

- The key can store the starting value for one-time codes, as an entry "One-time code (time)" or "One-time code (counter)".
- The battery keeps the clock running. Time-based codes need the time of day.
- **How the finished six-digit code is shown in the interface or on the key is not described in the technical documentation.** I write nothing about it here.
- The command `osk codes` does **not** show these codes, but the list of exit numbers for scripts.
- KeePassXC and VeraCrypt work with a Yubico method. That is still missing: **Neither program works with the current state.**

## Signing a file or message

**Status:** The calculation core is written and checked on the computer. Never tried with a real smart card program (GnuPG, OpenSC).

The key behaves like a **smart card.** For this, Windows, Linux and macOS need no driver.

- It can report itself as a **PIV card** and as an **OpenPGP card.**
- Keys are kept in the security chip. They never leave it.
- **Every signature needs the button.** The screen names the service.
- With PIV, the PIN must be checked again at slot "9C" **immediately before every** signature.
- Elliptic curves are supported (P-256 and P-384). **RSA is not supported so far** (the calculation core rejects it).

How it works:

1. Ask the program (for example GnuPG) to sign with the key on the card.
2. Enter the PIN when asked.
3. On the key, read the name, **press long**, **press long.**

### With SSH and Git

**Status:** Checked with the test device. `git push` through the agent worked on Windows and Linux.

1. Start the agent:

```powershell
osk agent
```

2. Show the keys in the vault:

```powershell
osk keys
```

3. You confirm every signature on the key.

For programs that speak PKCS#11 there is a library (`osk-pkcs11`, as .dll and .so). With it, `openssl` checked a signature as "Verified OK", with the test device.

### Giving a secret to a program

```powershell
osk run NAME=eintrag -- programm
```

The secret goes only to the program that was started. The shell next to it does not see it.

## Signing in to the computer

**This does not work yet.**

- Signing in to Windows, Linux or the Mac with the key: not built.
- Unlocking the hard disk and the key as a keyboard (the key types your password): not built.
- These parts need administrator rights once for setup. That is a matter for the operating system.

Only these are built and checked with the test device: ssh agent, command line, PKCS#11, browser extension and a local interface for your own programs.

## Crypto wallet (Bitcoin and Ethereum)

**Status:** Checked on the computer. **A real payment has never been sent.**

- The key signs a payment only if **recipient, amount and fee** were readable on the screen.
- If one of them is missing, the key says **No**, without asking the chip.
- Among the things that are **not readable** are contracts, token amounts and typed data. The key signs such things only if you have **switched on** "sign blind" on the key.
- A half payment is never signed. If one part fails, everything fails.
- Not supported: Taproot inputs.
- Your wallet seed is created in a way that it is not copied to a second key and goes only to paired computers.

## Program only with the key (dongle) and server

**Status:** Only with the test device. The key side for this does not exist yet.

- **Level 1** (the key says "I am here") stops someone who only **copies** a program. Whoever can take a program apart removes the check.
- **Level 2** (data is locked with a key from the chip) is stronger in principle: Without the key, the data cannot be read.
- A server can sign with the key instead of with a key file. **Without a grant**, a server stands still, because nobody presses at 3 a.m. The grant is **off**, names keys, expires after **24 hours at most** and is logged. **With** a grant, malware on that server can sign.

More about this: [how-it-works.md](how-it-works.md).

## The key's log

The key keeps its own encrypted log. It shows **which program** fetched **which secret** and **when**. It is stored in the key, not on the computer. It covers the last few hundred events.

```powershell
osk log
```

The program name in the log is a **claim** by the program, not proof.

Secrets are never in the log.

## Several people on one key

One key has up to **four profiles**, each with its own PIN. The PIN of one does not open any other. A locked profile shows nothing, not even the names of the entries. Space: **768 entries per profile**, **3072 in the key.**

## Entry types

Login, one-time code (time), one-time code (counter), SSH key, certificate, PGP key, API key, recovery codes, wallet seed, licence key, bank card, identity, Wi-Fi, note, file.

Each entry carries three marks that you set yourself:

| Mark | Effect |
| --- | --- |
| may be mirrored | goes to your second key, and only there |
| paired computers only | is given out only to a paired computer |
| write-protected | cannot be changed or deleted, not even by you, until you lift it |
