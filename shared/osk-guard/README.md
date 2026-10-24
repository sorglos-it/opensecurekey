# osk-guard — bind your program to the key

A small library for programs that should run only while the security key is
plugged in, and whose data should be unreadable without it. Five languages,
one interface, no dependencies: C, Go, PHP, .NET and Python.

**Read [../../docs/dongle.md](../../docs/dongle.md) before you ship anything
with this.** It says what each stage is worth and what it is not. A dongle
protection that promises more than it keeps is worse than none, because
somebody relies on it.

Status: built and measured against a simulated device. **No real hardware has
ever been involved.**

## The two stages in one sentence each

| Stage | What it does | What it is worth |
| --- | --- | --- |
| 1 — `present()` | asks the key a fresh random question, verifies the signature | stops someone walking off with a copy; **anyone with a disassembler cuts it out** |
| 2 — `seal()` / `unseal()` | a key that never leaves the chip; without it the data is not there | nothing to cut out — the bytes cannot be reconstructed |

Stage 1 alone is a yes/no answer inside your own program. Ship stage 2 for
anything that matters.

## Start in 3 steps

1. Read your key's public key once and put it in your program:

   ```powershell
   osk guard-key
   ```

2. Start the local service (it belongs to `osk`, not to your program):

   ```powershell
   osk guard-agent
   ```

3. Call the library. Go, for example:

   ```go
   guard, err := oskguard.Open("127.0.0.1:8300", devicePublicKey)
   if err != nil { os.Exit(1) }            // no key: do not start
   if err := guard.Present(""); err != nil { os.Exit(1) }
   ```

## The same interface in all five

| Call | Does |
| --- | --- |
| `Open(address, devicePublicKey)` | connect to the local service, check it answers |
| `Present(binaryPath)` | one round of question and answer, verified **here**; empty path = the running file |
| `Seal(label, extra, plain)` | close data with a key that never leaves the chip |
| `Unseal(label, extra, blob)` | open it again; impossible without the key |
| `License(product)` | read the licence from the vault and check the maker's signature |
| `NodePublic(label, curve)` | public part of a node key (`ed25519` or `x25519`) |
| `NodeSign(label, job)` | sign a job with the key inside the chip |
| `NodeVerify(pub, label, job, sig)` | check another node's job — needs no key |
| `NodeAgree(label, peerPub)` | shared secret; may report `error.guard.noecdh` |

Every call fails with an **error code**, the same string in all five
languages, for example `error.guard.wrongdevice` or `error.guard.noecdh`.
The sentence for each code, in German, English, French, Italian and Spanish,
is in [lang/](lang/) — the same text the `osk` program shows.

### The one thing not in the library

Switching the **grant** on ("unlocked once, then no button press per
operation") is not reachable from here, on purpose. A library a program links
in must not be able to lower the bar for the whole machine. That switch lives
in `osk` and needs a press on the device.

## Why the library verifies the signature itself

The local service could lie. Any program on the machine can open a socket on
the loopback and answer `OK`. So the library does not ask "is the key there?"
— it generates its own random question, sends it through, and checks the
answer against the public key compiled into your program. The service never
sees a decision it could fake.

That is also why `Present()` hashes your program file and puts the hash into
the question: a patched file gets a signature over a different message, and
the check fails.

## The wire protocol, in full

TCP on the loopback only, one request line in, one answer line out, UTF-8 is
never needed because **every argument and every value is lowercase hex**. A
dash `-` means "not set". Lines are at most 4 MiB.

```
ping                                   -> OK v=1
attest <nonce32> <binhash32>           -> OK sig=<64> pub=<32>
seal <label> <extra|-> <plain>         -> OK blob=<...>
unseal <label> <extra|-> <blob>        -> OK plain=<...>
license <product>                      -> OK product=<hex> holder=<hex|-> number=<hex|->
                                             tiers=<hex,hex|-> seats=<n> issued=<n> expires=<n>
nodepub <label> <curve>                -> OK pub=<32> curve=<hex>
nodesign <label> <job>                 -> OK sig=<64>
nodeagree <label> <peerpub32>          -> OK sec=<32>
```

Any request can answer `ERR <code>` instead, where `<code>` is one of the keys
in [lang/](lang/).

### What the device signs

The device puts the purpose in front itself. That is what keeps a job
signature and a presence answer from ever being the same bytes — otherwise
somebody could hand you a job as a "question" and get a valid job signature
back.

```
presence:  "osk-guard/attest/1" 0x00 nonce(32) binhash(32)
job:       "osk-guard/sign/1"   0x00 label 0x00 sha256(job)
```

Both are signed with Ed25519 over the message itself (not over a
pre-hash), so a verifier needs plain Ed25519 and SHA-256, nothing else.

### Sealed file format

```
"OSKSEAL1" | version(1) | salt(16) | nonce(12) | AES-256-GCM(ciphertext+tag)
```

The file key is `HKDF-SHA256(deviceKey, salt, "OSKSEAL1")`, where `deviceKey`
comes out of the chip for that `label` and nothing else. `extra` goes in as
additional authenticated data. Two files never share a key, because the salt
is fresh every time.

### Licence format

```
"OSKLIC1" | version(1) | length(2) | CBOR payload | Ed25519 signature(64)
```

Signed over `"OSKLIC1" 0x00 payload` with the maker's key. The payload keys
are `p` product, `h` holder, `nr` number, `t` tiers, `s` seats, `i` issued,
`e` expires (0 = never).

## Folders

| Folder | Contents |
| --- | --- |
| `c/` | `osk_guard.h`, `osk_guard.c`, SHA-2 and Ed25519 verification, `example.c`, `Makefile` |
| `go/` | `osk_guard.go`, `example/` |
| `php/` | `OskGuard.php`, `example.php` |
| `dotnet/` | `OskGuard.cs`, `example/` |
| `python/` | `osk_guard.py`, `example.py` |
| `lang/` | the five language files |
| `tests/` | `guard-sim` (device simulator + service) and `run.py` (runs every example) |

### No foreign code

Where a platform has no Ed25519 (C, .NET, Python), the verification is
written out here from the formulas in RFC 8032 — **verification only**, never
signing. That is on purpose: verification touches no secret, so there is no
side channel to get wrong, and a wrong result is a failed check, not a leaked
key. Signing stays in the chip, where it belongs.

## Running the examples

```powershell
python shared\osk-guard\tests\run.py
```

It starts the simulator, runs every example that has its tools installed, and
says plainly which languages were skipped and why. A skipped language is not
a pass.
