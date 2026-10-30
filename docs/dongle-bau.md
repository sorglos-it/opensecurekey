# Building with the dongle: one contract for five languages

This page is for whoever changes `shared/osk-guard/`. It says what the five
implementations have in common, why the interface is shaped the way it is,
what happens when the key is pulled out, and what the mock does **not**
settle.

[`docs/dongle.md`](dongle.md) says what the protection is worth.
[`docs/dongle-firmware.md`](dongle-firmware.md) says what the device
answers. This page says how it is built and measured.

Status: written, compiled, and run on a PC against a mock. **Nothing has run
on a device.** The SE051C2 has not been delivered.

## The one sentence that shapes the interface

**Only the second stage is real protection. A plain yes/no question can be
cut out.**

Whoever protects a program with `if (key_present())` has not protected it: an
attacker replaces the question with `true` and is done. Real protection means
**a part of the program lies there encrypted and only the key can decrypt
it** — then the attacker is not missing the answer, they are missing the part.

So the library has to make the weak way hard and the strong way easy.

### How the interface does that

| Decision | Why |
| --- | --- |
| **There is no call that returns true/false.** `present()` returns an error or nothing at all. | A function called `is_unlocked()` would be the most convenient one in the library, and it would teach every user to build the removable kind of protection. `tests/vertrag.py` refuses the name, in all five languages, together with `is_unlocked`, `IsPresent`, `stick_da` and four more spellings. |
| **`unseal_code()` is as short to call as `present()`.** | The strong way must not cost more typing than the weak one. One call takes the sealed part and the expected hash and returns the bytes — or nothing. |
| `seal_code()` returns the hash to write into the program. | Otherwise the user has to compute it themselves, and whoever does that by hand does it once and then never again. |
| `present()` says in its own doc comment, in all five languages, what it does **not** achieve. | A yes/no call is offered, because it does handle the common case of a copied folder. So the limit is written where the call is, not only in a document nobody opens. |
| Switching the **grant** on is not reachable from the library. | A linked-in library must not be able to lower the bar for the whole machine. That switch lives in `osk` and needs a press on the device; `tests/vertrag.py` refuses `session_on`, `GrantOn` and four more spellings too. |

### What `unseal_code()` does

```
blob      = seal(label, extra = sha256(code), code)
code      = unseal(label, extra = sha256(code), blob), then checked against sha256
```

The expected hash goes in as additional authenticated data **and** is checked
against the result. Two things follow: a container sealed for another part
does not open here, and a part that comes back changed yields no bytes at all
— the buffer is overwritten and the call fails with `error.guard.codehash`.
Half-decrypted data is worse than none.

## The contract, in one place

Five implementations of one decision drift apart. So the decision lives in
**[`shared/osk-guard/contract.json`](../shared/osk-guard/contract.json)** and
nowhere else: the wire, the purpose strings, the sizes, the container format,
the mandatory calls with their name in each language, the forbidden names, and
the rule for error codes.

`python shared/osk-guard/tests/vertrag.py` measures it against the sources.
It needs no chip, no compiler and no mock — it reads the files. It checks:

1. every mandatory call exists in all five languages,
2. no forbidden name exists anywhere,
3. every error key a source mentions has a sentence in `lang/de.json`,
4. every language brings its own test run.

That is what caught the state this work started from: `watch()` existed in Go,
Python and .NET and was missing in C and PHP, so the five did **not** behave
alike. The check was red with 9 findings before the work and is green with
204 cases after it.

## Recorded answers

The requirement is not "every question is new" — it is **a recorded answer
must be refused**. Those are different sentences, and only the second one is
measured by resubmitting a recording.

Two ways, both of them real:

- **Over the wire** (Go, Python): a service that signs the first question
  honestly and from then on returns *exactly those bytes*. Round one must be
  accepted; round two, three and four must fail with
  `error.guard.signature`, and the test also counts that the service was
  really asked again.
- **Against checked-in bytes**
  ([`tests/replay-vectors.json`](../shared/osk-guard/tests/replay-vectors.json)):
  six genuine signatures from `tools/ed25519.py`, one of which is valid for
  the question it belongs to and five of which must be refused — the
  recording, a patched program file, another device, the old free-length
  `attest` form carrying `nonce||hash`, and a job signature submitted as a
  presence answer. All five languages run this file, and none of them needs a
  signer for it.

The first case is the measuring stick. Without it a library that refuses
everything would be green too.

Both tests were brought to red before being believed: with the Ed25519 check
disabled in the Go library, two cases fail; with the C verifier made to accept
83-byte messages, all five refusal cases fail.

## The grant: explicit, and logged with a who

Off by default, on only with a press on the device, named keys, at most 24
hours, and it dies with a power cut or an unplug — the fences are in
[`docs/dongle-firmware.md`](dongle-firmware.md).

What this work added is the **who**. The log entry had an `asked_by` field and
the guard applet left it empty, so the log said that *something* had been
switched on and not by whom. It now records the channel the request arrived on
and the key that was unlocked, for example `usb-vendor:prod-eu-1`, plus the
device clock. A refused grant writes **no** entry at all: a log that records
attempts as successes is worse than none.

**Open point for the master thread:** the device cannot know more than that.
There is no field in which the computer says who is at work — and a field the
computer fills itself would be the suspect's own statement. A real user name
would need either a signed operator identity or a second device, and that is
a decision, not an implementation detail. The same goes for the time: the
device clock counts from plug-in, so the entry says device time, not UTC
(see `docs/dongle-firmware.md`, second open point).

## What happens when the key is pulled out mid-operation

The device locks immediately on unplug — that is decided. What the library
does about it is this:

1. **The round in flight fails.** `seal`, `unseal`, `unseal_code`,
   `license`, `nodesign` and `nodeagree` return `error.guard.nodevice`. No
   call returns half of anything: either the complete answer or an error.
2. **A job being signed is aborted.** There is no queue, no retry and no
   resumption in the library. The signature was not produced, so the caller
   must treat the job as unsigned — not as "probably done".
3. **`watch()` notices at the next round** (2 seconds by default) and calls
   `lost` **exactly once**, then returns. It starts no thread of its own: the
   caller decides in which thread it runs, and a protected program exits
   inside `lost`.
4. **Nothing is cached.** A sealed part that was already unsealed stays in
   the program's memory — the library does not clear it behind the user's
   back, and `docs/dongle.md` says why that is a limit of stage 2 and not a
   promise.

What the user has to do: plug the key back in, build a new `Guard` with
`open()`, and **submit the job again**. The grant is gone with the unplug, so
a server also needs its grant switched on again, with a press on the device.

### The decision this leaves for Thomas

For a server signing jobs for NextGenCloud, the three sentences above mean: an
unplug during a signing run loses that job, and the server needs a human at
the device before it signs again. That is the correct behaviour for a key that
locks on removal, and it is also an outage that nobody is standing next to at
3 a.m.

The three ways out, and none of them is the library's call:

| Way | What it costs |
| --- | --- |
| Leave it as it is: the job is lost, a human plugs in and switches the grant on again | A server with one key has an outage until somebody turns up. Honest, and cheap. |
| Two keys per server, the job goes to the second one | Needs a second device per server and a decision about which key NGC accepts — two identities, or one identity on two devices, which the chip cannot do. |
| NGC retries the job itself, and the signature arrives later | Moves the problem into NGC: a job then has a state "waiting for a signature", and that is a change to the order protocol, not to the dongle. |

**Decided on 30 October 2026: one key per server.** An unplug means an
outage until somebody is at the device. Two keys were rejected because they
do not remove the outage, they move it — the second key can be pulled out
too — and they would cost a second device per server plus two identities per
server inside NGC, because the chip cannot put one identity on two devices.

Where the lost job is caught is therefore **NGC's side, not the dongle's**: a
job gets a state "waiting for a signature" and is submitted again. That is a
change to the order protocol, and it also helps for every other reason a
signature can fail. It is not built, and this strand does not build it — it
belongs with whoever owns the order protocol.

So the library stays as it is: the round in flight fails, `watch()` reports it
once, and nothing pretends the job got signed.

## No foreign code, no dependencies

| Language | What it uses | Note |
| --- | --- | --- |
| C | standard library, system sockets | SHA-2 and Ed25519 **verification** written out here from RFC 8032 |
| Go | standard library only (`go.mod` has no requirements) | `crypto/ed25519` is part of Go |
| Python | standard library only | own `ed25519_verify.py`, verification only |
| .NET | the framework | own `Ed25519Verify.cs`, verification only |
| PHP | PHP itself, `sodium` | `sodium` has been part of PHP's core since 7.2; it is bundled, not fetched |

Verification only, never signing: verification touches no secret, so there is
no side channel to get wrong, and a wrong answer is a failed check and not a
leaked key. Signing stays in the chip.

A note on the PHP row, because it cost an hour: `sodium` ships with PHP but is
not switched on in every installation. On this machine `php_sodium.dll` sits
in the winget package's `ext` folder while PHP's compiled-in `extension_dir`
points at `C:\php\ext`, which does not exist — so `php -m` shows no sodium and
PHP looks like it has no crypto. `tests/run.py` already handles this (it tries
the folder next to the binary); a hand-run test needs it spelled out:

```powershell
$ext = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\PHP.PHP.8.4_Microsoft.Winget.Source_8wekyb3d8bbwe\ext"
php -d extension_dir=$ext -d extension=sodium shared\osk-guard\php\test_guard.php
```

## How to run all of it

```powershell
python shared\osk-guard\tests\vertrag.py
```

```powershell
python shared\osk-guard\tests\run.py
```

```powershell
python shared\osk-guard\tests\dotnet-test.py
```

```powershell
python shared\osk-guard\python\test_guard.py
```

```powershell
wsl make -C apps/firmware/test/guard run
```

`tests/run.py` starts the mock and runs every example twice — once against a
chip that can do everything, once against one that refuses ECDH. A language
whose tools are missing is reported as **skipped, with the reason**, and a
skipped language is not a pass.

`tests/dotnet-test.py` exists because the .NET test run had no runner at all
and was therefore never measured on a machine without the .NET SDK. It
borrows the compiler search from `run.py` instead of writing it a second
time.

## What was measured on 30 October 2026, and what was not

| Test run | Before | After |
| --- | --- | --- |
| `apps/firmware/test/guard` | 519 cases green (35 / 347 / 114 / 23) | **539 cases green** (35 / 347 / **134** / 23) |
| `tests/vertrag.py` | did not exist | **204 cases, 0 findings** (9 findings when first run) |
| C `test_guard` | 9 hashes, 21 signatures, 24 licences | same, plus **6 recorded answers, 1 valid** |
| Go | 71 cases | **74 cases** (3 of them recorded answers) |
| Python | 62 cases | **67 cases** |
| PHP | 21 signatures, 24 licences | same, plus **6 recorded answers** |
| .NET | never run on this machine | **green, including 6 recorded answers** |
| `tests/run.py` | 6 result lines per language | **7 result lines** (the program part) — 8 of 10 runs green, C skipped |

Not measured, and not claimed:

- **No real device, no chip.** The SE051C2 has not arrived. Every signature
  in these runs comes from software.
- **The C example** could not run here: this machine has no C compiler for
  Windows (only one inside WSL, whose binaries cannot reach the mock on the
  Windows loopback — and widening the loopback check to let them would be
  removing a fence to measure a fence). The C **library** is measured, in
  WSL, including the recorded answers.
- **No audit.** Nobody outside this project has looked at the crypto.
- **The firmware side still only runs against mocks**, as
  `docs/dongle-firmware.md` already says.

### What the mock does not settle

The mock checks our own side of the contract. These assumptions about the real
chip stay open, and each of them can only be answered by the delivered part:

| Assumption in the mock | Why it is open |
| --- | --- |
| The chip can do ECDH at all | A delivered SE051 configured for FIPS mode at the factory refuses it (AN12413 4.10.3.1). The mock has a switch for this and both paths are measured — which of the two the delivered part is, is measurement two in `docs/se050.md`. |
| Ed25519 takes the whole message, up to 128 bytes | From AN12413 4.10.1.2. If the delivered driver takes less, the limit of 105 bytes on the old `attest` form moves, and with it a documented number. |
| A key object per label, created at setup | The chip cannot derive a key pair internally without handing the material to the host, so this is a table. How many objects actually fit in 104 kB is measured on the part. |
| `ReadObject` refuses private and symmetric keys | AN12413 3.7.1. The whole promise "the key never leaves the chip" rests on this sentence, and the mock cannot prove it — it was written to behave that way. |
| The derived seal key is the same on every call for one label | The mock derives deterministically. Whether the part does, and whether it survives a power cycle, is unmeasured. |
| The device clock | counts from plug-in in the mock and on the device; since the battery was dropped there is no clock that keeps running between two uses, so every log time is a device time until the computer hands over UTC. |

The lesson from 29 October 2026 stands behind that table: a mock that keeps a
counter differently from the real other side makes everything green while the
device would not have signed at all. These rows are the places where that
could happen again.
