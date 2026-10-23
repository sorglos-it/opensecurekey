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
- Repository skeleton following the house standard
- Architecture, threat model and hardware notes in `docs/`
- Internal interface headers for firmware modules
- Protocol description between firmware and management program
