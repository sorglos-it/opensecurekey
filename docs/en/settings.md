# Settings

**Status:** The settings of the program are checked against the test device. The settings in the key itself do not exist yet on a real key.

There are few settings. This is on purpose.

## In the program `osk`

Open the **Settings** page in the interface. Settings are saved in the file `config_osk.json` **next to the program**, not in the system.

| Setting | Meaning | Default | Allowed values |
| --- | --- | --- | --- |
| Language | Language of the interface | `de` | `de`, `en`, `fr`, `it`, `es` |
| Name of this program | The name the program gives to the key. It ends up in the log. | `osk` | Text up to 64 characters |
| Port | Port the interface listens on | `8101` | Number from 1024 to 65000 |
| Test device address | For testing only: address of the test key | empty | for example `127.0.0.1:8201`, and only on this computer |

Good to know:

- **The name is a claim.** The key writes it into the log, but it proves nothing. Only the key itself can refuse something.
- If the port is taken, the program looks for a free one starting from the set value. If it finds none, it says so.
- **Leave the test device address empty** as long as you use a real key. The test device is a testing tool. Never put real secrets there. Its PIN is `123456`, and it encrypts nothing.
- If the settings file is broken or too large, the program still starts. It uses the defaults and says why.

Example (`config_osk.json`):

```json
{
  "lang": "de",
  "port": 8101,
  "appName": "osk",
  "simAddress": ""
}
```

## Per entry in the key

Each entry has three switches. See [usage.md](usage.md), section "Entry types".

- may be mirrored
- paired computers only
- write-protected

## In the key itself

You set these things **on the key**, not in the program. **Status:** not tried anywhere yet.

| Setting | Default | What it changes |
| --- | --- | --- |
| Profile and PIN | up to 4 profiles, each with its own PIN | Who sees what |
| NFC tap function (open an address or type a one-time code) | can be switched off | A phone opens an address when it taps the key |
| Sign blind | off | The key also signs things that it cannot show in readable form. The question on the screen then looks clearly different. |
| Server grant | off | A server may sign without a press, for named keys, for 24 hours at most |
| Bluetooth | only for the management program | Not for signing in in the browser. Pairing only with number comparison and a press on the key. |

Good to know:

- **Everything risky is off** until you switch it on on purpose.
- **NFC always needs the button.** Even when the NFC contact is short. This protects against an attack from a pocket, where someone relays the radio link.
- The Bluetooth radio address changes, so that nobody can track you.
- NFC and Bluetooth work only while the key is plugged into a USB port. It has no battery.

## Language on the key

The screen on the key knows the same five languages. If a text is missing, it uses English, never an empty space.

## Changing the language

Change the language in the program under **Settings.** Language files are in the folder `lang`, one per language.
