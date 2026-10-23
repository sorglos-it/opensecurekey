# Changelog

## [Unreleased]

### Added
- Management program `osk` (Go): own HID layer for Windows, Linux and macOS, own
  CBOR, all 14 protocol commands, local web interface, command line, five
  languages, device simulator. Tested against the simulator only — no real
  device exists yet, and macOS has never been compiled.
- Formatting rule for the firmware (`.clang-format`, Zephyr style)
- Tools: `set_version.py`, `build.py`, `flash.py` (untested, no hardware), `release.py`, `check.py`, CI on Linux and Windows, `docs/build.md`
- Repository skeleton following the house standard
- Architecture, threat model and hardware notes in `docs/`
- Internal interface headers for firmware modules
- Protocol description between firmware and management program
