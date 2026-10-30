# The stand-ins, and what they assume

**Read on 2026-10-30.** This file is a reading of every stand-in (Attrappe) in
the project against the wording of the contracts it imitates. Nothing here was
measured on hardware — the SE051C2 has not been delivered and no test bench
exists, so every single check in this project has run against a stand-in.

The lesson this project learned on 2026-10-29 is the frame for everything
below:

> **A stand-in checks its own side of the contract, not the other one.**

That day CTAP addressed the device-wide signature counter with the reserved id
0, the vault looked for an index record for every id and found none for 0, and
on the device nothing would have been signed. Everything was green, because
`test/ctap/mocks.c` kept a device-wide counter of its own and ignored the id.
The section **Where the 2026-10-29 mistake still sits** shows where that same
shape sits today.

The contracts read: `apps/firmware/include/osk_se050.h`,
`apps/firmware/include/osk_store.h`, `shared/protocol.md`, and the head
comments of the real implementations in `apps/firmware/src/classes/`.

## How many there are, and where

**17 stand-ins in three languages.** Fifteen in C under
`apps/firmware/test/`, one in JavaScript, one in Go.

| # | File | Imitates |
| --- | --- | --- |
| 1 | `test/applets/mocks.c` (1110) | SE050 incl. RSA, store, UI, transport |
| 2 | `test/ctap/mocks.c` (863) | SE050, store incl. sigcount, UI, clock |
| 3 | `test/guard/mocks.c` (558) | SE050, store, device log, UI |
| 4 | `test/wallet/mocks.c` (464) | SE050 (secp256k1), store, UI |
| 5 | `test/msc/mocks.c` (209) | transport, a RAM drive, USB sink |
| 6 | `test/authz/fake_se050.c` (153) | SE050: hmac, random, sign, counters |
| 7 | `test/boot/fakes.c` (227) | boot flash, signature, hash |
| 8 | `test/se050/fake_bus.c` (274) | the I2C bus below the real driver |
| 9 | `test/store/fake_se050.c` (417) | SE050 for the vault, incl. counters |
| 10 | `test/store/fake_flash.c` (288) | external NOR flash (W25Q512JV) |
| 11 | `test/store/fake_crypto.c` (349) | `osk_crypto.h` (AEAD, hash, HKDF) |
| 12 | `test/store/fake_card.c` (184) | the SD data carrier |
| 13 | `test/store/fake_vcrypto.c` (46) | `osk_volume_crypto.h` |
| 14 | `test/store/fake_ui.c` (86) | UI port for the vault |
| 15 | `test/ui/fake_panel.c` (422) | OLED panel and the two buttons |
| 16 | `apps/web/tests/osk-attrappe.js` (305) | the `osk` program, for the extension |
| 17 | `apps/desktop/tests/simulator/main.go` (565) | the whole device, for the host side |

Not counted: `apps/desktop/classes/pkcs11/pkcs11_stub.go` — a build stub with no
behaviour, so there is nothing for it to assume.

**Five of them imitate the same contract.** `osk_se050.h` is stood in for by
#1, #2, #3, #4, #6 and #9 — six, in six different ways. That is where the
contradictions below come from.

## The open assumptions, worst damage first

The last column is the one to sort by. A wrong buffer length is a crash on the
first attempt and costs an afternoon. A wrong assumption about what the chip
hands back makes data unreadable and only shows up once there is data in it.

| # | Part | Stand-in | The assumption | What to measure on the device | Damage if wrong |
| --- | --- | --- | --- | --- | --- |
| 1 | FIDO2 keys | `test/ctap/mocks.c`, `test/applets/mocks.c` | `osk_se050_key_derive_transient()` succeeds. **The real driver returns `OSK_ERR_UNSUPPORTED`** (`se050.c:971`, reasoned at length), `docs/se050.md` says the SE05x cannot do it, and `test/guard/mocks.c` refuses it for exactly that reason. `osk_ctap_key.c:51` and `:76` are both built on it. | Nothing — this is already decided against the chip. The question is only which replacement (one persistent key per credential) FIDO2 gets. | **No FIDO2 credential can be created at all.** Not a wrong answer: no answer. Every green CTAP key test is green against a function the chip refuses. |
| 2 | Vault, files, PQC | `test/store/fake_se050.c`, `test/ctap/mocks.c`, `test/applets/mocks.c`, `test/guard/mocks.c` | `osk_se050_ecdh()` hands back something already mixed, 32 bytes long. #9 returns `SHA256(sorted public keys)`, #3 an HMAC, #1 an HMAC-SHA1 padded to 32, #2 a value the test plants. **None returns a raw X coordinate.** Reported open by strand N for `pqc` and the file tool. | Whether the SE051C2 gives the **raw** ECDH result or only a derived one, and how long it is. | **Every encrypted file and every server session unreadable**, and only noticed once real data is in them. For P-256 the length happens to match 32, so it will be silently wrong rather than crash. |
| 3 | Commissioning | `test/applets/mocks.c`, `test/guard/mocks.c` | `osk_se050_rotate_scp_keys()` returns `OSK_OK` and looks at nothing — not `current`, not `key_version`. `osk_se050_require_scp()` always succeeds. The contract says `current` must be the set that is valid right now, and that keyset **12** must be replaced too or NXP keeps a back door. | On a real chip, in this order: rotate 11, rotate 12, read back, only then `require_scp`. | **A bricked part, one per mistake.** `require_scp` cannot be taken back. If rotate silently failed and require succeeded, the channel is required with keys nobody knows. Tests cannot tell the two apart today. |
| 4 | Firmware update | `test/boot/fakes.c`; `tests/simulator/main.go` | `fake_verify()` verifies by **recomputing the signature** from the message alone — a keyless symmetric check, where the real thing is Ed25519 against a built-in public key (`osk_boot.h:68`). The simulator's `firmware` command accepts any `part` and answers OK with no check at all. | A real Ed25519 verify, and an image signed with the **wrong** key, on the device. | **Foreign firmware runs.** No stand-in on either side ever rejects a wrongly signed image, so "the signature is checked" has never been shown. |
| 5 | Backup / restore | `tests/simulator/main.go:432` | `backup` returns the literal `sim-backup` and encrypts nothing; `restore` accepts **any** non-empty blob and answers OK. `shared/protocol.md` calls it an encrypted blob. | A restore of a foreign, a truncated and a tampered backup, each of which must be refused. | **Silent data loss.** A user restores a damaged backup, is told it worked, and the vault is gone. Nothing in the host tests can catch a restore that does not verify. |
| 6 | Pairing | `tests/simulator/main.go:425` | `pair` answers with the constant number `"428 913"`. `shared/protocol.md` rule 3 says that number is computed over the pairing transcript **including both `hello` kem lists**, and the whole protection against a man in the middle rests on it. | The number computed on two real devices over the real transcript, and that it differs when a `hello` is rewritten. | **A man in the middle talks two hybrid-capable devices down to X25519 and neither notices.** No test can tell a device that binds the transcript from one that does not. |
| 7 | Flash, boot | `test/boot/fakes.c` vs `test/store/fake_flash.c` | The two stand-ins for the same flash disagree. #10 models real NOR: programming only clears bits (`&=`), and address and length must be page- and sector-aligned. #7 uses plain `memcpy` — bits go 0→1 without an erase — and `fake_flash_erase()` erases arbitrary byte ranges with no sector granularity. | Writing over a region that was not erased, and an unaligned write, on the real W25Q512JV. | **An update that bricks the device on the first try.** A writer that forgets an erase, or writes unaligned, is green under #7 and leaves ANDed garbage on the device; an erase of 100 bytes wipes 4 kB of neighbours. |
| 8 | Wallet | `test/wallet/mocks.c:90` | `osk_se050_key_import_ec()` **is not in `osk_se050.h` at all.** It is declared in `wallet_signer.h`, called by `wallet_signer.c:145`, has no real implementation, and the stand-in accepts every import and every policy. ECDSA is computed on the host side from the key the stand-in keeps in RAM. Reported by strand S; `docs/wallet.md:87` already names it. | Whether a real SE051 imports an EC private key at all, and under which policy. | **The wallet cannot sign on the device** — and the key passes through the nRF52840 on the way in, which contradicts "keys are born in the chip and never leave". |
| 9 | PIV, OpenPGP, CSR | `test/applets/mocks.c` | The chip pads `OSK_RSA_PAD_PKCS1_V15` itself. If it expects a finished block instead, the signature goes over the wrong bytes. Reported by strand M; `docs/csr.md` has it in full. | One PKCS#1 v1.5 signature against a known-good verifier. | **Certificates and signed mail that no one can verify — and it looks green on the PC.** Strand M calls it the same shape as the counter on 2026-10-29, and that is right. |
| 10 | Everything using the chip | all six SE050 stand-ins | `osk_se050_sign()` returns a DER SEQUENCE. The contract does not say so; the card side (`osk_card_sig_der_to_raw()`) relies on it. The six stand-ins return **DER of variable length** (#1, #2, #4), **64 raw bytes** (#3, #9) and **32 bytes** (#6). | What the chip actually returns, and its length, per curve. | **Every ECDSA request comes out crooked** — but this one fails loudly and at once, which is why it ranks below #9. |
| 11 | authz / realms | `test/authz/fake_se050.c:99` | `osk_se050_sign()` accepts `cap == 32` and writes 32 bytes. Every real curve needs at least 64. The contract names no minimum, so each strand invented one. | The smallest buffer the chip will accept, per curve. | **A signature buffer sized at 32 bytes**, i.e. a truncated signature or an overflow in whatever was built against this stand-in. |
| 12 | Data carrier | `test/store/fake_vcrypto.c` | Borrows the vault's GCM stand-in and passes it through — deliberately, and the file says so. On the device ChaCha20-Poly1305 is computed. Tag and nonce lengths match in both, so a mix-up would be silent. | That the nRF accelerator really does ChaCha20-Poly1305, and how fast. | The file tool is unusable through slowness rather than wrong — unless the two are confused somewhere, in which case nothing decrypts. |
| 13 | Data carrier | `test/store/fake_card.c` | A single block write always either succeeds whole or is refused. There is no torn-write injection for the card, unlike `fake_flash.c` which has one. `osk_card_sync()` always returns OK. | Pulling power during a block write on a real SD card, and whether `sync` really flushed. | **A half-written block in the user's files.** The rollback detection for the carrier has never been tested against a torn write. |
| 14 | Chip memory | `test/applets/mocks.c`, `test/guard/mocks.c` | `osk_se050_free_memory()` always returns `0x7FFF`. `osk_se050.h` and `shared/protocol.md` both spend a paragraph on warning before the limit instead of failing at it. | A chip that is more than half full, so a real figure comes back. | **The warning path has never run.** The device meets the limit by failing to create a key, which is exactly what the function was added to prevent. |
| 15 | PIN | `test/applets/mocks.c:82`, `test/ctap/mocks.c:50`, `test/store/fake_se050.c:8` | The retry limit is **8**, written out independently in three stand-ins. The real driver **reads** `max_tries` from the chip object's attributes (`se050.c:950`) — it is whatever commissioning created, not 8 by decree. Against the project rule that a number lives in one place only. | The retry limit the AuthObject is actually created with. | Three test suites are wrong about the lockout at once, and the PIN display shows the wrong number of attempts left. |
| 16 | Everything using the chip | all six SE050 stand-ins | **"Without a successful `open()` no other function answers"** — the first rule in `osk_se050.h`, enforced by the real driver (`if (!g_open) return OSK_ERR_IO`). **No stand-in enforces it.** #2 tracks `g_open` but no function consults it; #1, #3, #4 and #9 do not track it. Transient key lifetime ("until the next derive or until `close()`") is modelled by none of them. | Calling any function before `open()` and after `close()` on the device. | Every start-order mistake in the firmware is invisible in the tests and returns `OSK_ERR_IO` on the device. Broad rather than deep: it touches every module. |
| 17 | Vault, buffers | `test/ctap/mocks.c`, `test/applets/mocks.c` vs `test/guard/mocks.c`, `test/store/fake_se050.c` | A buffer that is too small gets `OSK_ERR_FULL` from the first two and `OSK_ERR_ARG` from the other two. The contract names neither. | Which the chip's driver returns. | Error handling that branches on the code takes the wrong branch. One afternoon. |
| 18 | Store hashes | `test/store/fake_crypto.c:252` | `osk_crypto_sha256()` is **not** SHA-256 — it is a 64-bit FNV mix expanded to 32 bytes. (Scope: CTAP has its own real SHA-256 in `ctap/osk_crypto.c`, and the search `tag` is written by CTAP and only compared byte-wise by the store, so the tag path is not affected.) | Nothing, as long as no store hash is also computed by the other side. | Low today; becomes high the moment any store-side hash has to agree with the host. Worth a look whenever store code starts hashing something shared. |
| 19 | Wallet, applets | `test/wallet/mocks.c:78`, `test/applets/mocks.c` | `osk_se050_random()` is a counter, not random — deliberately, so a run repeats. | Nothing. | None, in this direction. But no test ever sees the chip repeat a nonce, so nonce-reuse handling is untested. `fake_crypto.c` does count duplicates, which is why this is low. |
| 20 | Applets ECDH | `test/applets/mocks.c` | The 32-byte shared secret carries only **20 bytes of entropy** — HMAC-SHA1 output, with the first 12 bytes repeated into the tail. | Nothing. | None on the device (the chip gives full entropy). But no test can ever catch code that reads only bytes 20..32 as a separate key. |
| 21 | Extension | `apps/web/tests/osk-attrappe.js` | Six assumptions, written out in the file's own head comment: native byte order never measured on a big-endian machine; what the browser passes as the extension id; error texts are keys, not sentences; `reveal` answers at once, so the extension's 120-second deadline is unmeasured; `hello` reports an invented device; the vault is in RAM, so 768 entries per profile, list order and a full vault are not modelled. | The extension against a real `osk` and a real device. | Each one small on its own. This stand-in is the one that does it right: it is strict, it checks the **other** side of the contract, and it names what it cannot prove. |

## Where the 2026-10-29 mistake still sits

**`test/ctap/mocks.c:730`, `osk_store_sigcount_next()`, line two of the body:
`(void)cred;`**

The stand-in still keeps **one** device-wide counter — in a single file — for
every credential, and ignores the id it is handed. The real store
(`store_items.c:1454`) keeps two kinds apart: `OSK_SIGCOUNT_DEVICE` (which is
`0`) raises `hdr.sigcount_dev`, and every other id resolves to an index record
and raises `rec->sigcount`.

Three things follow:

1. **Two credentials share a counter in the tests** and have separate ones on
   the device. Any check that reasons across two credentials is green and means
   nothing.
2. **The error path is never taken.** A credential with no index record gets an
   error from the real `sigcount_rec()` and `OSK_OK` from the stand-in. That is
   the 2026-10-29 failure itself: the path the device takes is the one the
   stand-in cannot reach.
3. The number `0` is defined in `osk_vault_format.h:167`, a store-internal
   header, and referred to from `ctap/osk_sigcount.h`. **The contract both
   sides read, `osk_store.h`, does not mention it.** Someone writing a stand-in
   from `osk_store.h` alone cannot know that 0 is special — which is how
   `(void)cred` got there.

Strand B reported the first point on 2026-10-29 (`strang-b-ctap.md:40`) and it
stands as a comment in `src/classes/ctap/osk_sigcount.h`. It is known and not
fixed.

**Not my fix to make.** Reported to the master.

## Where two stand-ins contradict each other

Six of these, in the table above. Pulled out here because each one means two
strands have been building against different realities, and at least one of
them is wrong:

| Function | The disagreement | Table |
| --- | --- | --- |
| `osk_se050_key_derive_transient` | #2 and #1 make it succeed, #3 refuses it citing the real driver — **and #3 is the one that matches the driver and the docs** | 1 |
| flash programming | #7 lets bits go 0→1 and ignores alignment, #10 models real NOR | 7 |
| `osk_se050_sign` | DER of variable length (#1, #2, #4) vs 64 raw bytes (#3, #9) vs 32 bytes (#6) | 10, 11 |
| `osk_se050_key_public` | 65 raw bytes (#2) vs point behind a 2-byte wrapper (#1) vs a 32-byte hash (#3) vs HKDF output (#9) | — |
| `osk_se050_ecdh` peer format | peer must be 65 bytes and start 0x04 (#2) vs exactly 32 (#3) vs any non-empty length (#1) | 2 |
| `osk_se050_hkdf` | #2 allows only `OSK_KEY_ID_ROOT`; #1 allows any id but **refuses more than 20 output bytes**, which no contract limit supports; #3 and #9 allow any id and any length | — |
| `osk_se050_counter_create` on an existing id | #9 returns `OSK_ERR_EXISTS` and documents the AN12413 trap (a bare WriteCounter against an existing counter means "increment by one"); #6 creates a **second** entry at 0 and returns OK, after which `counter_read` still finds the old one | — |

`osk_se050_key_public` is the one to settle first of the unnumbered three:
`test/applets/mocks.c` builds PIV and OpenPGP certificates against a public key
with a two-byte wrapper in front, `test/ctap/mocks.c` builds FIDO attestation
against a bare 65-byte point. Both cannot be right.

## Where no stand-in stands at all

Not assumptions — gaps, listed so they are not mistaken for tested ground.

- **`kem` does not exist anywhere outside `pqc/`.** `shared/protocol.md` spends
  a page on why a fallback an attacker can force is the hole, and names three
  rules that all sit on the device: the method is fixed when the key is created
  (`agree` on a hybrid label must get `OSK_ERR_DENIED`), `0` and an absent
  `kem` are errors, and both `hello` kem lists go into the pairing number. The
  word does not appear in `apps/desktop/` at all, and the simulator has no
  command 20. Protocol.md says so itself for the firmware side; it does not say
  the host side is equally empty. Together with table row 6 this means **the
  downgrade protection is documented and unbuilt on both sides.**
- **Commands 14–18 and 20** on the firmware side, as `shared/protocol.md`
  already states. The simulator answers 15–18 by forwarding to
  `classes.GuardFakeDevice` — deliberately one implementation and not a second
  stand-in, which is the right call and worth copying.

## What the stand-ins get right

Worth naming, because these are the patterns the others should follow:

- **`apps/web/tests/osk-attrappe.js`** is strict by design: it rejects anything
  that does not match the wording of `native_messaging.go`, says what was
  wrong, and lists its own six open assumptions in its head comment. "It checks
  the other side of the contract, not its own" is its second line.
- **`tests/simulator/main.go`** signs with real Ed25519 and computes real
  X25519 and HKDF after a forgery was found in the old XOR-based answer, and
  routes the dongle commands through one shared implementation so two stand-ins
  cannot drift apart.
- **`test/store/fake_crypto.c`** checks the tag for real, decrypts into its own
  buffer so a caller never sees half-opened data, and counts nonce reuse.
- **`test/store/fake_flash.c`** models real NOR: bits only go 1→0, alignment is
  enforced, and a write can tear in half.
- **`test/se050/fake_bus.c`** counts an empty I2C frame as a fault instead of
  forgiving it, because the real SE050 hangs on one (errata I2C.2).
- **`test/store/fake_vcrypto.c`** says in its first line what it is not, and a
  `_Static_assert` breaks the build if the tag lengths ever diverge.
- **`test/store/fake_se050.c`** documents the AN12413 double-create trap and
  refuses to let a counter go backwards "not even in the stand-in".
