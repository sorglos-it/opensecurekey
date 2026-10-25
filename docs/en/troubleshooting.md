# When something does not work

**Status:** None of this was experienced on a real key. The messages come from the program. General tips are marked as such.

## The key is gone

This is the section that matters. Read it **now,** not only once it has happened.

### First: What can the person who finds the key do?

- **Nothing, without the PIN.**
- The security chip counts the wrong tries. Erasing does not reset the counter.
- Even with the PIN, every approval needs the button on the key.
- There is **no remote lock.** The technical documentation says nothing about one.
- General advice, not from the key: If you are unsure, change the passwords of your most important accounts.

### Then: What do you have? Find your case.

#### Case 1: You have a second key (way "A spare is possible")

1. Take the second key.
2. It can do the same as the lost one, **for everything that was marked "may be mirrored".**
3. Your wallet seed was not among them. It never goes to a second key.
4. As soon as possible, set up a **new** second key. Otherwise you have only one again.
5. The public part of SSH, certificate and PGP keys is **not** on the second key so far. Save it again, otherwise a program cannot list the key.

#### Case 2: You have a backup and the main passphrase

1. Get a new key.
2. Set it up. See [installation.md](installation.md).
3. Load the backup. For this you need the file **and** the main passphrase.
4. If you no longer have the main passphrase but you have envelopes: **Three out of five** (or the number you chose) open the file without the main passphrase.
5. Note: The program warns that loading replaces the content of the device.

**Open:** Whether a new key from the backup can also use your **passkeys** again is not settled in the technical documentation. For passkeys, rely on the second key.

#### Case 3: You have none of this, and you chose "this key only"

- For the logins created with it there is **no spare.**
- At every service you have to start over, using the way the service offers for lost logins.
- The key itself cannot help you with that.

#### Case 4: Everything gone: both keys and the main passphrase

- **Whoever loses the main passphrase and all keys loses everything.**
- Nobody can bring the content back. Not even the project. There is no back door that could be stolen.

### How to prepare

These four things, best right at setup:

1. **Pair a second key** (only with way A).
2. **Save a backup,** store it apart from the main passphrase.
3. **Emergency sheet:** main passphrase on paper, in a safe place. Also note where the backup is.
4. **Envelopes** for people you trust: one part per envelope, three out of five open it.

**Never** keep everything in the same place. The file and the main passphrase together open everything.

The order of your safety:

- Second key + backup + sheet: a mishap is no drama.
- Second key only: A fire or theft of the bag takes both away.
- None of these: A lost key is a total loss.

## The key is not recognised

In the program under **Devices** it says "No device found."

General tips (not tried on the key):

1. Try another USB port.
2. Another cable, or plug straight into the computer, without a hub.
3. Unplug the key, plug it in, restart the program.
4. The key has no battery and takes all its power from the USB port. If the screen stays dark, it gets no power: try another port (point 1).

What else matters:

- **macOS:** Without the special way of building it (cgo), the program starts, but **does not find a real key** and says so.
- If you want to use the test key, enter its address in the settings. See [settings.md](settings.md).
- The ID of the key (vendor and product number) is **provisional** and not yet agreed with the firmware.

Messages about this:

| Message | Meaning | What to do |
| --- | --- | --- |
| No device found. | No key is answering. | See above. |
| The devices of this computer cannot be listed | The system refuses the list. | Restart the program. If that does not help, report it with the text in brackets. |
| The device cannot be opened | Possible cause: Another program is using the key, or the system does not allow it. | Close other programs that use the key. |
| Nothing can be read from the device / Nothing can be written to the device | The connection broke off. | Plug the key in again. |
| The device did not answer in time. / The device took too long. | The key may be waiting for your press. | Look at the screen on the key. |
| The connection to the device is disturbed. | As above. | Plug the key in again. |
| This operating system is not supported yet. | There is no way yet for this system. | Use Windows or Linux. |
| The device speaks a different protocol version. | Program and firmware do not fit together. | Update the program or the firmware. |

## The PIN is locked

- **Three wrong PINs in a row** lock the key until you **unplug it and plug it in again.** After that you may try again.
- The counter is in the security chip. Writing again or erasing the memory does **not** reset it.
- **How many wrong tries there are in total** before the PIN is locked for ever, and what happens then, is **not** in the technical documentation. So do not guess. Look for the PIN first.
- For the smart card side (PIV, OpenPGP) there is also a PUK and an admin PIN. They are in the vault, each with its own counter.

| Message | Meaning | What to do |
| --- | --- | --- |
| The device is locked. | You have not unlocked yet. | **Unlock.** The key asks for the approval on its own screen. |
| The approval on the device is missing. | You did not confirm (or confirmed with "No"), or it took more than 30 seconds. | Try again. Press long, then press long. |

### Resetting

In an emergency the sign-in part (FIDO) can be reset.

- **This is final.** The main secret becomes new, **every old login is worthless.** You have to sign in again at every service.
- Resetting does **not** work over NFC.
- Deleting on the key asks twice.
- Whether this also solves a locked PIN is not described in the technical documentation.

## The browser does not accept the key

**Status:** Never tried in a browser. This list only knows the **known limits.**

- **Old sites that only know U2F** may not accept the key. It does not send an attestation certificate.
- **Company sites that require a certificate** may reject it. It attests itself.
- The key calls itself **FIDO 2.0 and 2.1.** Commands to manage passkeys (list, delete) and to enrol fingerprints in the browser **do not exist.**
- The key reports **no built-in user verification** (`uv`) as long as no sensor is inside. That is correct. The PIN is reported separately.
- Over **Bluetooth** there is **no** sign-in in the browser. Browsers do not support FIDO over Bluetooth reliably. Use USB or NFC.
- NFC and Bluetooth work only while the key is plugged into a USB port. Held loose against a phone, nothing works.
- The **signature counter** starts at zero at every power-on. A site that insists on growing counters might take the key for a copy.
- The **manufacturer ID** (AAGUID) is provisional.

Messages of the browser extension:

| Message | Meaning | What to do |
| --- | --- | --- |
| This entry belongs to a different address. | The entry is saved for another site. | **This is the phishing protection.** Check the address. If it is right, change the address on the entry. |
| This page is not encrypted. No secret goes here. | The page does not use https. | Do not fill in. |
| The channel to the browser broke off. | The extension lost the connection. | Restart the browser. |
| The registration file could not be written. | The extension could not register itself. | Check the rights in the folder. |
| That is not an extension id. / I do not know this browser. | Wrong input when registering. | Check the ID and the browser name. |

## Messages about entries and backup

| Message | Meaning | What to do |
| --- | --- | --- |
| There is already an entry with this name. | Duplicate name. | Choose another name. |
| There is no room left on the device. | The memory of the profile is full (768 entries). | Delete old entries. |
| The device does not know this entry. | Entry deleted or named wrongly. | Reload the list. |
| The name matches several items | Not unique. | Name it more exactly. |
| This device cannot do that yet. | The function is missing in this firmware. | Update the firmware, if there is a newer one. |
| The backup file is empty or too large. | Wrong file. | Choose another file. |
| The data on the device does not have the expected form. | The data is damaged, or it comes from a **newer** firmware. A newer version is never read wrongly, it is not opened at all. | Update the firmware. If that does not help, load the backup. |
| The firmware image is empty. | No file chosen. | Choose a file. |
| The settings file cannot be read. / The settings file is too large. / The settings file has a mistake in it. | The file `config_osk.json` is damaged. | Delete it. The program then uses the defaults. |

## Messages about the ssh agent and the interfaces

| Message | Meaning | What to do |
| --- | --- | --- |
| This place is already taken | Another agent or service is already running. | Stop the other one. |
| This key is encrypted with a password. | The SSH key in the vault is still protected. | Put it into the vault decrypted. |
| This key is too short and no longer safe. | Weak key. | Create a new key. |
| I cannot handle this kind of key | Not supported. | Choose another kind. |
| This key was asked for, but it is not in the vault. | The program asks for a key that you do not have. | Put the key into the vault. |
| The access was not approved. / No answer came to the question about approval. | You refused or pressed nothing. | Try again. |

The exit numbers for scripts are shown by:

```powershell
osk codes
```

## Messages about dongle protection

| Message | Meaning | What to do |
| --- | --- | --- |
| No security key found. Please plug it in. | The key is not plugged in. | Plug it in. |
| A different security key is plugged in than the configured one. | The program belongs to another key. | Plug in the right key. |
| The file cannot be opened: wrong key or it was changed. | The key does not match the locked file, or the file was changed. | Use the key that locked it. |
| The licence has expired. | The licence was genuine and is over. | Get a new licence. |
| There is no licence for … on this security key. | No licence for the product. | Put the licence on the key. |
| This chip cannot do key agreement. | The chip was set this way at the factory. Server operation with the key in the chip is dropped. Signing continues to work. | This cannot be changed. |

## This does not work yet

So that you do not search for long:

- **KeePassXC and VeraCrypt** with the key
- **Signing in to the computer,** unlocking the hard disk, the key as a keyboard
- a ready-printed emergency page from the program
- **macOS**

## If you cannot find it

Report it in the project on GitHub (sorglos-it/opensecurekey) and include: operating system, version of `osk`, the exact message, and what you did before. Passwords and PINs never belong in a report.
