# Vault format

Status: **implemented and tested on a PC**, never run on hardware — there is no
hardware yet. The tests use a RAM flash double, and the crypto and the SE050 are
test doubles too, so this describes a structure that has been exercised, not a
device that has worked.

The truth is `apps/firmware/src/classes/store/osk_vault_format.h`; this file
explains it. If the two disagree, the header is right and this file is a bug.

## What lives where

The vault owns one flash partition. Nothing else writes into it.

| Area | Size | Content |
| --- | --- | --- |
| header | 8 KiB (2 sectors) | version, nonce counter, addresses, slot bitmap — plaintext |
| index | 1 MiB | one encrypted generation per profile |
| log | 128 KiB | 512 slots of one page, each encrypted on its own |
| secrets | 8 MiB | 1024 slots of 8 KiB, one secret per slot |

Sectors are 4 KiB, pages 256 B. The geometry is checked against the real chip
when the vault is opened; a chip with other sizes is refused rather than
written to.

## Limits

| | |
| --- | --- |
| profiles per device | 4, each with its own PIN |
| items per profile | 64, so 256 in a device |
| longest secret | 4096 B (small files included) |
| name / address | 64 / 128 characters |
| log entries | up to 512, oldest dropped |

Items per profile is a deliberate limit: one index generation is held in RAM
(64 records of 320 B, so ~20 KiB, plus the same again as ciphertext while it is
written), and the nRF52840 has 256 KiB in total. Raising it means paging the
index, not changing a constant.

## The header

Plaintext, because nothing can be opened without it. It holds nothing secret:
format version, a sequence number, the nonce counter, where each profile's index
generation sits, and a bitmap of which secret slots are taken.

It is written as a sequence of equal-sized records across two sectors. Each new
record goes into the next free page; the valid one is the record with the
highest sequence number and a correct checksum. When a sector is full, the other
one is erased and writing continues there.

That gives three things at once:

- **Atomic switching.** A record torn in half by a power cut has a wrong
  checksum and is ignored, so the previous record still rules. Erasing only
  turns bits to 1, and `0xFFFFFFFF` is not a valid magic number, so a
  half-erased sector can never be mistaken for a record.
- **Wear spreading.** 16 records fit in a sector, so a sector is erased only
  every 16th change. Measured over 400 changes: 25 erases across the two
  sectors.
- **One place for the truth.** Which index generation counts, which slots are
  taken and how far the nonce counter has come are decided by exactly one
  record.

The slot bitmap is in the header, not in the index, so a slot stays taken while
its profile is locked and its index cannot be read. That leaks how many secrets
a device holds, nothing more.

## Nonces

A GCM nonce is 12 bytes: an 8-byte counter plus a 4-byte purpose (secret, index,
log, backup, transfer). The same counter value in two roles therefore never
gives the same nonce.

The counter only grows, and it is persisted **before** a nonce is handed out —
in blocks of 64, so that not every write costs a header record. On unlock the
vault continues at the persisted value and skips the gap: the nonces inside the
block count as spent whether they were used or not. A power cut therefore wastes
up to 64 nonces and repeats none.

Repeating a GCM nonce under the same key is the one mistake that breaks
AES-GCM completely — it leaks the authentication key, not just one message.
This is why the counter is persisted first and why the test keeps a register of
every nonce ever used and fails on the first repeat, across simulated power
cuts.

## Index

One generation per profile, encrypted as a whole with that profile's key. The
plaintext is a small header plus one fixed 320-byte record per item: id, type,
profile, flags, timestamp, which slot holds the secret, its length and nonce,
the search tag, the name and the address. Never the secret itself.

A generation is always written to a **new** place in the ring, and it only
becomes valid with the header record that points to it. So the index moves on
every change, which spreads wear by itself: measured over 400 changes, 146 of
the 256 index sectors were used, none more than once.

The additional authenticated data is the profile number. The format version is
inside the encrypted generation header, so a generation can be read without
knowing beforehand which version it is — that is what makes migration work.

## The search tag

An item can carry a tag of up to 32 bytes — not secret in itself, meant for
finding the item without opening it. `osk_store_find_tag()` searches it and
**decrypts no secret and asks for no confirmation**.

FIDO2 needs this. When a site asks for a passkey, the right credential has to be
found among hundreds; opening each one would be slow and would ask the user for
every single item, which makes a login unusable.

The tag sits **inside the encrypted index**, not in the plaintext header. A
plaintext list would be faster to search, but it would tell anyone who reads the
flash which services the user has an account with. So the search runs over the
index, which is in RAM anyway once the profile is unlocked, while the secrets
themselves are never touched. A locked profile finds nothing at all
(`OSK_ERR_DENIED`).

Two details that are tested:

- A **partial** tag matches nothing. The length has to be equal, so a tag cannot
  be guessed byte by byte, and the comparison has no timing difference.
- A tag longer than 32 bytes counts as **no** tag rather than being truncated. A
  half tag would find the wrong item.

## Item flags

| Flag | Meaning |
| --- | --- |
| mirrored | may be transferred to a second device |
| paired hosts only | only handed to a computer that has been paired |
| read-only | cannot be changed or deleted, by anybody |

`osk_item_meta_t` in `osk_store.h` carries the first two. The third needs
`osk_store_item_set_readonly()` from `osk_store_profiles.h`, because the shared
header has no field for it — worth adding there when the header is next changed.

## Profiles

Each profile has its own PIN. The key schedule is:

```
device  = SE050.hkdf(root, "osk/vault/device")        never leaves the chip
log     = HKDF(device, info "osk/vault/log")
profile = HKDF(device, salt SHA-256(PIN), info "osk/vault/profile<n>")
```

Two defences, on purpose:

- The PIN is checked **in the SE050**, which holds the try counter. Flash alone
  would allow endless guessing.
- The PIN also goes into the profile's key. So the PIN of one profile cannot
  open another — the key would be wrong and the authentication tag fails. That
  is tested.

The derivation texts carry no format version. Otherwise every migration would
need the PIN of every profile in order to build new keys.

## Secrets

Each secret sits on its own slot of two sectors with its own nonce. A plaintext
frame in front of it (magic, item id, length, checksum) can be checked without
any key, and the id in it says nothing about the content. The frame is the
additional authenticated data, so a secret cannot be moved to another item.

A slot is always erased and written whole. A changed secret always goes to a
**different** slot: as long as the new one is not valid, the old one must stay
completely readable. The allocator walks a cursor around all 1024 slots, so a
slot that just became free is not reused immediately and wear spreads over the
whole area — measured: 400 changes touched 400 different slots, none twice.

## A change, step by step

1. New secret onto a **free** slot.
2. New index generation to a **free** place in the ring.
3. **Header record** that points at both — from here the change counts.
4. Old slot released and erased.

A power cut before step 3 leaves the old header valid, so the old state. During
step 3 the checksum fails and the old record still rules. During step 4 a taken
but unused slot stays behind, and it is erased the next time it is needed. In
every case the vault opens and every item reads.

This is tested the only way it can be honestly tested: each kind of change is
first measured for how many write and erase operations it needs, then repeated
once per operation with the power cut exactly there, tearing that operation in
half. After each one the device is re-opened and checked: old state or new
state, nothing in between, every secret intact, no nonce repeated.

## Log

A ring of 512 page-sized slots. The plaintext holds time, reason, item id and
the name the asking program claims — never a secret. It is encrypted anyway,
because the list of services used is worth something by itself.

Two details came out of the power-cut test and matter:

- The sequence number does **not** say which slot a record is in. The next
  record goes into the slot after the youngest one. Otherwise, after a power
  cut, the same number would be written into the same slot a second time — and
  programming can only pull bits to 0, so the slot would hold nonsense.
- Only a slot that is really empty is written. If it is not, its whole sector is
  erased first, which drops the oldest records. That is what a ring buffer is.

The checksum sits at the end of the slot and covers the ciphertext too, so a
slot torn in half by a power cut is recognised instead of being counted as a
record that cannot be decrypted. Reading walks backwards from the youngest slot,
skipping invalid slots and stopping at an empty one, so every counted entry can
really be read.

## Versions and migration

The header carries the format version, and so does every index generation:

| Version | Record | What it added |
| --- | --- | --- |
| v1 | 240 B | the first format |
| v2 | 256 B | read-only flag, HOTP counter |
| v3 | 320 B | the search tag (current) |

Migration needs no step of its own and cannot lose data:

- An old generation is converted **in RAM** while it is loaded, so the vault is
  fully readable right after a firmware update. Every old format has its own
  converter straight to the current one, and the records are rewritten from the
  back forwards because they grow inside the same buffer. One converter per old
  format means more code than a chain, but each one is readable on its own and
  can be tested on its own - and both are tested, v1 and v2.
- What an old format did not have stays empty and is never guessed. A vault from
  v1 or v2 has no search tags, so FIDO2 sets them again on first use — a wrong
  tag would be worse than none.
- A version **newer** than this firmware knows is refused when opening
  (`OSK_ERR_FORMAT`) instead of being read with the wrong record size.
- Writing always produces the current format. Whoever changes something
  migrates that profile on the way.
- `osk_store_migrate()` does the same for all profiles at once, each as its own
  transaction. The version in the header rises only after the last profile, so a
  power cut leaves some profiles in the new and some in the old format — both
  readable. A locked profile cannot be migrated (no key, no index), and the
  function says so with `OSK_ERR_DENIED` instead of claiming success.

The keys do not depend on the format version, so migration is a rewrite and not
a re-keying.

## Backup file

A file for a disk or a NAS that can set up a new device. Its key comes from a
master passphrase through Argon2id and has nothing to do with the SE050 —
otherwise the backup would be tied to the very device that was lost.

```
[header]  magic, file format, vault format, Argon2id parameters, salt - plaintext
[chunk]   one per item: 4 byte length + ciphertext
...
[chunk]   the closing one: number of chunks and a digest over all of them
```

Each chunk carries the search tag along with the item — otherwise FIDO2 would
not find its credentials after a restore. Each chunk is encrypted on its own, so
the device never holds more than one item in RAM. The file header and the chunk's running number are the additional
authenticated data: a chunk cannot be moved into another file or to another
position, and lowering the Argon2id parameters in the header makes the file
unreadable instead of weaker. The closing chunk makes a truncated file fail.

**Passphrase.** At least 12 **characters** — not digits. A minimum number of
digits drives people into birth dates; characters allow a phrase people actually
remember. The device suggests eight words from a list of 256 (8 bits per word,
so 64 bits), and the list is in `osk_wordlist.c`: lowercase a-z only, 3 to 7
letters, no word the prefix of another.

**Argon2id on a stick.** The nRF52840 has 256 KiB of RAM, so the memory cost
has to stay small: 64 KiB and 64 passes. The strength therefore has to come from
the passphrase, which is why eight words are suggested rather than requested. A
management program on a computer may write the file with large parameters
instead (64 MiB, 3 passes); the parameters are in the file, and a device that
cannot manage them says `OSK_ERR_UNSUPPORTED` instead of running out of memory.

**Envelopes.** The 32-byte file key can be split with Shamir's scheme into n
parts of which k are enough (`osk_shamir.h`), printed as one line of text each
for sealed envelopes. Relatives then need k envelopes and no passphrase. A
single envelope reveals nothing, and each part carries a 4-byte digest of the
secret, so too few or mistyped parts give an error instead of a wrong key.

Restoring adds the items from the file to whatever is already there, one atomic
transaction per item, with new ids — the old ones belong to another device.

## Device to device

The computer or radio link in between only ever sees ciphertext.

1. Each device has its SE050 create an ephemeral P-256 key and hands out only
   the public part.
2. The public parts are exchanged over the untrusted path.
3. Each SE050 computes the same shared secret (ECDH). The computer in between
   cannot: it has neither private part.
4. From the shared secret come the transfer key and a six-digit number. Both
   displays show it, the user compares and presses. Both the key and the number
   are derived over the **sorted pair of public parts**, which is why a matching
   number proves both sides have the same partner. Someone in the middle
   produces two different numbers — that is tested.
5. The vault travels chunk by chunk, same framing as the backup, with the
   sorted public parts as additional authenticated data.

What does not travel: items without the mirrored flag; the SE050 root secret
(that has the chip's own pairing path); and anything a fingerprint sensor would
produce, which is not in the vault at all and therefore cannot travel. Stage 1
has no sensor (see `../docs/threat-model.md`); when one arrives, its template
stays in the sensor and never reaches the vault.

Without the confirmed number no byte leaves the device, and a transfer that
breaks off is detected by the closing chunk.

## Known limits

- **Rollback.** The plaintext header is protected by a checksum, not by a MAC.
  Someone who can write the flash directly can put an older but genuine header
  and index back and roll the vault back to an earlier state. Everything stays
  authentic, so nothing notices. Fixing this needs a monotonic counter in the
  SE050 that is compared when opening. **Open.**
- **Metadata.** The number of stored secrets can be read from the plaintext slot
  bitmap, and the size of an index generation shows roughly how many items a
  profile has. Names, addresses and secrets stay encrypted.
- **Interrupted restore.** A backup restore or a device-to-device transfer that
  breaks off part way leaves the items read so far in the vault. Each one is
  atomic, so the vault is sound, but it is not all-or-nothing across the file.
- **Nothing on hardware.** Every number here was measured against a RAM flash
  double with test doubles for crypto and the SE050. Timing, erase durations and
  Argon2id cost on the real chip are unmeasured.
