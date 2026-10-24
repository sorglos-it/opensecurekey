# How other programs reach the key

`docs/integration.md` lists the routes. This page says what is **built**, how
to use it, and — just as important — what was **measured** and what was not.

**No real device was involved in any measurement on this page.** Everything
ran against the dummy in `apps/desktop/tests/simulator`. The hardware is on
order. macOS is **not tested at all**: the code builds for it, nothing more.

Measured on 24.10.2026, Windows 11 (Go 1.27, clang 23 from the ESP toolchain)
and Debian 13 in WSL2 (Go 1.27, gcc 14, OpenSSH 10.0, OpenSSL 3.5, Chromium 156).

| Route | State | Proof |
| --- | --- | --- |
| ssh agent | built | `git push` through it, Windows and Linux |
| command line for scripts | built | `osk keys`, `env`, `run`, `put`, `log`, `codes` |
| PKCS#11 library | built | `openssl` verifies a signature; `ssh -I` does a `git push` |
| browser extension | built | a login field filled in a real Chromium |
| local interface | built | a foreign program fetches a secret after approval |
| device types as a keyboard | not built | firmware, not this program |
| smart card minidriver, CryptoTokenKit | not built | needs admin rights / is OS integration |
| FIDO-SSH (`sk-ecdsa`) | not built | firmware |
| fills the OS keychain | not built | — |

Every route shares three rules, and they are not decoration:

- **Every access tells the device the name of the asking program.** Where the
  operating system knows the name it is used (`SO_PEERCRED`,
  `GetNamedPipeClientProcessId`); otherwise the route's own name is used. It
  stays a **name**, never proof, and every interface says so.
- **No private key is ever kept.** For each signature the secret is fetched
  again, used, and overwritten. A crash dump holds nothing.
- **The device decides.** A route that is switched off gets `OSK_ERR_DENIED`,
  not a silent success.

---

## 1. ssh agent

`ssh`, `git`, `scp` and Ansible already speak to an agent. Ours answers two
questions — "which keys do you have" and "sign this" — and refuses everything
that would add or remove keys: the vault is the device, not this process.

```bash
osk agent                 # listens, prints the line you need
osk agent --print         # only says where it would listen
```

| System | Place | How ssh finds it |
| --- | --- | --- |
| Linux, macOS | `$XDG_RUNTIME_DIR/osk/agent.sock` | `export SSH_AUTH_SOCK=…` |
| Windows | `\\.\pipe\osk-ssh-agent` | `$env:SSH_AUTH_SOCK = '…'` |

`OSK_AGENT` overrides the place.

**Keys** are vault items of type `sshkey`, holding the private key file as it
comes out of `ssh-keygen`. `openssh-key-v1` and the older PEM forms are read;
ed25519, ecdsa p256/384/521 and rsa (2048 bits and up) are signed with. A key
**encrypted with a password** is refused with that reason — put it in the
vault decrypted, the device is the lock.

```bash
osk put --type sshkey --name github --site github.com < ~/.ssh/id_ed25519
osk keys                  # the authorized_keys lines
```

**One button press too many, and why — now avoidable.** `ssh` wants the list
of identities *before* anything is signed. When this was written, `list` did
not carry the public part of a key, so the agent fetches every ssh key once at
the start, computes the public part and overwrites the private one immediately
— one press per key, only to learn its name. `shared/protocol.md` has carried
a `pub` field on `list` since the strand reported this; the agent does not use
it yet. See the loose ends at the bottom.

### What was measured

- `git push` over the agent, **Windows**: our named pipe,
  `ssh.exe` (OpenSSH_for_Windows_9.5p2), branch `main` landed in a bare repo.
- `git push` over the agent, **Linux**: our socket, OpenSSH 10.0, branches
  `linux` and `linux2` landed.
- `ssh-add -l` lists the key through the agent on both systems.
- The public part we compute equals, character for character, the line
  `ssh-keygen` prints — for all four key kinds. The comparison lines come
  from the real `ssh-keygen` and are checked in as test vectors.
- 9 mutants, all as expected (8 dead, 1 control alive).

---

## 2. Command line for scripts

`osk get <name>` existed. These are new:

| Command | What it does |
| --- | --- |
| `osk keys [name]` | public SSH keys, as `authorized_keys` wants them |
| `osk put --type … --name … [--site …]` | new entry, secret from standard input |
| `osk env NAME=entry …` | assignments for a shell, `.env` or JSON |
| `osk run NAME=entry … -- program …` | start a program with the secret in its environment |
| `osk log [count]` | the device log: who fetched what, and when |
| `osk codes` | the exit codes |

```bash
osk env GITHUB_TOKEN=github                    # OSK_… in sh syntax
osk env --format ps --prefix APP_ datenbank    # PowerShell
osk env --format dotenv datenbank > .env       # careful: this is a file
osk run GITHUB_TOKEN=github -- gh pr list      # nothing on disk at all
```

**`osk run` is the safe one.** A secret never belongs in a command line — the
process list shows it to everyone. With `run` it goes into the environment of
the one child process and nowhere else. What that program then does with it
is up to the program: once a value is in a foreign process's environment,
nobody can overwrite it any more. That is the limit of this route, and it is
the reason `osk env` exists only for cases where a file really is wanted.

**Exit codes**, so a script can tell *why* something failed:

| Code | Meaning |
| --- | --- |
| 0 | all good |
| 1 | error, not distinguished further |
| 2 | wrong usage |
| 3 | no device found or no connection |
| 4 | the device is locked — run `osk unlock` |
| 5 | approval missing, or the route is switched off |
| 6 | there is no such entry |
| 7 | the name matches several entries |

---

## 3. PKCS#11 library

The biggest lever: Firefox, Thunderbird, OpenSSL, Java, `ssh -I` and VPN
clients all already speak this interface.

```bash
go build -buildmode=c-shared -o osk-pkcs11.dll  ./classes/pkcs11   # Windows
go build -buildmode=c-shared -o osk-pkcs11.so   ./classes/pkcs11   # Linux
go build -buildmode=c-shared -o osk-pkcs11.dylib ./classes/pkcs11  # macOS
```

A C compiler is needed — PKCS#11 *is* a C interface. Without one the package
still builds (as an empty program), so `tools/check.py` does not depend on a
tool that only this one building block needs.

**Settings come from `OSK_CONFIG`** (full path to `config_osk.json`). Without
it the defaults apply: a real device, no dummy. That is deliberate — a
library sitting inside Firefox should not go looking for a dummy on the
network. "Next to the program" would be the Firefox folder and therefore
meaningless.

### What it offers, and what it refuses

One slot, one token. Per ssh key in the vault a private and a public object,
per readable certificate a certificate object. Mechanisms: `CKM_RSA_PKCS`,
`CKM_SHA1/256/384/512_RSA_PKCS`, `CKM_ECDSA`, `CKM_ECDSA_SHA256/384/512`,
`CKM_EDDSA`.

Every writing and every decrypting function is `NULL` in the function table:
`C_CreateObject`, `C_DestroyObject`, `C_SetAttributeValue`, `C_GenerateKey`,
`C_Decrypt`, `C_UnwrapKey`. There is no path on which foreign software could
write to the device or get a secret out of it. And the library *says* so
before anything is tried: `CKA_SENSITIVE` true, `CKA_EXTRACTABLE` false,
`CKA_ALWAYS_AUTHENTICATE` true, and `CKA_VALUE` on a private key is refused
as **sensitive**, not as "does not exist" — PKCS#11 has its own return value
for each, and software tells them apart.

### One deliberate deviation from the specification

A write-protected token may refuse a read-write session. The first version
did exactly that — and locked `ssh` out: OpenSSH asks for `CKF_RW_SESSION`
every time, with no fallback, and stopped with `C_OpenSession failed: 181`.
The flag is now accepted. It costs nothing, because there is no writing
function to reach. The reasoning is in the code at `Pkcs11Token.OpenSession`.

### What was measured

- Our own probe (`classes/pkcs11_probe/probe.c`, loads the library the way
  Firefox does) runs the whole way: `C_GetFunctionList`, `C_Initialize`,
  `C_GetTokenInfo`, `C_Login`, find objects, read attributes, sign — on
  **Windows and Linux**. Before login **no object is visible**; `CKA_VALUE`
  on a private key is refused.
- **`openssl` confirms the signature**: `Verified OK`. With one byte of the
  data changed: `Verification failure`.
- **`ssh -I ./osk-pkcs11.so` did a `git push`** on Linux — OpenSSH loaded the
  library itself, read both public keys out of it and signed the login with
  it. No agent, no key file. The commit is in the bare repo.
- 15 mutants, all as expected (14 dead, 1 control alive).

```bash
# the probe, both systems
clang -O1 -Wall -o probe classes/pkcs11_probe/probe.c            # Windows
gcc   -O1 -Wall -o probe classes/pkcs11_probe/probe.c -ldl       # Linux
OSK_CONFIG=…/config_osk.json ./probe ./osk-pkcs11.so . 123456
openssl dgst -sha256 -verify pub.pem -signature unterschrift.bin daten.bin
```

**Two traps, both paid for:** on Windows these structures are packed byte by
byte (the specification says so), and cgo cannot map a packed structure to
fields — every field access goes through C (`classes/pkcs11/pkcs11_fields.c`).
And `CK_ULONG` is `unsigned long`: **four** bytes on Windows, **eight** on
Linux. Every number crossing that border is converted, never passed through.

---

## 4. Browser extension

Lives in `apps/web/`. It fills login fields — **only on the address stored in
the entry.** That is not a convenience feature, it is the whole point: an
extension that fills everywhere is a phishing aid. It would enter the
password for `bank.example.org` on `bank.example.org.attacker.tld` too, and
the user would only see that it "worked".

### The rule, and what follows from it

**The host name of the entry and the host name of the page must be equal,
character for character.** No exceptions:

- No substring. `example.org` does not match `notexample.org`.
- **No subdomains.** An entry for `example.org` does **not** fill on
  `login.example.org`, and the other way round. Whoever needs both makes two
  entries. That is inconvenient and right: allowing subdomains means knowing
  where a domain ends, and that is exactly where such rules break.
- No wildcards. `*.example.org` is not a host name and fills nowhere.
- No `http`. The only exception is your own machine, where nobody can listen.

### Two barriers, because the first one can fall

1. **In the extension**: the address comes from the **address bar**
   (`chrome.tabs`), never from the page's content. A page cannot influence it.
2. **In `osk`** (`classes/native_origin.go`): the same check again. This one
   holds even if the extension is taken over and claims any address it likes.

A third, small one sits in the injected function: right before filling it
compares the address once more, from the document itself. Should the tab have
moved on between reading the address and filling, the secret would otherwise
land on the new page. That window is small — phishing lives on small windows.

### How the browser reaches osk

Through **Native Messaging**, not through a port. A port on 127.0.0.1 is
reachable from every page in the same browser; this child process is started
only by the browser, and only for the extensions named in its registration
file.

```bash
osk native --register chrome  <extension-id> <folder>
osk native --register firefox <extension-id> <folder>
```

That writes two files — the registration file and a tiny start script (the
browser cannot pass a subcommand) — and says where the browser looks for
them. It does **not** write into the browser's folders itself: that is
another program's configuration, and this program does not take that decision
away from anyone. No step needs admin rights.

### What the extension may do

`nativeMessaging`, `activeTab`, `scripting` — and no host permissions. It may
touch a tab only **after** the user has clicked its icon. The fill script is
injected at that moment, into that one tab, and is not resident anywhere.

Five languages in `apps/web/lang/`. One deviation from the house rule, forced
by the platform: an extension cannot list its own folder, so the available
languages are named in `assets/langs.json` — a **data** file. A new language
is still one file plus one line, no code change.

### What was measured

In a real **Chromium 156**, with the real extension, real Native Messaging to
the real `osk`, against the dummy:

- The **password field was filled** (23 characters, the value from the
  device), the user name field with the entry's name, and the search field in
  front of it left alone.
- An entry belonging to `bank.example.org` was **refused** on `127.0.0.1`
  with that reason, and nothing was filled.
- The list the popup would show contained only the entry for that address.
- 13 checks of the rules with `node --test`, and 18 mutants over the
  phishing protection and the local interface, all as expected.

**What this did not show**, and the reason: the run drove a **copy** of the
extension whose manifest additionally carries a permission for the one test
page. A Chromium without a window cannot produce the click on the toolbar
icon, and only that click grants `activeTab` — measured:
`chrome.action.openPopup()` with a user gesture opens the window but grants
nothing, `tabs.get` keeps returning `undefined` and `executeScript` says
*Cannot access contents of the page*. The shipped extension stays on
`activeTab`; what is unproven is that the click grants the right, and that is
the browser's job, not ours.

**Also worth knowing:** the vault format has no separate field for a user
name (`shared/vault-format.md`: name, address, one secret). The extension
therefore offers the **entry's name** as the user name and shows it
beforehand. An entry needing a different user name than its own name needs a
new field in the format; reported to the protocol strand.

---

## 5. Local interface

For your own programs.

```bash
osk socket                  # listens, prints the place
osk socket --print
```

| System | Place |
| --- | --- |
| Linux, macOS | `$XDG_RUNTIME_DIR/osk/local.sock` |
| Windows | `\\.\pipe\osk-local` |

`OSK_SOCKET` overrides it. Deliberately a **different** place from the ssh
agent: whoever may reach `ssh` should not thereby reach the whole vault.

One JSON map per line in, one per line back — the simplest format there is,
so anyone can connect a program in any language in ten lines, without a
library from us.

```json
{"was":"hello"}
{"was":"list"}
{"was":"reveal","item":4}
{"was":"lock"}
```

**Every access is approved on its own.** On each `reveal` the program stops
and asks in its own window: *which program* wants *which secret*. Only after
an explicit yes does the question go to the device, where the user presses
the button a second time. There is no "remember" and no "always allow" —
those two buttons are what separates a local interface from an open vault.
Anything that is not a clear yes is a no: an empty line, a closed input, a
timeout.

### What was measured

On **Windows and Linux**, with `python` as the foreign program:

- With `j` the secret arrives (identical on both systems, SHA256
  `9e86cb79…`), the device log shows `lokal: python3.13` and
  `lokal: python.exe` — names from the operating system.
- With `n` nothing arrives, and the device never sees the request.
- An item number that does not exist triggers no question at all — otherwise
  the question itself would be a way to probe the vault.

An example client is in the strand's notes; it is 60 lines of Python with no
dependencies.

---

## What is deliberately not built

Windows smart card minidriver, CryptoTokenKit on macOS, disk unlock and PAM
login. All of them are operating system integration and need admin rights to
install — `docs/integration.md` says so, and that is exactly what makes the
other five routes useful on a strange computer.

## Measured: how many approvals an operation costs

A public key is not a secret. Fetching one through `reveal` cost a button press
per key — and a user who learns to dismiss button presses without looking will
dismiss the one that matters too. That makes the real question at signing time
worthless, and that question is the point of the whole device.

Since the device sends the public part with `list`, the agent takes it from
there. Measured, not assumed (`tests/freigaben_test.go`, three keys):

| Operation | Approvals |
| --- | --- |
| Listing identities | **0** |
| Signing once | **1** |

Both halves matter. Without the second row the test would also pass if nothing
asked for approval at all.

The counter-check: with the agent forced back to the old path, the same test
reports "listing cost 3 approvals, expected 0" and three reveal fallbacks. So
the test measures something.

One existing test had to change with it. It required "at least 2 accesses",
meaning one for listing and one for signing, and its comment named the real
worry: the agent must not keep a private key. Lowering the number to 1 would
have stopped measuring that worry, so the test now checks the property itself —
**every** signature costs exactly one access. If the agent kept the key, the
second signature would be free.

**Still open: PKCS#11 does the same thing.** `Pkcs11Token.loadObjects` still
reveals every key, because it needs the public parameters (curve point, modulus,
exponent) and currently reads them out of the private key. Using `pub` there
needs a parser for the SSH public-key format — an addition, not a rewrite, and
not something to hurry through in a crypto parser. The agent was the larger win:
`ssh` lists identities on every connection, while PKCS#11 loads its objects once
per session.

## Loose ends

- **macOS is untested.** The code builds (cross-compiled); nothing was run.
- **No real device.** Every number on this page comes from the dummy.
- **`list` now carries `pub`, and two routes do not use it yet.** The protocol
  gained the field after this code was written (`shared/protocol.md`, command
  4). The ssh agent and the PKCS#11 library still fetch every key once to
  compute its public part — one button press per key that is no longer
  necessary. Taking it out is a small change in `Agent.Identities` and
  `Pkcs11Token.loadObjects`, and it needs the dummy to answer `pub` first.
- The vault format has no field for a user name, so the extension offers the
  entry's **name** as the user name.

Two earlier entries here are **done**: the contradiction over name and address
lengths (`items.go` now says 64/128, same as the device) and the status line in
`docs/integration.md`. The icon script is now `tools/logo.py`; it reproduces
the checked-in `apps/web/assets/icon-128.png` byte for byte.
