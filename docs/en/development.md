# Building it yourself and developing

This chapter is for everyone who builds the project themselves. The detailed technology is in English in the folder [`docs/`](../) (for example `architecture.md`, `threat-model.md`, `vault.md`, `build.md`).

**Status:** There is no key yet. Building and testing is done on the computer, with test devices for chip, memory and screen.

## Folders

| Folder | Content | Language |
| --- | --- | --- |
| `apps/firmware` | Software in the key (nRF52840 + SE051) | C / Zephyr |
| `apps/desktop` | Management program `osk` with local interface | Go |
| `apps/web` | Browser extension | JavaScript |
| `shared/` | Protocol, vault format, library `osk-guard` | |
| `hardware/` | Circuit diagram, pin assignment, layout notes | |
| `cad/` | Case | |
| `tools/` | Build and test tools | Python |

## What you need

| Part | Tool | Without this tool |
| --- | --- | --- |
| all | Python 3 | nothing works |
| `apps/desktop` | Go 1.24 or newer | is skipped |
| `apps/web` | Node.js 20 or newer (only for checking) | Building works, checking is skipped |
| `apps/firmware` | nRF Connect SDK (Zephyr, `west`) | is skipped |

"Skipped" always means: **not built, not checked.**

## Check everything

```powershell
python tools/check.py
```

- It checks what is possible without hardware.
- At the end it says how many steps ran, what was skipped and how many tests were run.
- The exit code is 0 only if nothing failed **and** at least one test ran.

## Building

```powershell
python tools/build.py desktop
```

```powershell
python tools/build.py web
```

```powershell
python tools/build.py firmware
```

The results are in `dist/`.

The program `osk` can also be built directly. It needs no third-party package and no cgo (except for macOS).

```powershell
go build -C apps\desktop -trimpath -ldflags "-s -w" -o ..\..\dist\osk.exe .
```

## Testing without a key

The test key (the test device) runs only in memory. It encrypts nothing and only pretends that you pressed the button. **Never put real secrets there.**

```powershell
go run -C apps\desktop .\tests\simulator -port 8201
```

Then you enter `127.0.0.1:8201` under **Settings → Test device address.** The PIN of the test device is `123456`.

The tests of the program:

```powershell
go test -C apps\desktop ./...
```

## Four levels of "checked"

Every statement about "checked" means one of four levels:

1. written
2. compiled
3. in the image
4. ran on the key

This project is at **level 2, plus runs on the computer with test devices.** Levels 3 and 4 are missing everywhere.

## Decisions and their reason

| Decision | Reason |
| --- | --- |
| Two chips: the radio chip talks, the security chip holds secrets | The read protection of the radio chip has been beaten before. |
| Two ways of setup, chosen once | The security chip does not derive a key inside itself. A secret created in the chip can never be mirrored. |
| Screen and button at every approval | The computer must not decide what you approve. |
| No approval without a working screen | You could not see what you are approving. |
| Names and addresses are never cut off | A cut-off recipient is worse than no screen. |
| Level 1 without a fingerprint sensor | With FIDO2 it replaces the PIN, it does not add to it. No sensor met all four conditions. |
| Update space in the external memory | The internal memory of the radio chip is tight. |
| Own font and own checkers for the protocol | No third-party graphics or parser library, hard limits, no dynamic memory. |
| No third-party code | Everything is written against the specification. Encryption always comes from checked libraries. |

## Five languages

- No text is fixed in the code.
- Each language is a file in the folder `lang/`: `de.json`, `en.json`, `fr.json`, `it.json`, `es.json`.
- A test reports missing keys as an error.
- If a text is missing, the English one applies.

## Assembly

**New:** This section is new. More rules for assembly will be added here once the hardware is on the table.

- There is no binding rule for assembly yet. The earlier rule about the ferrite foil under the battery is void without the battery.
- Dimensions and rules are only in `hardware/layout-notes.md`. No number is written here on purpose, so that it cannot go stale again.

## Taking part

Never change only the technology. When function, operation, settings or structure change, the documentation belongs with it: `docs/` and this manual.
