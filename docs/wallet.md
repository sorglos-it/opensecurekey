# Wallet — Bitcoin and Ethereum

What the key does when a wallet program asks it to sign a payment.

Status: written and compiled on a PC, 1099 checks and two fuzz runs green.
**Nothing has run on the device, no transaction has ever been broadcast, and
no real coins exist anywhere near this code** — the board is not built yet.

## Which specifications

| Specification | What it is used for |
| --- | --- |
| BIP39 | mnemonic words → seed (English wordlist, PBKDF2-HMAC-SHA512, 2048 rounds) |
| BIP32 | key derivation, extended keys (xprv/xpub) |
| BIP44 | account layout `m/purpose'/coin'/account'/change/index` |
| SLIP-0010 | the retry rule, and ed25519 derivation (hardened only) |
| BIP174 | PSBT — read, check, sign, hand back |
| BIP143 | signature hash for SegWit v0 |
| BIP13 / BIP16 | Base58Check addresses |
| BIP173 / BIP350 | Bech32 and Bech32m addresses |
| EIP-155 | Ethereum legacy transactions with a chain id |
| EIP-2930 / EIP-1559 | typed transactions (0x01, 0x02) |
| EIP-191 | `personal_sign` messages |
| EIP-712 | typed data, from the two finished hashes |
| EIP-55 | address spelling as a checksum |
| FIPS 180-4 / 202, RFC 2104 / 8018 | SHA-512, Keccak-f[1600], HMAC, PBKDF2 |

Everything is written against the specification. No code, no structure and no
names were taken from any other wallet.

## The rule the display exists for

**What the device cannot show in a readable form, it does not sign.**

`wallet_confirm.c` enforces that, not the callers. A signature needs a
recipient, an amount and a fee on the screen; if any of the three is missing,
the answer is "no" and the chip is never even asked. There is one way past it
— the owner can switch blind signing on at the device — and then the question
on the screen is a visibly different one.

These cases are *not* readable, by decision:

| Case | Why |
| --- | --- |
| Ethereum transaction with data | A contract call is not described by an address and an amount. |
| Contract creation (no recipient) | Same reason, and there is nothing to show. |
| EIP-712 typed data | The device gets two hashes, not the content. Checking the struct would mean a JSON reader and the whole type system on the device. |
| A message that is not printable text | Nothing to compare. |
| PSBT output whose script has no address | OP_RETURN and anything unknown. |
| PSBT input without a known amount | Without it the fee cannot be computed, and the fee is what the owner controls. |

## Where the seed lives

In the vault, as an `OSK_ITEM_SEED` entry — **not** in the SE050. The SE05x
cannot derive a key pair internally (`docs/se050.md`), so the root secret has
to be somewhere the firmware can reach it for derivation. The vault key itself
still comes from the chip, so the seed is stored encrypted and only reachable
while the vault is open.

A seed entry is created with `mirrored = false` and `paired_host_only = true`,
so the vault refuses to copy it to a second stick and refuses to hand it to an
unpaired computer. `architecture.md` says a crypto seed belongs in "this device
only", and this is how that is carried out.

Deriving a key always goes through `osk_store_reveal()`, which asks for
approval and writes the access into the device log. The seed is wiped from RAM
in the same function that fetched it.

## Signing happens in the chip

`wallet_signer.c` does three things in this order: ask the chip which curves it
has (`osk_se050_curves`, never assume), hand the derived key to the chip, and
check the answer against the public point before it leaves the device.

If the chip cannot do the curve, the answer is `OSK_ERR_UNSUPPORTED`. **There is
no fallback that signs in the firmware.** A delivered SE051 can come from the
factory without EdDSA and Curve25519; secp256k1 is not affected by that, but it
is still asked for rather than assumed.

Checking the signature costs one point multiplication. It is worth it: a wrong
signature on a payment costs the money twice, once through the error and once
through the second attempt. The check is in the tests too — a deliberately
corrupted answer from the chip is detected (`test_psbt.c`).

### One function is missing from the contract

`osk_se050_key_import_ec()` is declared in `wallet_signer.h` and **belongs in
`apps/firmware/include/osk_se050.h`**. The wallet cannot work without it: the
derived key is born in the firmware, and the chip is supposed to sign with it.
AN12413 4.7.1.8 (`WriteECKey`) accepts a private value handed in from outside,
so the path exists — only the entry in the shared header is missing. This has
been reported to the coordinating session; until it is moved, the declaration
lives in the wallet's own header and the test mock provides it.

## What is deliberately not implemented

| Not implemented | Why | What happens instead |
| --- | --- | --- |
| Taproot (BIP341) | Its signature hash is a separate scheme. Half of it would be more dangerous than none. | A Taproot input is recognised and refused. Taproot *addresses* are shown correctly, so sending to one works. |
| Any sighash flag except `SIGHASH_ALL` | The others leave parts of the transaction exchangeable, which nobody can judge on a small screen. | `OSK_ERR_UNSUPPORTED`. |
| Legacy Bitcoin ledger protocol (`GET_TRUSTED_INPUT`) | It requires the device to cache foreign transactions in its own format and authenticate them with a device key. PSBT replaces all of it. | A vendor APDU (`0xF0`) takes a whole PSBT; Electrum and Sparrow also exchange PSBT as a file or through the clipboard. |
| PSBT with zero inputs | There is nothing to sign, and "zero inputs" is not distinguishable in the bytes from the SegWit marker. | Refused with `OSK_ERR_FORMAT`. Two of BIP174's valid vectors are these, and the tests state the expectation explicitly. |
| Finalising a transaction | The device hands out signatures; the computer assembles. | So the device never holds a finished transaction. |
| Mnemonics and passphrases outside ASCII | BIP39 wants Unicode NFKD for those, and getting it wrong means a different seed. | Refused rather than guessed. |
| Non-English wordlists | English is the only list every wallet understands. | — |
| Token amounts (ERC-20 and friends) | The device does not know a token's decimals, so "1000000" could be anything. | A contract call is not readable; see the rule above. |
| ed25519 public keys and addresses | SLIP-0010 derivation is there, but the public point needs a second curve, and the chip is meant to provide it. | `osk_bip32_neuter()` answers `OSK_ERR_UNSUPPORTED` for ed25519. |

## Limits, and what happens at them

Fixed buffers everywhere, no dynamic memory in the crypto path, every length
checked before it is read, every buffer wiped after use.

| Limit | Value | At the limit |
| --- | --- | --- |
| PSBT inputs / outputs | 8 each | the whole PSBT is **refused**, never signed in part |
| Script length | 128 bytes | refused (`OSK_ERR_FULL`) |
| Derivation path depth | 8 | refused |
| BIP32 derivation entries per PSBT input | 4 | refused |
| Assembled APDU payload | 2048 bytes | refused |
| EIP-191 message | 512 bytes | refused |
| Mnemonic / passphrase | 257 characters | refused |

A half-signed payment is worse than none, so nothing is ever signed in part:
if one input cannot be handled, the whole call fails.

## Files

| File | Contents |
| --- | --- |
| `wallet_hash.c` | SHA-512, HMAC-SHA512, PBKDF2, RIPEMD-160, Keccak-256 |
| `wallet_bn.c` | 256-bit numbers, fixed width |
| `wallet_ec.c` | secp256k1 — public points, signature checking, recovery id |
| `wallet_codec.c` | Base58Check, Bech32, Bech32m, EIP-55, hex |
| `wallet_bip39.c` + `wallet_wordlist.c` | mnemonic ↔ seed, 2048 English words |
| `wallet_bip32.c` | derivation, xprv/xpub |
| `wallet_seed.c` | the seed in the vault, and the path from there to a key |
| `wallet_signer.c` | signing in the SE050, DER ↔ raw, low-s |
| `wallet_confirm.c` | amounts without floating point, and the display rule |
| `wallet_btc.c` | transactions, scripts, addresses, BIP143 |
| `wallet_psbt.c` | BIP174 |
| `wallet_rlp.c` | RLP, strictly canonical |
| `wallet_eth.c` | EIP-155/2930/1559, EIP-191, EIP-712, addresses |
| `wallet_apdu.c` | the APDU layer |

## Which words appear on the screen

Nothing in the wallet contains a sentence for the user. Where a text is needed,
a language key is passed on (`wallet.vertragsaufruf`,
`wallet.eip712-inhalt-unbekannt`, `wallet.blind-unterschrift`, …) and the
display layer looks it up. Only what really comes from the other side — an
address, an amount, a message — is shown verbatim. There is no `lang/` folder
under `apps/firmware` yet; the keys above are the wallet's contribution to it.

## What was measured, and how

Everything below ran on a PC, in WSL (Debian, gcc 14.2.0, ext4), with
`-fsanitize=address,undefined`. Four stages exist — written, compiled, in the
image, executed — and this is stage four for the PC and stage one for the
device.

```
cd apps/firmware/test/wallet
make                      # 1099 checks, 0 errors
make fuzz FUZZ_SECS=20    # both fuzz targets green
```

| Suite | Checks | Against what |
| --- | --- | --- |
| `test_hash` | 26 | FIPS 180-4, RFC 4231, RIPEMD-160 paper, Keccak |
| `test_bn` | 28 | independent calculation |
| `test_ec` | 69 | independent calculation, including `n-1` |
| `test_codec` | 63 | BIP173 / BIP350 vectors |
| `test_bip39` | 115 | **the official BIP39 vectors** (entropy, words, seed with "TREZOR") |
| `test_bip32` | 270 | **the official BIP32 vectors 1–5**, including the invalid-key list |
| `test_btc` | 67 | independent calculation |
| `test_psbt` | 172 | **all 20 invalid and 14 valid PSBTs from BIP174**, plus a full sign-and-verify run |
| `test_rlp` | 61 | the yellow paper's examples, plus the non-canonical cases |
| `test_eth` | 95 | **the EIP-155 example**, EIP-55 cases, independent calculation |
| `test_confirm` | 50 | the display rule itself |
| `test_apdu` | 83 | the command layer, mostly broken commands |

Fuzz figures from one 20-second run each, saturated cache, with
`-fsanitize=address,undefined`:

| Target | Runs | What got through |
| --- | --- | --- |
| `fuzz_psbt` | 1445 | 282 read, 282 displayed, 92 signed |
| `fuzz_rlp` | 4 075 893 | 744 599 read, 549 101 displayable |

The two numbers are far apart for a reason worth writing down: `fuzz_psbt`
really signs, and one signature is roughly a thousand point operations with
this (deliberately plain) arithmetic. A high number would mean the deep path
was never reached. Both targets fail the run if nothing gets through — a fuzz
run that only ever hits the outer wall is not a measurement.

The SE050 mock really computes ECDSA on secp256k1. A mock that only returned
bytes would have switched off the signature check in `wallet_signer.c`, and
that check is the whole point.

## What is not proven

- Nothing has run on the nRF52840 or on a real SE051. Code size, stack use and
  speed on the target are unmeasured.
- The APDU layer has never been spoken to by MetaMask, Electrum or Sparrow.
  That it is byte-compatible is written against the published command shape,
  not observed.
- `osk_se050_key_import_ec()` is answered by the mock. Whether a real SE051
  accepts the import with the intended policy is untested.
- The timing of the arithmetic is written to avoid value-dependent branches,
  but no side-channel measurement has been made.
