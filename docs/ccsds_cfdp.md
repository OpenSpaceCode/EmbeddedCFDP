# CCSDS File Delivery Protocol (CFDP) — Implementation Notes

This document accompanies the EmbeddedCFDP library and summarises the parts of
**CCSDS 727.0-B-5, *CCSDS File Delivery Protocol (CFDP)*** that the library
encodes and decodes. It is a working reference for maintainers, not a
replacement for the standard.

## Scope of the implementation

CFDP moves files (and small "messages to user") between two entities. A
transfer, called a *transaction*, is carried by a sequence of Protocol Data
Units (PDUs). This library implements the **on-the-wire codec** for the basic
PDU set — the layer every CFDP entity needs regardless of its transaction
state machine, timers or filestore:

| Element | Standard reference | Module |
|---------|--------------------|--------|
| Fixed PDU header | §5.1 | `cfdp_pdu` |
| File Data PDU | §5.3 | `cfdp_pdu` |
| EOF / Finished / ACK / Metadata / NAK directives | §5.2 | `cfdp_directive` |
| Prompt / Keep Alive directives | §5.4 | `cfdp_directive` |
| Modular file checksum | §4.2.2 | `cfdp_checksum` |

The transaction engine, timers, retransmission policy, filestore actions and
the optional Type-Length-Value (TLV) parameters (fault location, filestore
requests/responses, messages to user) are intentionally **out of scope** for
this basic implementation.

## Fixed PDU header

All multi-octet fields are big-endian. The header is four fixed octets
followed by three variable-length identifier fields.

```
 Octet 0:  | Version (3) | PDU Type (1) | Direction (1) | Trans. Mode (1) | CRC (1) | Large File (1) |
 Octet 1-2:| PDU data field length (16)                                                              |
 Octet 3:  | Seg. Ctrl (1) | Len of Entity ID - 1 (3) | Seg. Metadata (1) | Len of Txn Seq - 1 (3)   |
 Octet 4+: | Source Entity ID | Transaction Sequence Number | Destination Entity ID                 |
```

- **Version** is `0b001` in CCSDS 727.0-B-5 (`CFDP_PROTOCOL_VERSION`).
- **PDU Type** selects a File Directive (`0`) or File Data (`1`) payload.
- **Large File** flag widens file offsets and sizes from 32 to 64 bits.
- The two length fields store *(count − 1)*; the API exposes the actual octet
  count (1..8).

## File Directive PDUs

Every directive payload starts with a one-octet directive code:

| Directive | Code |
|-----------|------|
| EOF | `0x04` |
| Finished | `0x05` |
| ACK | `0x06` |
| Metadata | `0x07` |
| NAK | `0x08` |
| Prompt | `0x09` |
| Keep Alive | `0x0C` |

### Condition codes (§5.5)

`0x0` No error · `0x1` Positive ACK limit reached · `0x2` Keep Alive limit
reached · `0x3` Invalid transmission mode · `0x4` Filestore rejection ·
`0x5` File checksum failure · `0x6` File size error · `0x7` NAK limit reached ·
`0x8` Inactivity detected · `0x9` Invalid file structure · `0xA` Check limit
reached · `0xB` Unsupported checksum type · `0xE` Suspend request received ·
`0xF` Cancel request received.

## Modular checksum (§4.2.2)

The legacy CFDP checksum is the sum, modulo 2³², of the 4-octet words formed by
the file contents aligned to their absolute file offset. Each octet at file
offset `o` contributes to byte lane `o mod 4` of the accumulator, so the
checksum is independent of how the file is split into segments and of the order
in which segments arrive. `cfdp_checksum_update()` exploits this to fold one
File Data segment at a time into a running value.

## A minimal transfer

A one-segment unacknowledged (Class 1) transfer is three PDUs:

1. **Metadata** — file size, checksum type, source and destination file names.
2. **File Data** — the file offset followed by the file octets.
3. **EOF** — the final condition code, the file checksum and the file size.

See `examples/example.c` for a runnable build-and-parse walk-through.

## References

- CCSDS 727.0-B-5, *CCSDS File Delivery Protocol (CFDP)*, Blue Book.
- CCSDS 720.1-G-4, *CFDP — Part 1: Introduction and Overview*, Green Book.
