# Dongle and server use: what it holds, and what it does not

Two things people ask for: *my program should only run while the key is
plugged in*, and *my servers should sign with the key instead of a key file on
the disk*. Both are built. This page says what each one is worth.

Status: built and measured against a simulated device. **No real hardware has
ever been involved.** The library is [`shared/osk-guard/`](../shared/osk-guard/).

## Read this first

A dongle protection that promises more than it keeps is worse than none at
all, because somebody relies on it. So, plainly:

**Stage 1 — asking the key "are you there?" — can be cut out of a program by
anyone with a disassembler and an afternoon.** It does not matter how the
question is phrased, how many times it is asked, or how the answer is
checked. The check is a branch in your own binary, and a branch can be
removed. Every vendor who tells you otherwise is selling something.

What stage 1 *does* do is make a copy useless to someone who just copies. That
is a real and common case — a colleague takes the folder home, a customer
installs it on a second machine — and stage 1 handles it. It handles nothing
beyond it.

**Stage 2 — a key that never leaves the chip — is different in kind.** There
is no branch to remove, because the data is not there. An attacker without
the key does not face a check that says no; they face bytes that cannot be
turned back into anything. If what you protect actually matters, stage 2 is
the answer and stage 1 is decoration.

## Stage 1: question and answer

The program makes up a 32-byte random value, sends it to the device, and gets
back a signature. It verifies that signature itself, against a public key
compiled into the program.

```
device signs:  "osk-guard/attest/1" 0x00 nonce(32) sha256(program file)
```

Three properties, each measured by a test:

- **Every question is new.** A recorded answer is worth nothing, because it
  was an answer to a different question.
- **A different key is noticed**, even one that signs perfectly well — the
  signature is checked against the key the vendor wrote into the program, not
  against the key the device sends along.
- **The program file is bound in.** Its SHA-256 goes into the question, so a
  patched file gets a signature over a different message and the check fails.

### Why the library verifies, and not the service

The library talks to a local service (`osk guard-agent`) over the loopback.
Any program on that machine can open a socket and answer `OK`. So the library
never asks "is the key there?" — it asks for a signature over its *own* random
question and does the Ed25519 verification in its own code. The service never
gets to make a decision it could fake.

That is also the limit of the arrangement: whoever can patch your program can
patch out the verification too. The service is not the weak point; the
program is, and that is unavoidable for any yes/no check.

### What binding the program file is worth

It stops a *modified* program from getting a valid answer. It does not stop a
modified program from ignoring the answer. Both statements are true at once,
and the second is the one people forget.

## Stage 2: a key that never leaves the chip

`seal()` closes data with a key derived inside the secure element; `unseal()`
opens it again. Without the device, the key does not exist anywhere, so the
data is not readable — not "refused", unreadable.

```
blob = "OSKSEAL1" | version | salt(16) | nonce(12) | AES-256-GCM(data)
key  = HKDF-SHA256(chipKey(label), salt, "OSKSEAL1")
```

Use it for whatever your program cannot work without: a data model, a rules
table, a configuration, code you load at runtime. Keep the part of the
program that is worth protecting inside a sealed blob, and the dongle stops
being a check and becomes a dependency.

### The honest limit of stage 2

- **The derived key reaches the computer's memory while it is in use.** It has
  to: the decryption happens there. Malware running at that moment can read
  it, exactly as it could read any secret the vault reveals.
- **A program that has run once with the key could have cached what it
  unsealed.** Stage 2 protects against *not having the key*, not against
  somebody who had it earlier and wrote the result down.
- **What never leaves the chip is the root secret.** That is why a copy of the
  machine is useless, and why a different label cannot derive another label's
  key.

So stage 2 is not magic either. It is simply a much better position: an
attacker needs the device at the moment of use, instead of needing a
disassembler once.

## Licences on the key

A licence says until when, with which tiers, and on how many seats a program
may run. The maker signs it; the program verifies it; the device only carries
it (`OSK_ITEM_LICENSE`).

```
"OSKLIC1" | version | length(2) | CBOR(product, holder, number, tiers, seats, issued, expires)
          | Ed25519 signature over "OSKLIC1" 0x00 payload
```

- **It cannot be forged**, because the maker's private key is not shipped, and
  because every binding verifies the signature itself. The local service hands
  over the signed block, not the fields — it used to hand over the fields, and
  then a fake service on the loopback could have minted licences without
  touching the program. That hole was found by one of the language bindings
  before anything relied on it.
- **It cannot be copied to another machine**, because it lives on the key, and
  a key is a thing, not a file.
- **It can be ignored**, by the same disassembler as in stage 1. Checking a
  licence is stage 1 in a different coat.

Reading it needs the vault open, and the device logs the access like any other
secret. Expiry is deliberately a separate question from the signature: an
expired licence is genuine and over, and a program that confuses the two tells
the user the wrong thing.

## Server use: the node key lives in the chip

A node signs its jobs with Ed25519 and agrees session keys with X25519, both
inside the secure element. Copy the server and the copy has no identity —
that is the entire point, and it is what
[`docs/threat-model.md`](threat-model.md) means by the row "copies the
server".

```
device signs:  "osk-guard/sign/1" 0x00 label 0x00 sha256(job)
```

The purpose string is put in front **by the device**, not by the computer.
That is why `attest` (stage 1) and `sign` (jobs) can never produce the same
bytes: without it, somebody could hand your server a job, have it asked as a
"presence question", and walk away with a valid job signature. A test covers
exactly this.

### The grant: both sides of it, next to each other

Without a grant, the device wants a button pressed for every single
signature.

- **Without the grant, a server stands still.** Nobody is standing in the
  server room at 3 a.m. to press a button. Measured against the simulator:
  the signature is refused with `OSK_ERR_DENIED`, and that is the correct
  behaviour, not a bug.
- **With the grant, malware on that server can sign** — anything it likes,
  for as long as the grant lasts, without touching the device. There is no
  version of this feature that avoids that. A machine that can sign without a
  human can be made to sign by whatever else runs on it.

Both sentences are true. The grant exists because the first one makes the
server use case impossible, and it is dangerous because of the second one. So
it is fenced in:

| Fence | What it means |
| --- | --- |
| Off by default | there is no setting that turns it on quietly |
| Explicit | it has to be switched on knowingly, with a button press on the device |
| Named keys only | a grant lists the keys it covers; everything else still asks |
| Time-limited | it expires by itself, at most 24 hours, with no action needed |
| Logged | every signature, and the grant itself, is in the device log |
| Not reachable from the library | `osk-guard` has no call for it; a linked-in library must not be able to lower the bar for the whole machine |

If you run this on a server, assume the signing key can be used by anything
that gets root on that server while the grant is active. Keep the grant short,
name one key, and read the log.

## When the delivered chip cannot do ECDH

A delivered SE051 can be configured at the factory for FIPS mode, and such a
part **refuses ECDH outright** (AN12413 4.10.3.1). The feature bits that
decide this are set at the factory and cannot be changed. One call to
`osk_se050_open()` on the delivered chip settles it.

On such a part:

- **Signing still works.** Jobs are signed, nodes still have identities, and
  stage 1 and stage 2 are unaffected.
- **Key agreement with the key inside the chip does not exist.** The program
  reports `error.guard.noecdh` and says which use case is gone.

What we deliberately do **not** do is compute the key agreement in the
firmware instead. It would work, and it would look the same from outside, and
it would be a lie: the key would then live in the nRF52840's memory, where a
copy of the server could reach it. That is not "slightly less secure", it is
the withdrawal of the one promise the server use case makes. So the program
says so instead — in all five languages, in a sentence that names the use
case and does not look like a missing PIN.

## What none of this protects against

| Attacker | What happens |
| --- | --- |
| Copies your program folder | stage 1 stops them; stage 2 leaves them with unreadable data |
| Has a disassembler and time | removes every stage-1 check, including the licence check |
| Has a disassembler and wants your sealed data | needs the device at the moment of use; without it, there is nothing to remove |
| Reads your program's memory while it runs | sees the derived file key and whatever was unsealed |
| Gets root on a server with an active grant | can sign anything, until the grant expires |
| Has the device | everything it allows, which is why the device asks and logs |

## Protocol additions

The dongle part needs four commands beyond
[`shared/protocol.md`](../shared/protocol.md). Numbers 15–18 are proposed by
this work; `attest` (14) gains two fields.

| `c` | Name | Data | Needs approval |
| --- | --- | --- | --- |
| 14 | `attest` | `ch` nonce(32), `bh` program hash(32) → `sig`, `pub` | configurable |
| 15 | `derive` | `info`, `n` → `key` | no (never returns the root secret) |
| 16 | `agree` | `k` label, `pub` peer(32) → `sec`; may answer `OSK_ERR_UNSUPPORTED` | yes, or grant |
| 17 | `session` | `on`, `k` labels, `ttl`, or `ask` → state | yes, to switch on |
| 18 | `sign` | `k` label, `h` hash(32) → `sig`; or `pub`+`cv` for the public part | yes, or grant |

`attest` and `sign` are separate commands on purpose — see the purpose strings
above. Both must put the purpose in front on the device side; a firmware that
signs whatever it is handed breaks the separation no matter what the host does.

## Using it

1. Read the device public key once and put it in your program:
   `osk guard-key`
2. Start the local service: `osk guard-agent`
3. Call the library — see
   [`shared/osk-guard/README.md`](../shared/osk-guard/README.md) for all five
   languages.

## What was measured, and what was not

Against the simulator in `apps/desktop/classes/guard_device.go` (real
Ed25519, X25519, HKDF and AES-GCM; only the button press and the chip itself
are simulated):

- The Go side: every property above has a test, and **six mutation controls**
  run — five mutants that must die and one that must live. All six behave as
  required. A seventh mutant, which removed a second binding of the seal
  label, *survived*: the label was bound in three places and only one of them
  could be tested. The two redundant ones are gone, which is why the test now
  measures the binding instead of the wall in front of it.
- The examples in all five languages run against the simulator, in both
  variants — a chip that can everything, and a chip that refuses ECDH.

Not measured, and not claimed:

- **No real device.** Everything on this page is true of the simulator and of
  the code; whether the delivered SE051 allows ECDH is measurement number two
  in [`docs/se050.md`](se050.md) and is still open.
- **No audit.** Nobody outside this project has looked at the crypto.
- **The firmware side of commands 14–18 does not exist yet.** The host side
  and the simulator do.
