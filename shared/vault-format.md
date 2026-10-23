# Vault format

Status: **implemented and tested on a PC**, never run on hardware — there is no
hardware yet. The tests use a RAM flash double, and the crypto and the SE050 are
test doubles too, so this describes a structure that has been exercised, not a
device that has worked.

The truth is `apps/firmware/src/classes/store/osk_vault_format.h`; this file
explains it. If the two disagree, the header is right and this file is a bug.

## What lives where

The vault owns one flash partition. Nothing else writes into it.

The partition has two regions, and that is a decision, not an accident.
**Region 0 is byte for byte what versions 1 to 3 had. Region 1 was appended
behind it in version 4.**

| Area | Offset | Size | Content |
| --- | --- | --- | --- |
| old header | 0 | 8 KiB (2 sectors) | the header of versions 1–3; read only, never written again |
| index, part 0 | 8 KiB | 1 MiB | encrypted index blocks |
| log | 1 MiB + 8 KiB | 128 KiB | 512 slots of one page, each encrypted on its own |
| secrets 0–1023 | 9.1 MiB | 8 MiB | 1024 slots of 8 KiB, one secret per slot |
| header | 9.13 MiB | 64 KiB (16 sectors) | version, nonce counter, block addresses, slot bitmap — plaintext |
| index, part 1 | 9.19 MiB | 3 MiB | encrypted index blocks |
| secrets 1024–3071 | 12.19 MiB | 16 MiB | 2048 further slots of 8 KiB |

**28.2 MiB in total** (29,564,928 B). Sectors are 4 KiB, pages 256 B. The
geometry is checked against the real chip when the vault is opened; a chip with
other sizes is refused rather than written to.

Why appended and not enlarged in place: a slot number says where a secret
lives, so moving the areas would move every secret. A device upgraded from
version 3 would then have to copy its whole data area while exactly that data
is the only copy it has. Appending costs an ugly second region and two branches
of address arithmetic, and it costs **no** relocation at all — the upgrade never
moves a byte of secret data. The two old header sectors (8 KiB) are left lying
there once the upgrade is through; that is the whole price.

## Limits

| | |
| --- | --- |
| profiles per device | 4, each with its own PIN |
| items per profile | 768, so 3072 in a device |
| longest secret | 4096 B (small files included) |
| name / address | 64 / 128 characters |
| log entries | up to 512, oldest dropped |

**3072 is the number the budget allows, and here is the arithmetic.** One item
takes exactly one secret slot of 8 KiB, so 3072 items are 24 MiB of data area.
The index needs one 24 KiB place per 64 items — 48 places for a full device —
and a ring wants room to move, so it gets 170 places (4 MiB over the two parts),
3.5 times the live set. With the log and the header that is 28.2 MiB.

The chip has 64 MiB, and 29.5 MiB of it is already spoken for: the three
portable programs (27.5 MiB, measured), 1 MiB for the second firmware slot and
about 1 MiB of file system. That leaves 34.5 MiB. **4096 items would not fit**:
32 MiB of slots plus a 2.7 MiB index is 34.9 MiB, over the line before any
margin. Getting to 4000 would mean a 4 KiB slot — and that would shorten the
longest secret and, worse, move every existing slot. Not worth it for 25 % more
entries; see `../docs/hardware.md`.

Items per profile is a limit of the **index block**, not of the flash: one block
of 64 records of 320 B is ~20 KiB of RAM, plus the same again as ciphertext
while it is written, and the nRF52840 has 256 KiB in total. Only ever one block
is in RAM, so 768 items cost exactly as much RAM as 64 did. Raising it further
means more blocks per profile (`OSK_IDX_BLOCKS_MAX`) and more slots — both are
flash, and the flash is what ran out.

## The header

Plaintext, because nothing can be opened without it. It holds nothing secret:
format version, a sequence number, the nonce counter, where each profile's index
generation sits, and a bitmap of which secret slots are taken.

It is written as a sequence of equal-sized records across a ring of 16 sectors.
A record is 1 KiB — four flash pages, because the slot bitmap for 3072 slots
alone is 384 B. Each new record goes into the next free slot; the valid one is
the record with the highest sequence number and a correct checksum. When a
sector is full, the next one in the ring is erased and writing continues there.

The ring is in region 1, not in the two old header sectors. It could not stay
there: four records per sector would mean an erase on every fourth change,
four times as often as before, and the area cannot grow forwards because the old
index ring starts right behind it. With 16 sectors the wear is now *better* than
it was — measured over 400 changes: 90 erases spread over 16 sectors, the worst
of them 6.

The size of `reserved` in the header is chosen so that the checksum is the last
field with no compiler padding behind it. The checksum is computed over
`sizeof` minus four bytes; with padding behind it, it would cover itself and
never match. There is a static assertion on that — it cost an hour to find once.

That gives three things at once:

- **Atomic switching.** A record torn in half by a power cut has a wrong
  checksum and is ignored, so the previous record still rules. Erasing only
  turns bits to 1, and `0xFFFFFFFF` is not a valid magic number, so a
  half-erased sector can never be mistaken for a record.
- **Wear spreading.** Four records fit in a sector and there are 16 sectors, so
  a sector is erased only every fourth record and the erases spread over the
  whole ring. Measured over 400 changes: 90 erases over 2751 writes, the worst
  single sector 6 times.
- **One place for the truth.** Which index block counts, which slots are
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

A profile's index is a row of **blocks** of up to 64 records, each block
encrypted on its own with that profile's key. The plaintext of a block is a
small header plus one fixed 320-byte record per item: id, type, profile, flags,
timestamp, which slot holds the secret, its length and nonce, the search tag,
the name and the address. Never the secret itself.

**Only one block is in RAM.** That is the point of version 4 and the reason the
limit could go from 64 items to 768 without the nRF52840 needing more memory.
Every block occupies the same 24 KiB place in the ring even when it is half
empty, so the header needs a list of 48 addresses and no list of lengths.

All blocks but the last are full. That makes the arithmetic trivial and needs no
directory: item number *n* sits in block *n / 64* at place *n % 64*. The number
of items per profile is in the plaintext header, so counting and bounds-checking
cost no decryption.

A block is always written to a **new** place in the ring, and it only becomes
valid with the header record that points to it. So the index moves on every
change, which spreads wear by itself. Measured over 400 changes of one item in a
profile that holds one item: 170 index sectors used, none more than twice — the
block is short there, so only the first sector of each place gets dirty. With a
**full** block of 20 KiB all six sectors of a place are used; measured over 200
changes: 377 index sectors, none more than once.

The additional authenticated data is the profile number alone — in every format
version. **The block number is inside the encrypted block header, not in the
additional data.** That is deliberate: a block can only be read without knowing
its version beforehand if the additional data is the same in all versions, and
that is what makes migration work. It protects just as well, because the
authentication tag covers the block header: a block with a different number
cannot be forged, and a genuine block moved to another block's place is caught
by the number after decryption. In versions 1 to 3 that field was zero, and a
version-3 index therefore reads as block 0 — which is exactly right, because
there was only one.

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
index, while the secrets themselves are never touched. A locked profile finds
nothing at all (`OSK_ERR_DENIED`).

### What a search costs

Since version 4 the index is paged, so a search has to read and decrypt **every
block of the profile** — the tag it is looking for can be in any of them. That
is the price of the higher limit, and it was measured rather than assumed
(`test_paging.c`, case "Suchen ueber die Kennung"):

| Items in the profile | blocks decrypted | bytes decrypted | per search, PC |
| --- | --- | --- | --- |
| 64 | 1 | 20 KiB | ~150 µs |
| 768 (full) | 12 | 240 KiB | 1.20–1.45 ms |

**The microseconds are a PC number and say nothing about the device.** They were
measured on a desktop against the crypto *double*; the real AES-GCM comes from
the nRF52840's CryptoCell and was never run. The honest numbers are the left
three columns: one block per 64 items, 240 KiB read and decrypted for a full
profile, and **no secret touched and no confirmation asked** — that part is
asserted, not timed.

Six repeated runs gave 1.20, 1.25, 1.25, 1.25, 1.30 and 1.45 ms, so the
measurement is stable to about 10 %.

What this means for FIDO2 is **open until it is measured on hardware**. 240 KiB
has to come off the QSPI flash and through AES-GCM for every passkey login. If
that turns out too slow, the shortcut is known and not built: one extra
encrypted block per profile holding only `{tag digest, item id}` pairs, 8 bytes
per item, so 3072 items fit in a single block — one decryption instead of
twelve. It costs a second write per change and one more invariant to keep, and
nothing measured says it is needed yet. It is not in version 4 for that reason.

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
2. The affected index block(s) to **free** places in the ring.
3. **Header record** that points at all of them — from here the change counts.
4. Old slot released and erased.

Step 2 is usually one block. Deleting is the exception: the last item moves into
the gap, and if the gap and the last item are in different blocks, **two** blocks
are written — both before the header record, so both become valid together or
neither does. That path has its own power-cut test over all 87 of its write
positions.

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
| v3 | 320 B | the search tag |
| v4 | 320 B | the index in blocks: 768 items per profile instead of 64 (current) |

**Version 4 did not change the record at all.** What changed is the plaintext
header and the layout of the partition. That makes the upgrade from version 3
the cheapest one so far:

- The old header is recognised by its `hdr_size`, read as `osk_hdr_v3_t` and
  converted **in RAM** at mount time — exactly the way old index records are
  converted. Its one index generation per profile becomes that profile's block
  0, at the address it already had. The item count was not in the old header; it
  is filled in when block 0 is first read, from the block's own count, and that
  is the whole truth because a profile with one block has no other items.
- **Not one byte of secret data moves.** Slots 0 to 1023 are still where they
  were, which is the reason region 1 was appended instead of the areas being
  enlarged.
- Header records from then on go into the new 64 KiB ring. The old record stays
  behind with a lower sequence number, and the youngest record always wins, so
  it never comes back. Mount reads both areas.
- A vault with mixed versions reads either way, so a power cut during the
  upgrade is the same non-event it was before — tested at all 10 write positions
  for each of v1, v2 and v3.

Migration needs no step of its own and cannot lose data:

- An old index block is converted **in RAM** while it is loaded, so the vault is
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
  bitmap, and since version 4 the plaintext header also gives the **exact**
  number of items **per profile** (in version 3 the size of an index generation
  gave it away to within a record). The block addresses are in plaintext too,
  because the vault has to be able to allocate index space without overwriting
  the blocks of a *locked* profile, and so it must know where they are even
  then. Names, addresses and secrets stay encrypted.
- **Interrupted restore.** A backup restore or a device-to-device transfer that
  breaks off part way leaves the items read so far in the vault. Each one is
  atomic, so the vault is sound, but it is not all-or-nothing across the file.
- **Nothing on hardware.** Every number here was measured against a RAM flash
  double with test doubles for crypto and the SE050. Timing, erase durations,
  the cost of a tag search and the Argon2id cost on the real chip are
  unmeasured.
- **8 KiB for a 32-byte password.** A slot is two sectors because the longest
  secret is 4096 B and a frame plus the authentication tag push it 36 bytes over
  one sector. For a password that is a 250-fold waste, and it is what caps the
  device at 3072 items. Two slot classes, or a 4060-byte limit and one-sector
  slots, would buy 6000 items — at the price of relocating every existing
  secret. **Open**, and only worth reopening if 3072 turns out to be too few.
