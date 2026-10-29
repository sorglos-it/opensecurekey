# Changelog

## [Unreleased]

### Added
- Card applications, strand C: **RSA in PIV and OpenPGP**, **PIV attestation per
  slot (F9)**, **static password** and **one serial number per device**.
  - **RSA** on the card side, against the contract in `include/osk_se050.h`.
    PIV gets RSA-2048 — SP 800-73-4 names an algorithm identifier for no other
    length, and the ones for 3072 and 4096 are recorded as **open** rather than
    guessed. OpenPGP gets all three lengths, because it carries the modulus
    length as a number instead of hiding it in an identifier.
  - **Two questions have to say yes before RSA happens**, and they are not the
    same question: whether it was built (`CONFIG_OSK_RSA`) and whether the
    delivered chip can do it (`osk_se050_rsa_sizes()`). The SE051**A2** cannot
    do RSA at all and answers on the same I²C address as the C2, so a part
    without RSA is reported at startup and both applications keep working
    without it. The option takes effect in exactly one function.
  - **PIV RSA signing deliberately refuses the raw private-key operation** the
    specification allows, and unwraps the PKCS#1 block instead. Slot 9E needs
    no PIN, and a raw operation on a freely chosen value would be a signing
    oracle for anything.
  - **The hash algorithm is never derived from the length.** SHA-512/256 and
    SHA-256 are both 32 bytes; it comes out of the DigestInfo OID, and a test
    feeds a SHA-512/256 DigestInfo and requires that it be refused.
  - **Attestation** builds an X.509 certificate per slot, signed by the F9 key,
    with the issuer name copied byte for byte from the stored F9 certificate —
    a rebuilt name breaks the chain. Fixed validity, because the device has no
    clock it may trust. No invented object identifiers: the slot, the serial and
    the release policy go into standard subject fields. No PIN and no button,
    because nothing secret is released and the F9 key only ever signs data the
    module assembles itself.
  - **The serial number** is derived in the security chip and is therefore
    unique per device and stable across restarts. It closes a real gap: until
    now every device carried the same number, and GnuPG tells cards apart by it,
    so two of our sticks on one computer looked like one card. It is an
    identifier and **not** a proof — what cannot be forged is the attestation
    signature over it.
  - **Static password**: the card side holds and validates, printable ASCII
    only; the typing belongs to the keyboard transport, and the seam points that
    way so the two strands build side by side.
  Both firmware variants — with and without RSA — build and run green with gcc
  and with clang, under AddressSanitizer and UndefinedBehaviorSanitizer; the
  counts and the exact commands are in `docs/smartcard.md`. The run found a real
  bug in the new code: the DER integer writer put its length in a single byte,
  which holds for every curve and breaks on a 257-byte RSA modulus.
  **Nothing has run on hardware** — level 2 of four.
- Setup wizard, read-only drive, signed updates and the device side of the
  dongle commands:
  - **Setup wizard** — four steps, not skippable, and the first question is the
    one that cannot be taken back: mirrored or this-device-only. Both ways state
    their price, the recommendation says why, and there is **no quiet default**.
    `mode` deliberately starts at 1, not 0, because CBOR reads an absent field
    as zero and that would have made "forgot the field" mean "mirrored".
    Printable emergency sheet that contains **no printed secret**.
  - **Read-only USB drive** carrying the three portable programs. Every write is
    refused, including the ones disguised as something else; the content only
    changes through a signed firmware update (`tools/mkimage.py`).
  - **MCUboot with the second slot in the external flash**, ed25519-signed, with
    rollback blocked and an abort test at every point of the update — after each
    one the device still starts.
  - **Device side of commands 14–18**, with the purpose strings kept apart, held
    in place by the forgery case the program side found.
  - **PKCS#11 now costs zero approvals** when listing, like the agent.
  10 firmware test suites, 94,281 checks green. **Nothing has run on hardware,
  and nothing has been compiled for ARM** — that is being set up separately and
  has not succeeded yet.
- Dongle and server use: software that only runs while the stick is plugged in,
  and servers that sign and encrypt through the stick instead of a key file on
  disk. **Two stages, and the documentation says what each is worth**: stage one
  is challenge/response bound to the program's own checksum — it can be cut out
  of a program by anyone with a disassembler, and claiming otherwise would be
  selling something. Stage two is a key that never leaves the stick, without
  which parts of the program or its data are not readable at all. Licences in
  the vault, server mode with the node key in the chip and a grant that is off
  by default, expires by itself and is logged. `osk-guard` as a small embedding
  library for C, Go, PHP, .NET and Python, with a running example each.
  14 mutation checks, 10 of 10 example runs, nothing skipped.
  **While rebuilding the shared simulator to sign for real, a forgery became
  visible**: the old attest call would have produced a valid dongle answer from
  a 64-byte challenge. Separate purpose strings now prevent it, held in place by
  two cases and a mutant — and the firmware must make the same separation.
  **No real device, no outside review**, and the firmware side of commands 14–18
  does not exist yet.
- Five integration routes, each proven by one real operation against the device
  simulator: **ssh-agent** (`git push` through it on Windows and Linux; the
  public part computed from the private key matches `ssh-keygen` character for
  character), **command line** (`osk run` hands the secret only to the child
  process — measured, the shell next to it sees nothing), **PKCS#11** as .dll
  and .so with 29 functions (`openssl` reports "Verified OK", a flipped byte is
  rejected, and `ssh -I ./osk-pkcs11.so` signed a `git push` with no agent and
  no key file), **browser extension** (a login field filled in a real Chromium;
  an entry for another address was refused — that refusal is the phishing
  protection, not a convenience), and a **local socket** where a refused request
  never reaches the device.
  42 mutation checks across three sets. Four of the strand's own tests were
  hollow at first measurement — they failed at an earlier wall and would never
  have tested the latch — and were sharpened until the mutants died.
  **No real device was involved. macOS is cross-compiled but untested.**
- Crypto wallet: BIP39 (12 or 24 words, optional passphrase), BIP32/BIP44 and
  SLIP-0010 derivation in the device, PSBT signing per BIP174 (the open route
  Electrum, Sparrow and Bitcoin Core all speak), Ethereum transactions
  (EIP-155, 2930, 1559) and messages (EIP-191, 712), plus a Ledger-compatible
  APDU layer so MetaMask and Electrum recognise the device without changes —
  protocol written from the specifications, no foreign code.
  **What the device cannot display legibly, it does not sign** without an
  explicit extra approval. That rule is the reason the display exists at all.
  1099 test cases and two fuzzers green, including the official BIP32 and BIP39
  test vectors. **No real transaction was ever sent** and nothing ran on
  hardware.
- Smart card side: USB-CCID (Windows, Linux and macOS bring the driver
  themselves — that is why this route was chosen), PIV per SP 800-73-4 with
  slots 9A/9C/9D/9E, OpenPGP card 3.4, Yubico-OTP and HMAC-SHA1
  challenge-response for KeePassXC and VeraCrypt. Keys live in the secure
  element, certificates in the vault; every signature asks for approval on the
  device. 564 test cases green. **No real card reader stack was involved.**
- NFC and Bluetooth transports. **NFC** as an ISO/IEC 14443-4 Type 4 tag with
  ISO-DEP framing, chaining and timeouts, carrying the same CTAP core as USB —
  the core was built transport-independent and this is the proof. Plus NDEF tap
  (open an address or type a one-time code with no app at all), switchable off.
  Every operation over NFC still needs the button pressed, with no exception for
  "NFC is brief anyway" — that is the defence against relay attacks from a
  pocket. **Bluetooth** as a GATT service for the management program only, not
  for FIDO: browsers do not support FIDO over Bluetooth reliably. Pairing uses
  LE Secure Connections with numeric comparison, encryption required, no "Just
  Works", and the button has to be pressed on the device; the radio address
  rotates against tracking.
  1367 test cases and two fuzzers (ISO-DEP and NDEF parsers) green.
  **No radio was measured** — there is no board. Everything here is written and
  compiled, nothing more.
- Display and on-device approval: SSD1306-family driver over SPI for both the
  target part (48×64) and the test board (128×64), size as a constant rather
  than an assumption. `osk_ui_confirm()` now draws a screen per occasion —
  login, signature, decryption, secret release, pairing, wipe — showing the
  service, the recipient and the amount **verbatim**. Long names wrap or scroll;
  nothing is silently truncated, because a cut-off recipient on a transfer is
  worse than no display at all. Two hand-built fonts covering all five
  languages, costing 5.3 KB of flash — 0.55 % of the usable 960 KB, counted
  from the generated tables rather than estimated. Button handling (short,
  long, double, debounce, timeout), status display, five language files.
  4094 test cases green on 48×64 and 4092 on 128×64, both with sanitizers.
  **Nothing has ever appeared on a real display**, and the SPI binding
  (`osk_ui_port_nrf.c`) has never been compiled — it needs the SDK.
- Vault for the firmware: the encrypted store behind every secret the device
  holds. Typed items (login, TOTP/HOTP, SSH key, certificate, PGP key, API key,
  recovery codes, BIP39 seed, licence, card, identity, Wi-Fi, note, small file),
  AES-256-GCM per secret with a counter-based nonce that is persisted before it
  is handed out, atomic writes so a power cut at any point leaves a readable
  state, an encrypted device log, backup files, Shamir-split master passphrase,
  and device-to-device transfer. Search runs over a non-secret tag so a passkey
  login does not have to open a single entry.
  **3072 items per device** (4 profiles of 768). The first version held 256,
  limited not by flash but by RAM — a whole index generation sat in memory.
  Since format version 4 the index is paged into blocks of 64 records and only
  one block is ever in RAM, so 768 items per profile cost exactly as much RAM
  as 64 did. 4096 would not fit in the flash budget; the arithmetic is in
  `shared/vault-format.md`.
  The price of paging: a search now decrypts every block of the profile —
  240 KiB for a full one, about 1.3 ms on a PC against the crypto double.
  **What that costs on the device is unmeasured**, and it happens on every
  passkey login.
  83,298 test cases green under gcc with AddressSanitizer and
  UndefinedBehaviorSanitizer, including an abort at every single write position
  (97 of them in the new paths alone) and a register that fails on the first
  repeated nonce. Nothing has run on hardware.
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
