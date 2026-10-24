# The management program `osk`

`osk` is the counterpart on the computer: it finds the device, opens the vault,
lists and changes entries, pairs devices, writes firmware and saves backups.
It is one file per operating system, needs no installation and no admin
rights, and stores its settings next to itself — so it can later live on the
read-only partition of the key itself.

**Status: nothing has been tested against real hardware. The hardware has not
been ordered. Everything below was measured against the test device in
`apps/desktop/tests/simulator`.** macOS has not been compiled or run at all.

## What runs, and against what

| Part | State |
| --- | --- |
| Interface (7 pages), command line, protocol (all 14 commands) | runs against the test device |
| CBOR, frames, entry checks, languages | own tests, green on Windows and Linux |
| HID on Windows (`hid.dll` + SetupAPI) | compiles and finds the computer's HID devices; never spoken to our device |
| HID on Linux (`/dev/hidraw`) | compiles; never spoken to our device |
| HID on macOS (IOKit) | written, never compiled — no Mac was available |
| Secrets in memory, clipboard wipe, origin check | implemented, covered by tests where a test can see it |

## Build

Go 1.24 or newer, no other dependency, no cgo, no third-party package.

```powershell
go build -C apps\desktop -trimpath -ldflags "-s -w" -o ..\..\dist\osk.exe .
```

Linux (on the Linux machine, or from Windows):

```powershell
$env:GOOS = "linux"; $env:GOARCH = "amd64"; $env:CGO_ENABLED = "0"; go build -C apps\desktop -trimpath -ldflags "-s -w" -o ..\..\dist\osk .
```

macOS is the one exception: the file `classes/hid_darwin.go` needs cgo,
because macOS offers no way to reach a HID device without IOKit. Built with
`CGO_ENABLED=0` on a Mac, the program still starts and still talks to the test
device — it just finds no real device, and says so in plain words.

```bash
# on a Mac
CGO_ENABLED=1 go build -trimpath -ldflags "-s -w" -o dist/osk .
```

The program is about 9 MB. Most of that is the Go runtime plus the HTML
template engine; it has no shared libraries to carry along.

## Run

```powershell
dist\osk.exe
```

It prints the address, for example `http://127.0.0.1:8101/`. Open that in a
browser. For scripts:

```powershell
dist\osk.exe list
```

```powershell
dist\osk.exe get "My account"
```

```powershell
dist\osk.exe unlock
```

`unlock` reads the PIN from standard input — never from a command line
argument, because those are visible in the process list.

## Test device (no hardware needed)

```powershell
go run -C apps\desktop .\tests\simulator -port 8201
```

Then put `127.0.0.1:8201` into **Settings → Test device address**, or into
`apps/desktop/config/config_osk.json`. The PIN of the test device is `123456`.
It keeps its vault in memory, encrypts nothing and only claims that the user
pressed the button — it is a test tool, never a place for real secrets. The
interface marks it as a test device.

## Tests

```powershell
go test -C apps\desktop ./...
```

- `classes/cbor_test.go` — CBOR round trip, and that bad input is refused
  (trailing bytes, non-shortest length, indefinite length, duplicate keys,
  floats, too deep)
- `classes/frame_test.go` — splitting and joining 64 byte reports
- `classes/items_test.go` — entry checks and wiping
- `classes/texts_test.go` — the five languages, same keys everywhere, same
  placeholders, English fallback, and that every key used in pages, scripts or
  code really exists
- `tests/e2e_test.go` — the whole protocol, the command line and the interface
  against the test device, including the origin check

The end-to-end test starts the test device itself with `go run`, so the first
run takes a while.

## Layout

```
apps/desktop/
├── main.go          entry point: interface or command line
├── VERSION
├── classes/         logic, one job per file
│   ├── cbor.go      own CBOR, strict, with limits
│   ├── frame.go     64 byte reports
│   ├── hid.go       shared part of the HID layer
│   ├── hid_windows.go · hid_linux.go · hid_darwin.go · hid_other.go
│   ├── connect.go   find devices, test device over loopback
│   ├── client.go    the protocol from shared/protocol.md
│   ├── items.go     entry types, mirrors osk_item_type_t
│   ├── app.go       shared core of interface and command line
│   ├── server.go    local web server, origin check
│   ├── api.go       the interface's own endpoints
│   ├── cli.go       command line
│   ├── config.go    settings next to the program
│   └── texts.go     languages
├── views/           one file per page
├── templates/       the frame around the pages
├── assets/          app.css, app.js
├── config/          config_osk.json.example
├── lang/            de, en, fr, it, es
└── tests/           end-to-end test and the test device
```

## The frame on the wire

`shared/protocol.md` names the 64 byte report and the CBOR map, but not the
report header. These three bytes are **our proposal** and must be agreed with
the firmware:

| Byte | Meaning |
| --- | --- |
| 0 | flags: `0x01` first report, `0x02` last report |
| 1–2 | number of payload bytes in this report (big endian, 1…61) |
| 3–63 | payload |

A whole message is at most 16 kB. The device identifies itself by USB vendor
`0x1209`, product `0x05E1` and usage page `0xF1D1` — also provisional, also to
be agreed with the firmware.

## Security

- **No token, and still not open to other pages.** The server binds only
  `127.0.0.1`. Every request must carry a `Host` header that is one of our own
  addresses, every changing request must carry a matching `Origin`, and
  `Sec-Fetch-Site` must be `same-origin` or `none`. That blocks both a foreign
  page in the same browser and a DNS name pointed at `127.0.0.1`.
- **Secrets stay short-lived.** A revealed secret travels in one answer and
  the buffer is overwritten right after. The PIN buffer is wiped as soon as it
  has been sent. Nothing secret ever goes into an address, a log line or a
  file. Go can still move a buffer while the garbage collector works — that is
  a known limit, written down here rather than hidden.
- **The clipboard is emptied** about 20 seconds after a copy.
- **Every request names the asking program.** The device writes that name into
  its log. The interface says on every page that the name is a claim by the
  program, not proof — only the device itself can refuse.
- **The answers of the device are treated as hostile input**: strict CBOR, hard
  limits on size, depth and number of entries, the sequence number has to
  match, and the protocol version has to match.
- The settings file is read with a size limit; a broken file never stops the
  program, it falls back to the defaults and says why.

## Setup: the one question that cannot be taken back

A device in factory state is led through four steps, in this order and with no
way around them: **mode, PIN, backup, emergency sheet**. While the device
reports itself as fresh, every other page answers `303` to `/setup` — a
bookmark on the overview does not help, and that is the point. The question
has to be answered before a secret exists.

The first step is the one that matters, so the interface is built not to
soften it:

- **No default.** Neither way is preselected, and `Next` stays disabled until
  one is chosen **and** the "I understand this cannot be changed" box is
  ticked. The server asks for that confirmation a second time (`confirm` in
  `/api/setup`), so a hand-written request cannot slip past it either.
- **Both ways name their price** next to their promise — the mirrored one that
  the root secret is created outside the secure chip once, the other one that
  there is no spare, ever.
- **The recommendation stands beside them with its reason**, not in place of
  the choice.
- **The device decides.** A device that is already set up answers
  `OSK_ERR_DENIED`; the program refuses beforehand as well, but the device has
  the last word. Nothing in the program can change the mode afterwards —
  `Client.Setup` is the only place that sets it and it is only reachable here.

After the PIN is set, the program opens the vault with that same PIN right
away. The next step is the backup, and the backup needs an open vault; asking
the user for the PIN they chose two seconds ago is exactly the kind of
question people learn to click away. If that opening fails, the answer says so
(`unlocked: false` plus the reason) — the setup itself has happened and is
final, and that is not something to swallow quietly.

The **emergency sheet** (`/sheet`, printable, reachable afterwards as well)
holds what is needed when both devices are gone: the serial number, the chosen
mode, the date, where the backup file is, the passphrase, and who holds which
Shamir envelope and how many are needed (`shared/vault-format.md`). The last
three are **lines to fill in by hand** — the program prints no secret, and the
PIN is deliberately not on the sheet at all. In "this device only" the sheet
says plainly that the backup does not bring back what is in the secure chip.

**Not built on the device side.** `shared/protocol.md` has no command for
this and no field in `hello` for the mode; the table ends at 18. The program
speaks command 19 `setup` (`mode`, `pin`) and reads `fresh` and `mode` from
`hello`. A device that says nothing about either counts as set up — better no
wizard than the irreversible question at a device that already holds accounts.
Both additions have to go into `shared/protocol.md`, which is not this part's
file.

## Measured: what listing costs over PKCS#11

The public part of a key is not a secret, and the device sends it with `list`
(field `pub`). `Pkcs11Token.loadObjects` used to fetch every key with `reveal`
to compute modulus, curve and point — one button press per key, protecting
nothing. Reading the public blob instead needs a parser for the SSH public-key
format: `classes/agent_pubkey.go`, measured against the keys that the real
`ssh-keygen` produced (`classes/agent_golden_test.go`).

Measured, not assumed (`tests/freigaben_pkcs11_test.go`, three keys and a
certificate):

| Operation | Approvals |
| --- | --- |
| Listing objects (`C_FindObjects`) | **0** |
| Signing once | **1** |
| Signing a second time | **1** |

The third row matters: a token that kept the key would get the second
signature for free.

The counter-check: with the pub path switched off in `loadObjects`, the same
test reports "listing cost 4 approvals, expected 0" and 4 reveal fallbacks. So
the test measures something. `RevealFallbacks()` counts the old path at
runtime too — it must never fire silently.

That both paths produce the *same* attributes is a separate test
(`classes/pkcs11_pubweg_test.go`), because counting approvals alone would also
be green if the objects were wrong. The reveal path now writes the public part
out of the private key and reads it back through the same parser: one
calculation, two ways in.

The parser is strict, because these bytes come from outside: the message has
to be used up completely, the curve point has to lie on the curve
(`crypto/ecdh` checks that), RSA below 2048 bits is refused, and re-encoding
has to yield the identical bytes — which rejects any non-shortest encoding
before it can become a key that a second program reads differently.

## Open points

- `tools/build.py desktop` (the project's own build script) does not know this
  program yet — the commands above do the job in the meantime.
- Vendor and product id, the usage page and the three frame header bytes need
  to be confirmed with the firmware.
- `reveal` currently returns the secret as bytes or text. Once the firmware
  decides, the alternative can go.
- Entry types are kept in step with `osk_item_type_t` by hand. If that header
  changes, `classes/items.go` has to change with it.
- No certificate is created yet: the page lists the certificates on the device
  and runs the `attest` check. Creating one needs the firmware side first.
- `shared/protocol.md` needs command 19 `setup` and the `fresh` / `mode`
  fields in `hello`; the simulator in `tests/simulator` needs a factory state
  to answer them. Neither file belongs to this part. Until then the wizard is
  played through against the small factory-state device in
  `tests/werkszustand_test.go`, which answers only what the wizard needs.
- `docs/anbindungen.md` still says PKCS#11 reveals every key under "Measured:
  how many approvals an operation costs". That is no longer true; the
  paragraph belongs to another part and was left untouched.
