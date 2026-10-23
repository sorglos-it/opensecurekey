# Threat model

Status: design. No audit has been done.

## What the device protects

| Attacker | What they get |
| --- | --- |
| Finds the device | Nothing without PIN or fingerprint. Counter sits in the SE050, so it cannot be reset by wiping the flash. |
| Opens the device, probes the board | The I2C line between the two chips is encrypted (SCP03). The flash is encrypted. The SE050 resists power analysis and fault injection (CC EAL6+). |
| Owns the computer (malware) | Each release needs a button press, and the display names the service. Malware cannot press the button. |
| Phishing site | Passkeys are bound to the domain; the extension only fills on the stored address. |
| Sits between phone and device (NFC relay) | Still needs the button press on the device. |
| Copies the server | The node key stays in the SE050, so the copy has no identity. |

## What it does not protect against

- **Modified firmware.** The SE050 guards against chip attacks, not against a
  firmware that lies to the user. The signed bootloader is the only defence, and
  it must be in place before read protection is switched on.
- **A keylogger seeing the PIN.** The PIN is typed on the computer. That is why
  the fingerprint is the everyday path and the PIN is only the fallback.
- **Losing both devices and the master passphrase.** Nothing can be recovered.
  The setup wizard therefore forces a second device or a printed emergency sheet.
- **Someone forcing the user to press the button.** No device solves this.

## Decisions that trade security for use

| Decision | Cost | Why |
| --- | --- | --- |
| Passkeys can be mirrored to a second device | keys are no longer device-bound | without it a lost device means lost accounts |
| Server mode without a button press per signature | malware on that server can sign | a server cannot wait for a human; limited to named keys and logged |
| Read-only drive with the programs on it | more USB interfaces to get right | a key nobody can use is not secure either |

Each of these is off by default and has to be switched on knowingly.
