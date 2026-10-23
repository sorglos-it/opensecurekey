# Vault format

Status: design. Not implemented.

The vault lives in the external flash. Everything in it is encrypted; the key is
derived by the SE050 for each session and wiped from RAM afterwards.

## Layout

```
[header]  version, salt, counter   - plain, needed to open the rest
[index]   one record per item      - encrypted, holds metadata only
[blobs]   the secrets              - encrypted, each with its own nonce
[log]     ring buffer              - encrypted
```

## Record

| Field | Meaning |
| --- | --- |
| `id` | stable number, never reused |
| `type` | see `osk_item_type_t` in `apps/firmware/include/osk_store.h` |
| `profile` | which profile owns it |
| `name`, `site` | shown without revealing the secret |
| `flags` | mirrored to second device, paired hosts only, read-only |
| `changed` | UTC, for sync and for "too old" warnings |

## Encryption

- AES-256-GCM per blob, nonce = counter from the header, never repeated.
- The header counter only ever grows; it is written before the blob, so a power
  cut can waste a nonce but never repeat one.
- Writes are atomic: new blob first, then the index entry, then the old blob is
  released. A power cut leaves the old state intact.

## Migration

The header carries a format version. After a firmware update the device migrates
on first unlock, and the old content stays readable until the new one is written.
