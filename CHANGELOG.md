# Changelog

## [Unreleased]

### Added
- FIDO2/CTAP2 core for the firmware (`0.0.1.0`): MakeCredential, GetAssertion,
  GetNextAssertion, GetInfo, ClientPIN (PIN/UV auth protocols 1 and 2), Reset,
  Selection, plus CTAP1/U2F and the USB-HID transport (channels, chaining,
  keepalive, timeouts). Extensions: hmac-secret (and therefore WebAuthn PRF),
  largeBlob, credProtect, minPinLength. Own strict CBOR reader and writer,
  canonical form only, no dynamic memory. Keys are derived from the root secret
  in the SE050 instead of stored, so the number of services is unlimited; the
  97-byte credential ID carries the derivation salt encrypted and
  HMAC-protected, and the MAC is checked before anything is decrypted.
  737 test cases and two fuzzers (5.3 million CBOR rounds, 50 million HID
  packets) green on a PC with AddressSanitizer and UndefinedBehaviorSanitizer.
  **Nothing has run on the device and nothing has run in a browser** — the board
  is not built. The SE050 was a test double: deterministic, but without real
  curve arithmetic, so the message structure is tested, not the cryptography.
  No FIDO conformance test, no audit. Details and open gaps in `docs/ctap.md`.
- Management program `osk` (Go): own HID layer for Windows, Linux and macOS, own
  CBOR, all 14 protocol commands, local web interface, command line, five
  languages, device simulator. Tested against the simulator only — no real
  device exists yet, and macOS has never been compiled.
- Formatting rule for the firmware (`.clang-format`, Zephyr style)
- Tools: `set_version.py`, `build.py`, `flash.py` (untested, no hardware), `release.py`, `check.py`, CI on Linux and Windows, `docs/build.md`
- Hardware groundwork — on paper, nothing built or measured: parts list with
  order codes, prices and sources (`docs/parts.md`), power budget and battery
  sizing (`docs/power-budget.md`), schematic as a netlist
  (`hardware/schematic.md`), pin assignment checked against the nRF52840
  (`hardware/pinout.md`), board shape and layout rules
  (`hardware/layout-notes.md`)
- Repository skeleton following the house standard
- Architecture, threat model and hardware notes in `docs/`
- Internal interface headers for firmware modules
- Protocol description between firmware and management program

### Changed
- Secure element is now `SE051C2HQ1/Z01XDZ` instead of `SE050C2`: the SE050E
  variant has no RSA at all, which PIV and OpenPGP need. The SE051C2 has
  secp256k1, Ed25519 and RSA, 104 kB of user memory instead of 50 kB, the same
  HX2QFN20 package, and is in stock. Evidence: AN12436 Rev. 2.4 Table 1,
  AN12973 Rev. 2.0 Table 3.
- Charger `MCP73832` instead of `MCP73831` (status output readable on a GPIO);
  no battery protection IC on the board, as purchasable cells already carry one
  — reverse-polarity protection instead; 150 mAh cell instead of 100 mAh;
  0.71" display instead of 0.96"; 3 V flash variant (`W25Q512JVEIQ`) instead
  of the 1.8 V one.

### Security
- First commissioning **must** replace the platform SCP03 keys of the secure
  element. AN12973 Rev. 2.0 Table 6 publishes the factory ENC, MAC and DEK keys
  in the clear, so with factory settings the I²C link between the two chips is
  not encrypted — contrary to what `docs/threat-model.md` claims.
