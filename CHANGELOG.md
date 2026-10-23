# Changelog

## [Unreleased]

### Added
- FIDO2/CTAP2 core for the firmware (`0.0.1.0`): MakeCredential, GetAssertion,
  GetNextAssertion, GetInfo, ClientPIN (PIN/UV auth protocols 1 and 2), Reset,
  Selection, plus CTAP1/U2F and the USB-HID transport (channels, chaining,
  keepalive, timeouts). Extensions: hmac-secret (and therefore WebAuthn PRF),
  largeBlob, credProtect, minPinLength. Own strict CBOR reader and writer,
  canonical form only, no dynamic memory. Credentials use a 97-byte credential
  ID that carries its derivation salt encrypted and HMAC-protected, with the MAC
  checked before anything is decrypted.
  This was written against a design in which keys are derived **inside** the
  secure element, which the SE05x turned out not to support — see "Changed"
  below. The CTAP core itself is unaffected; what changes is where the key
  comes from, and that the number of services is bounded by chip memory in
  "this device only" mode rather than unlimited.
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
- SE050/SE051 driver for the firmware: T=1' over I2C (framing, CRC-16/X-25,
  retry, timeouts, chaining), APDU and TLV layer, and SCP03 as the mandatory
  encrypted channel between the nRF52840 and the secure element. A narrow bus
  interface keeps the same driver code usable against a test bus on a PC, which
  is why there are tests at all. AES and AES-CMAC always come from an audited
  library — PSA Crypto on the device, Windows CNG or OpenSSL in the test
  builds — never from this repository, and no vendor code was read or copied.
  872 test cases green on a PC under MSVC and under gcc with AddressSanitizer
  and UndefinedBehaviorSanitizer. Verified against published values: AES, CBC
  and CMAC against NIST SP 800-38A/38B, the CRC against its standard check
  value, and the SCP03 derivation input against the worked examples in AN12413.
  **Nothing has run on the device** — there is no hardware. The frame layer is
  additionally unconfirmed because NXP's own T=1' description (UM11225) is not
  publicly downloadable; those values are derived from ISO/IEC 7816-3 and
  marked as unconfirmed in one single file. Details in `docs/se050.md`.
- Repository skeleton following the house standard
- Architecture, threat model and hardware notes in `docs/`
- Internal interface headers for firmware modules
- Protocol description between firmware and management program

### Changed
- Secure element is now `SE051C2HQ1/Z01XDZ` instead of `SE050C2`: the SE050E
  variant has no RSA at all, which PIV and OpenPGP need. The SE051C2 has
  secp256k1, Ed25519 and RSA, more user memory than the SE050's 50 kB, the same
  HX2QFN20 package, and is in stock. Evidence: AN12436 Rev. 2.4 Table 1,
  AN12973 Rev. 2.0 Table 3.
  **How much memory depends on the ordering option, not on the variant**: the
  same table gives "46 kB to 104 kB", with "Full Feature" at 46 kB and "Maximum"
  at 104 kB. Which one the test board carries is being checked before ordering;
  at 46 kB roughly half as many credentials fit in "this device only" mode.
- Charger `MCP73832` instead of `MCP73831` (status output readable on a GPIO);
  no battery protection IC on the board, as purchasable cells already carry one
  — reverse-polarity protection instead; 150 mAh cell instead of 100 mAh;
  0.71" display instead of 0.96"; 3 V flash variant (`W25Q512JVEIQ`) instead
  of the 1.8 V one.
- **Stage 1 is built without a fingerprint sensor**; release is confirmed with
  button and PIN. With FIDO2 the biometric *replaces* the PIN — it is not an
  additional factor — so a sensor would make the key more convenient, not more
  secure. On top of that no available sensor met all four requirements:
  matching on the sensor, buyable in single units, openly documented without an
  NDA, and small enough. The board still routes the pin header and its load
  switch, so stage 2 can fit a sensor without a new board.
- Transmit power is capped at 4 dBm, which keeps the 25 mA `IEXT` limit of the
  nRF52840's REG0 stage valid and saves a second voltage converter; the display
  and the fingerprint rail get load switches so that two large consumers never
  run at once.
- Board grows to 70 × 24 mm so that battery and NFC antenna sit in different
  thirds, with a ferrite foil between cell and antenna. If the enclosure turns
  out too large, the capacity gives way, not the distance.

### Security
- First commissioning **must** replace the platform SCP03 keys of the secure
  element. AN12973 Rev. 2.0 Table 6 publishes the factory ENC, MAC and DEK keys
  in the clear, so with factory settings the I²C link between the two chips is
  not encrypted — contrary to what `docs/threat-model.md` claims.
  Two things are required and **neither alone is enough**:
  `osk_se050_rotate_scp_keys()` for key set 11 **and** key set 12 — set 12 is a
  recovery set that can rewrite set 11, and for generic products NXP holds it,
  so rotating only 11 leaves a back door (AN12973 section 2.3) — and then
  `osk_se050_require_scp()`, because the chip ships as `SCP_NOT_REQUIRED`
  (AN12413 section 4.6.2) and would otherwise answer commands arriving with no
  channel at all. The factory key values are deliberately not copied into this
  repository.
- The PIN is stored as an AESKey authentication object rather than a UserID, so
  that the SE050 reports the failed-attempt counter: for UserID objects it does
  not (AN12413 section 3.2.4.4, errata APP.3). Without that, the lockout
  promised in `docs/threat-model.md` would depend on the firmware counting
  along, and firmware is what the secure element does not protect against.
- A per-service key pair is created inside the chip and never leaves it, so the
  number of services is bounded by chip memory rather than unlimited.
  `osk_se050_free_memory()` exists so the device can warn before that limit
  instead of discovering it when creating a key fails. The chip reports only
  `0x7FFF` for anything at or above 32768 bytes, so the figure is a lower bound
  good enough to warn with, not to calculate with.
