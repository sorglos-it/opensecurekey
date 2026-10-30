# Setting up

**Status:** The key does not exist yet. These steps describe the planned process. The program `osk` has so far only run against a test device. A setup assistant that guides you through steps 3 to 5 does not exist yet: The texts are written, the program for them is not.

## What you need

- the key
- a computer with Windows or Linux (macOS: never tried)
- a browser
- time to read the question in step 3 calmly

You need **no installation** and **no administrator rights** for the program `osk`.

## Step 1: Get the program

- `osk` is **one file** per operating system.
- It keeps its settings **next to itself**, not in the system.
- A finished program to download does not exist yet. No release has been built. If you want to build it yourself, read [development.md](development.md).

**Planned** is that the key brings the programs along itself: when plugged in it shows a small, read-only drive.

**Then this holds: "plug in and start" means waiting more than a minute.** The key hands out its data slowly. That is normal and not a fault. How long exactly and why is in [how-it-works.md](how-it-works.md), section "The program drive needs its time".

## Step 2: Plug in the key, start the program

1. Plug the key into a USB port.
2. Start `osk`.
3. The program prints an address, for example `http://127.0.0.1:8101/`.
4. Open this address in the browser.
5. Under **Devices** your key must be listed. If it says "No device found.", [troubleshooting.md](troubleshooting.md) will help.

The address leads only to your own computer. Nobody from outside can reach the page.

## Step 3: The one question

The interface asks you first:

> **Should a spare key be possible later?**

**This answer can never be changed later.** This is not a cautious warning, it is how the technology works: A secret that was created inside the security chip never comes out again. So it cannot be copied to a second key later.

There are two ways. Both have a price.

### Way A: "Yes, a spare is possible"

- **What you get:** You can set up a second key later that does the same. If you lose one, you take the other and get back into everything.
- **What you pay:** Your main secret is created **once outside the security chip** during setup. In that one moment, malware on your computer could read it along. Never again afterwards.

### Way B: "No, this stick only"

- **What you get:** Everything stays in the security chip for good. No secret leaves it, at any point.
- **What you pay:** There is **no spare.** If the key is lost or breaks, you cannot get back into the logins created with it. You would have to start over at every service.

### What the program recommends

- **For everyday use:** Allow a spare. If you lose your only key, you otherwise lose all logins.
- **For a crypto wallet:** You can add a **second profile without a spare** later.

Neither way is safer in general. It depends on what you fear more: the one moment on the computer, or losing the only key.

### Changing your mind

- This is only possible this way: **create a new profile** and **sign in again at every service.**
- One key holds up to **four profiles.** Each has its own PIN.
- It **has** to be this way: signing in with passkeys knows no profiles. Even on a plain switch from one profile to another you sign in at the services again. See [usage.md](usage.md), section "Several people on one key".

Before you can continue, you must tick: "I understand that this cannot be changed later"

## Step 4: PIN

- Nothing works without a PIN. Whoever finds the key gets nothing without the PIN.
- The PIN has at least **4 characters.** Umlauts each count as one character.
- How many wrong tries there are in total before the PIN is locked for good is **not yet decided.** Three wrong tries in a row first only lock it until you unplug the key. More about this in [troubleshooting.md](troubleshooting.md).
- The **security chip** counts the wrong tries. Nobody can reset the counter by erasing the memory.
- **Open:** Where exactly you set the PIN for the first time is not yet described.

## Step 5: Secure things before you use the key

This is the most important step. Do it **right away**, not "later".

1. **Set up a second key** (only with way A). See below.
2. **Save a backup.** See below.
3. **Write an emergency sheet.** See below.

**Why this cannot wait for "later":** whoever forgets their main passphrase and loses both keys **has lost everything.** Not "almost everything" and not "recoverable with some effort". There is no back door, no fallback procedure and nobody who can help. That is why the emergency sheet in the setup assistant **cannot be skipped:** no "skip", no "remind me later". Either a second key is paired, or the sheet is written.

**Status:** this obligation is planned and **not built.** Until the assistant exists, nobody forces you. Do both anyway, and do it now.

### Pair the second key

1. Plug in both keys.
2. In the interface choose **Pair a spare device**.
3. **Both keys show the same six-digit number.** Compare them.
4. If the numbers are **the same:** press the button on both keys.
5. If they are **different:** do not press. Then someone is sitting in between. Nothing is transferred in that case.

What is transferred:

- Only entries with the mark **"may be mirrored".**

What is **not** transferred:

- everything else
- a crypto wallet seed: It is created in a way that it is not copied to a second key

Pairing runs over USB at the computer only. Both keys are plugged in at the same time. There is no radio transfer from key to key, because the key has no battery.

The computer with both keys plugged in only sees encrypted data.

**Known gap:** The public part of SSH keys, certificates and PGP keys does **not** go to the second key so far, and it is not in the backup. The key works there, but a program can only list it once the public part has been saved again.

### Save a backup

1. In the interface open **Backup**, then **Save a backup.**
2. You choose a **main passphrase.** It has at least **twelve characters.** Not digits, but characters.
3. Take the key's suggestion: **eight words.** That is harder to guess than almost anything people think up, and easier to write on paper.
4. The file is encrypted. The computer cannot read it.
5. Put the file on a disk or a NAS.

**Important:**

- Keep the **file and the main passphrase apart.** Both together open everything.
- With the file and the main passphrase you can set up a **completely new key.** The file does not depend on the key that wrote it.
- When you load, the program warns you: "Loading replaces the content of the device." So do not load by accident.

### Emergency sheet

An emergency sheet is a sheet of paper that you keep **in a safe place**. It contains your **main passphrase.**

If you want others to be able to get in during an emergency, you can use **envelopes:**

- The key splits the key of the backup into several parts, for example five.
- Each part is **one line of text.** Print them out, one sealed envelope per person.
- **Three out of five** together open the backup, without the main passphrase.
- **A single envelope reveals nothing.** Not even one letter.
- A wrongly typed part is noticed. No wrong result comes out.

You decide the number of parts and how many are needed.

**Open:** A ready-printed "emergency page" produced by the program is not described as a feature. Write down your main passphrase and the place where the backup is stored yourself.

## Update

### Firmware (the software in the key)

1. In the interface open **Firmware**, then **Write the firmware.**
2. **Do not unplug the key while it is writing.**
3. The key accepts **only signed images.**

Your vault is kept. An older version of the vault is read and rewritten when you use it. A vault from a **newer** firmware is not opened at all, instead of being read wrongly.

**Status:** Writing to a key has never been tried. The tool for it (`tools/flash.py`) is untested.

### The program `osk`

There is no update function yet. You replace the file with a newer one.

## Removing

- **Program:** delete the file `osk`. The settings are next to it, not in the system.
- **Empty the key:** See [troubleshooting.md](troubleshooting.md), section "Resetting". **This is final.**
