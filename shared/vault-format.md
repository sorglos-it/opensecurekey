# Vault format

Status: **written and compiled, exercised by PC tests** (stage 2 of 4: written / compiled / in the image / run on the device), never run on hardware — there is no
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
| log entries | up to 512, oldest dropped, hash-chained |
| wipe PINs | one per profile, off by default |

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

| Items searched | blocks decrypted | bytes decrypted | per search, PC |
| --- | --- | --- | --- |
| 64 (one block) | 1 | 20 KiB | ~100–150 µs |
| 768 (one full profile) | 12 | 240 KiB | 1.20–1.45 ms |
| 3072 (all four profiles) | 48 | 962 KiB | 4.8–5.9 ms |

The last row is the whole full device, which is the closest this geometry gets
to the 4000 entries the question was asked about — 4000 do not fit, see
**Limits**. It works out at about 1.8 ms per 1000 entries on this PC, so the
cost is linear in the number of entries, as the block count says it must be.

**The microseconds are a PC number and say nothing about the device.** They were
measured on a desktop against the crypto *double*; the real AES-GCM comes from
the nRF52840's CryptoCell and was never run. The honest numbers are the left
three columns: one block per 64 items, 240 KiB read and decrypted for a full
profile, and **no secret touched and no confirmation asked** — that part is
asserted, not timed.

Six repeated runs of the one-profile case gave 1.20, 1.25, 1.25, 1.25, 1.30 and
1.45 ms, five runs of the whole-device case 4.8, 5.0, 5.3, 5.5 and 5.9 ms — so
the measurement is stable to about 10 %.

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

## The public part

An SSH key, a certificate and a PGP key have a part that is **not secret**: the
public key. Since version 5 an item can carry it, and `osk_store_public()`
hands it out **without asking for confirmation and without reading the slot the
secret sits on**.

That is not convenience, it is the opposite of a weakening. The ssh-agent has
to name *all* identities to `ssh` before anything is signed. Through
`osk_store_reveal()` that would be one press per key just to learn a name — and
a user who is asked for presses that do not matter learns to press without
looking. The one press that *does* matter, at signing time, is then worth
nothing, and that press is the core of the whole device.

Two places it can come from:

- **The chip.** If the private key lives in the SE050, the record holds its key
  id and the public part comes from `osk_se050_key_public()`. The vault keeps no
  copy and needs none.
- **The vault**, on a slot of its **own** — never together with the secret.
  That is the whole reason the promise above can be kept, and it is measured,
  not asserted: the flash double counts reads per sector, and over five calls
  the two sectors of the secret's slot are read **0** times, while a
  `reveal()` of the same item reads them twice. The counter is proven to fire
  before the zero is believed.

It is encrypted anyway, with the profile key. Not secret does not mean that
anyone who reads the flash should get the list of the user's keys — the same
argument as for the search tag.

Three details that are tested:

- A type that has no public part (password, note, TOTP) gets
  `OSK_ERR_UNSUPPORTED`, **not** a quietly empty buffer. An empty buffer looks
  like "no key stored" and would be an answer to a question that makes no sense.
- **A new secret drops the public part.** It belonged to the old key, and a
  *wrong* public key is worse than none: the agent would offer an identity it
  cannot sign for. It has to be set again afterwards.
- The record says `pub_len`, not `pub_slot`, when there is none. **Slot 0 is a
  valid slot**, so reading a 0 in `pub_slot` as "none" would mishandle exactly
  one item per device — the first one ever written.

How it gets in: `osk_store_item_set_public()` and
`osk_store_item_set_se050_key()` in `osk_store_profiles.h`. They are there and
not in the shared header because `osk_store_add()` has no field for it — the
same reason the read-only flag lives there. The vault deliberately does **not**
parse OpenSSH, X.509 or OpenPGP to peel the public part out of the secret: that
would be format work on foreign data in the middle of the security core, and it
would have to read the secret's slot — precisely what this path must not do.

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

## Rollback protection

The plaintext header carries a checksum, not a MAC, so anyone who can write the
flash can put back an older but completely genuine header and index. Everything
about that older state is authentic — right checksums, right authentication
tags, right format — which is exactly why nothing inside the flash can notice
it. **That valid old state is the attack.** A corrupted one is caught by the
encryption already.

The anchor therefore does not live in the flash at all. It is a monotonic
counter in the SE050 (`OSK_KEYID_VAULT_GEN`, `osk_se050_counter_*`), and the
number it holds is the header's sequence number — the one that already rises by
one with every commit. No second number was introduced: two quantities that are
meant to mean the same thing drift apart sooner or later.

**The order is the whole trick.** The header record is written first, the chip
counter is raised afterwards:

| state after a power cut | flash | chip | what happens next |
| --- | --- | --- | --- |
| cut before the header record | n | n | nothing changed |
| cut between record and raise | n+1 | n | the vault opens and raises the chip on the next open |
| raised, all well | n+1 | n+1 | — |
| **flash rolled back** | n−k | n | `osk_store_open()` returns `OSK_ERR_ROLLBACK` |

Raising the other way round would be the one failure that cannot be recovered
from: the chip would hold a number the flash never reaches, and the device
would be bricked by a power cut. `osk_se050_counter_raise()` is repeatable —
already at or above the value, it changes nothing and still says OK — which is
what makes the catch-up safe.

Two consequences that are easy to get wrong:

- **A fresh vault does not start at 1.** `osk_hdr_format()` reads the chip
  counter and starts the sequence above it. Otherwise erasing the flash would
  be the way back: wipe, restore an old image, done. This also covers the wipe
  PIN below.
- **The counter's initial value is read, not assumed.** AN12413 does not say
  what a freshly created counter starts at, so the code asks. There is a test
  that starts the double at 5000 and fails any firmware that quietly assumes 0.
- There is deliberately no `counter_set()` and no `counter_delete()` in
  `osk_se050.h`. Both would be a way back.

`osk_store_generation()` reports the sequence number.

The test (`test_guard.c`) proves the replayed image is **sound in itself**
before it proves it is refused: the image is restored once while the chip
counter still matches, and the vault opens and reads the secret. Without that
first step the later refusal could just as well come from a broken image, and
the test would be measuring something other than what it claims.

## The wipe PIN

A second PIN per profile whose entry erases the vault. Off by default
(`osk_store_wipe_pin_set` / `_clear` / `_is_set` in `osk_store_profiles.h`).

- The verifier in the header is **not** a hash of the PIN. It is derived
  through the device-bound value from the SE050, so the number is worth nothing
  on another machine. It has to sit in the plaintext header because it is
  checked while the profile is still locked — that is when the PIN is typed.
- It is checked **before** the chip PIN. The chip would reject it and burn a
  try, so after a few attempts the device could not be erased at all.
- The answer is `OSK_ERR_DENIED`, the same as a wrong PIN. Whoever is forcing
  the unlock should not see that anything happened, and whoever does not know
  there is a wipe PIN should not learn that there is one.
- It may not be the profile's real PIN; that would be a vault that erases
  itself on unlock. The attempt gives `OSK_ERR_ARG`.
- It is **not** a way around the rollback protection. The chip counter stays
  where it is and the fresh vault starts above it.

## Log

A ring of 512 page-sized slots. The plaintext holds time, reason, item id and
the name the asking program claims — never a secret. It is encrypted anyway,
because the list of services used is worth something by itself.

**The ring is hash-chained** (version 6). Each slot carries the first 16 bytes
of its predecessor's authentication tag, in plaintext so that the slot stays
readable after the predecessor has been overwritten by the ring, and as
*additional authenticated data* so that rewriting it destroys the record
instead of forging the order. The tag depends on the log key, which is rooted
in the SE050, so someone who can read and write the flash can neither forge a
record nor recompute a gapless chain.

`osk_log_verify()` walks back from the youngest record and checks two things
per step: the sequence numbers are consecutive (they are in the additional
data, so they cannot be renumbered), and the younger record's chain link is the
older one's tag prefix.

**It does not stop at an empty slot, and that is the point.** Reading does —
an empty slot is the end of the ring — and that made blanking one slot the
cheapest way to make everything older than it disappear without touching a
single chain link. The check therefore keeps walking the whole ring; a valid
record found *behind* a gap means the gap was made. A ring that has genuinely
wrapped has its empty run after the oldest surviving record and nothing beyond
it, so it passes — that counter-case is tested with 700 changes, because a
check that cries wolf on every device older than 512 entries is worth nothing.

Where the check honestly **stops** and reports nothing: at the oldest surviving
record, whose predecessor the ring has overwritten, and at a record from a
version before 6, which has no chain link.

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

## The FIDO signature counter

Counted in CTAP, stored here: `osk_store_sigcount_next()` / `_get()` in
`osk_store.h`. The item id it takes has **two fixed meanings**, and that split
is written down in both places (here and `osk_sigcount.h` in CTAP):

| id | counter | where |
| --- | --- | --- |
| 0 | the **device's** counter | `sigcount_dev` in the header record |
| anything else | that one item | `sigcount` in its index record |

Id 0 is free because the vault hands out item ids from 1 (`item_next`), and
`osk_store_add()` skips a 0 rather than trusting that — an old header brings
its own `item_next` along, and what stands there is not this subsystem's to
assume.

**Why the device needs one at all.** Most FIDO credentials have no index
record to hang a counter on: non-discoverable credentials are derived and
stored nowhere, and neither is U2F. A per-item counter could only count the
discoverable ones, and mixing the two would be worse than either — if a
credential ever fell back to the shared number, its counter could go *down*,
which is precisely the sign of a clone. The per-item counter stays for the
items that have a record; the device counter covers the rest.

The per-item field sits in four bytes of the index record that were zero in
version 5, the device field in four bytes of the header's `reserved`. Neither
changed a structure's size.

**What a login costs** (measured in `test_sigcount.c`, 1000 calls against the
RAM flash double, not estimated):

| | per 1000 logins | per login |
| --- | --- | --- |
| flash write + erase operations | 1235 | 1.24 |
| erases, all header sectors | 235 | 0.24 |
| erases, the worst single sector | 15 | 0.015 |
| SE050 counter raises | 1000 | 1 |

One header record per login, four records per sector, 16 sectors in the ring —
so the hottest sector sees an erase every 67th login. The SE050 counter is
raised once per login as well, because the rollback anchor follows every
commit; that is one NVM write in the chip per login. **Neither endurance
figure is cited here**: the flash's erase cycles and the SE050's counter
endurance are numbers from datasheets this subsystem has not read, and nothing
of this has run on hardware.

It is **not** the `counter` field in the same record. That one counts HOTP and
is shown to the user; this one goes to the relying party and is what proves the
key has not been cloned. Two counters, two purposes, two names — this project's
most frequent bug is two quantities that are called the same and measure
different things.

`_next()` raises and commits in one transaction and hands the value out only
after the header record, so a power cut gives the old value again rather than a
gap. It is called **before** signing: a counter raised after a successful
signature can be used twice on the same value by pulling the power at the right
moment. The test checks it across a restart, because a check that only counts
within one run is exactly the check that would have missed the bug it exists
for.

## Versions and migration

The header carries the format version, and so does every index generation:

| Version | Record | What it added |
| --- | --- | --- |
| v1 | 240 B | the first format |
| v2 | 256 B | read-only flag, HOTP counter |
| v3 | 320 B | the search tag |
| v4 | 320 B | the index in blocks: 768 items per profile instead of 64 |
| v5 | 320 B | the public part of a key, on its own slot or in the chip |
| v6 | 320 B | rollback anchor, hash-chained log, wipe PIN, FIDO signature counter (current) |

**Version 6 is the first upgrade that grows the plaintext header.** The record
stayed 320 B — the FIDO signature counter sits in four bytes that were
`reserved` and zero, so a version-5 record reads as "never signed" and nothing
is converted. The header, though, gained a 16-byte wipe-PIN verifier per
profile, so `sizeof(osk_hdr_t)` changed and with it the `hdr_size` that
identifies a record. A version 4/5 record is therefore read through
`osk_hdr_v5_t` and converted in RAM at mount time, exactly the way a version 3
record already was. Both areas are scanned, the highest sequence number wins,
and the records live in the same 1 KiB slots of the same ring — so nothing
moves and a power cut during the upgrade is the same non-event as before.

The log slot grew too, by the 16-byte chain link, and changed its magic from
`OSKL` to `OSKC`. Old slots are still read (so an update does not throw the
device log away); they simply cannot be chain-checked, and `osk_log_verify()`
stops there rather than reporting a break.

**Version 5 changed no byte that version 4 used.** The new fields sit in bytes
that were `reserved` and zero, so a version-4 block reads as "no public part,
no chip key" and there is nothing to convert — only the version number rises.
Tested like the others, including a power cut at each of the 9 write positions
of the upgrade.

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

- **Rollback — closed in version 6**, see *Rollback protection* above. What
  remains open is everything below the vault: a counter the SE050 itself never
  received (a bus the attacker controls) is not covered here, and none of it
  has run on hardware.
- **Metadata.** The number of stored secrets can be read from the plaintext slot
  bitmap, and since version 4 the plaintext header also gives the **exact**
  number of items **per profile** (in version 3 the size of an index generation
  gave it away to within a record). The block addresses are in plaintext too,
  because the vault has to be able to allocate index space without overwriting
  the blocks of a *locked* profile, and so it must know where they are even
  then. Names, addresses and secrets stay encrypted.
- **The public part does not travel.** A backup file and a device-to-device
  transfer carry the item and its search tag, but not the public part — the
  chunk has no field for it, and adding one means a new backup file format.
  After a restore an SSH key is there and usable, but the agent cannot name it
  until `osk_store_item_set_public()` has been called again. Same shape of gap
  as the missing tags after a v1/v2 upgrade. **Open.**
- **Interrupted restore.** A backup restore or a device-to-device transfer that
  breaks off part way leaves the items read so far in the vault. Each one is
  atomic, so the vault is sound, but it is not all-or-nothing across the file.
- **Deniability.** `docs/plan.md` names it twice — once as a task of this
  subsystem and once in milestone 17 — and nowhere says what it should mean.
  The possible readings differ in what they cost and in what they can promise:
  a hidden profile whose existence cannot be shown, a second passphrase on the
  backup file that opens a decoy set, or merely the fact that the plaintext
  header already reveals nothing but counts. The first of these cannot be built
  on this layout at all without a change that is visible from outside: the
  plaintext header gives the **exact** number of items per profile and the slot
  bitmap gives the number of secrets in the device, so a hidden profile would
  be countable. Nothing is built and nothing is guessed. **Open — needs a
  decision.**
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
