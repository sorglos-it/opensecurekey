# Security

## Reporting a problem

Please report security problems privately first: open a GitHub security advisory
on this repository. Do not open a public issue for an unfixed problem.

Expect a first answer within a few days. Once a fix exists, the problem is
described openly in `CHANGELOG.md` and in the release notes.

## Scope

In scope: firmware, management program, browser extension, the protocol between
them, and the hardware design in `hardware/`.

Out of scope: the SE050 secure element itself (an NXP product), the nRF Connect
SDK, and attacks that need a modified firmware to be loaded first — loading
firmware requires a signature, and that signing key is not part of this repository.

## What this project does not promise

- The device has not been certified by the FIDO Alliance.
- No security audit has been done yet.
- Until a release exists, nothing here has run on real hardware.
