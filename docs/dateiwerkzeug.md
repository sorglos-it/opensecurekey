# File tool: `osk encrypt` and `osk decrypt`

Status: written, compiled, tested against a stand-in device. **Nothing has run
on real hardware.** The security chip has not been delivered.

Two formats, both built from their public specification. No foreign library is
linked, and no foreign code, structure, naming or comment was taken over; the
Go standard library does the arithmetic it already offers (X25519, HKDF, HMAC,
AES, AES-GCM), and what it does not offer is written here from the RFC:
ChaCha20-Poly1305 (RFC 8439) and AES Key Wrap (RFC 3394).

```
osk recipient --key <label>
osk encrypt --format <age|pgp> [--to age1...] [--key <label>] [--in f] [--out f]
osk decrypt --format <age|pgp> --key <label> [--in f] [--out f]
```

The format is always named. There is no default and no guessing from the file:
whoever mixes the two up should notice it here, not at the recipient.

## The two rules this tool is built around

**The private key never exists as a file.** There is no export command, no
temporary PEM, no scratch copy on disk, and the `FileIdentity` interface has
deliberately only two questions — `Public()` and `Agree()`. Decryption does not
fetch a key; it hands the device a foreign public point and receives the shared
secret: command 16 `agree` of `shared/protocol.md`, which is
`osk_se050_ecdh()` in the chip. What the program sees is a different value for
every file. A test proves this from the device log: during decryption the log
shows `agree` and never `reveal`. A private age key (`AGE-SECRET-KEY-1...`) is
refused with a message that says why.

**Block by block.** A large file is never read whole. Both formats chunk at
64 KiB of plaintext; the number lives in `FileChunkSize` in
`apps/desktop/classes/file_tool.go` and OpenPGP's header field is computed back
from it, so the size exists in one place only. A test re-computes the
ciphertext length from the chunk arithmetic, so "it is chunked" is measured and
not claimed.

## What is written, exactly

**age** (`age-encryption.org/v1`): header line, one `-> X25519` stanza per
recipient (ephemeral point, file key wrapped with ChaCha20-Poly1305 under
HKDF-SHA256 of the shared secret, salted with both points), an HMAC-SHA256 over
the whole header, then a 16-byte salt and the STREAM payload — 11-byte
big-endian counter plus one byte marking the last chunk.

**OpenPGP** (RFC 9580): version 6 PKESK with X25519 (algorithm 25), recipient
**not** named (the fingerprint field is empty — allowed, and right here: a file
should not say which stick it belongs to), session key wrapped with AES-128 Key
Wrap under HKDF-SHA256 over `shared || ephemeral || recipient` with info
`OpenPGP X25519`; then a version 2 SEIPD packet with AES-256-GCM, chunked, with
a closing tag over the total plaintext length. Inside sits a literal data
packet in partial lengths.

Why not the older cv25519 form: it pulls the key's fingerprint into the
derivation, which would need a complete self-signed key packet — and this
device can only sign with a purpose string in front (`NodeSign`). The old form
is not merely old-fashioned, it is unreachable with this device.

## Checks, and what each one is worth

Go tests. `apps/desktop/classes/file_vectors_test.go` measures the
self-written arithmetic against **published** numbers — RFC 8439 sections
2.4.2, 2.5.2 and 2.8.2, RFC 3394 section 4.1. That is the point of those cases:
a round trip only proves that one calculation agrees with itself, and a wrong
calculation would be wrong both ways and look green. (It earned its keep at
once: the composite example disagreed in exactly one byte, by exactly the
difference of two letters — which showed the sentence was wrong, not the
arithmetic.)

`apps/desktop/tests/dateiwerkzeug_test.go` measures the formats: round trip
byte for byte at 0, 1, 1000, 65535, 65536, 65537 and 197 383 bytes; **every
single byte** of a small file flipped and every one detected; truncation
detected; the wrong recipient unable to open and receiving zero bytes of
plaintext; several recipients each able to open alone while a third cannot; no
approval on the device means no decryption; a chip without ECDH says so when
the identity is registered, not at the first file.

Flipping every byte found a real gap while it was being written: OpenPGP does
not authenticate the outer packet lengths, and a lying length was silently
read past. It is now rejected (`pgpBodyReader.atCleanEnd`).

A foreign program as a counter-check: `gpg --list-packets`. The gpg on this
machine is 2.4.9, and it confirms **the envelope only** — packet type 1 with 76
bytes, then type 18 with exactly the rest. It does **not** know the versions
from RFC 9580 ("unknown version") and reads the body as nonsense afterwards.
So the contents are not counter-checked by a second implementation. That needs
gpg 2.5 or newer, and for age the `age` tool, which is not on this machine.

## The stand-in, and what it does not prove

Decryption is tested against `GuardFakeDevice` in `apps/desktop/classes/` — the
stand-in that already existed and that was built against
`shared/protocol.md`. It answers command 16 `agree` with the **raw** 32-byte
X25519 result, which is what `osk_se050_ecdh()` in
`apps/firmware/include/osk_se050.h` promises: "shared secret with a foreign
public key", nothing further.

A stand-in checks our own side of the contract, never the other side. These
assumptions of it are still open and can only be settled on the device:

1. **Raw or already derived.** The stand-in returns the plain X25519 result. If
   the applet applies a KDF of its own, every file written here is unreadable
   on the device, and both formats break — the whole key derivation sits on
   this one assumption.
2. **Point validation.** The stand-in rejects points of small order, because
   `crypto/ecdh` does. Whether the chip rejects them, and what it answers when
   it does, is unknown. An all-zero shared secret must not become a key.
3. **ECDH may be missing entirely.** A part set to FIPS mode in the factory
   refuses ECDH (AN12413 4.10.3.1). The stand-in can act this out (`NoECDH`)
   and the tool reports it when the identity is registered — but what the real
   chip answers in that state has not been seen.
4. **One approval per call.** The stand-in demands a press per `agree` unless a
   session grant is on. On the device, `agree` is "yes, or grant" per
   `shared/protocol.md`. A file with several recipient stanzas makes several
   calls, so it may demand several presses. Not measured; the firmware side of
   commands 15–18 does not exist yet.
5. **Key labels.** The stand-in derives an X25519 key per label from its seed.
   On the device a label is a persistent object in chip memory — 104 kB on the
   SE051C2, see `shared/protocol.md`. How many file keys fit is unknown.
6. **Length and timing.** The stand-in answers instantly. The real chip runs
   over I2C under SCP03, so a file with several recipients costs real time per
   stanza. No figure exists.

## Open points

- Counter-check against a second implementation: `age`, and gpg 2.5 or newer.
- The browser extension and the web interface do not offer the file tool yet;
  only the command line does.
- `--out` refuses to overwrite an existing file. There is no `--force`, on
  purpose: a mistyped output path should not cost the only copy of a plaintext.
