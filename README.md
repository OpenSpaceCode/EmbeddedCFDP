# EmbeddedCFDP

A minimal, dependency-free embedded C implementation of the **CCSDS File
Delivery Protocol (CFDP)** wire format. Part of the OpenSpaceCode initiative —
reusable, standards-aligned components for small-scale space applications.

## Standards Compliance

- **CCSDS 727.0-B-5**: CCSDS File Delivery Protocol (CFDP) — Blue Book.

This library implements the *basic* protocol layer: serialisation and
deserialisation of the fundamental PDUs. The transaction state machine, timers,
retransmission and filestore are out of scope. See
[`docs/727x0b5e1.pdf`](docs/727x0b5e1.pdf) for the standard itself.

## Features

### Core Protocol Implementation

- **Fixed PDU header** (§5.1) — full flag set, variable-length entity IDs and
  transaction sequence numbers (1–8 octets), 32- and 64-bit (large file) modes.
- **File Data PDU** (§5.3) — segment offset plus file data, including the
  record continuation state and segment metadata when the header's segment
  metadata flag is set.
- **File Directive PDUs** (§5.2) — EOF, Finished, ACK, Metadata, NAK,
  Prompt and Keep Alive. The NAK codec carries up to
  `CFDP_NAK_MAX_SEGMENT_REQUESTS` (32 by default, overridable) segment
  requests, and rejects a PDU needing more rather than dropping the excess.
- **LV and TLV parameters** (§5.1.8, §5.1.9, §5.4) — filestore requests and
  responses, messages to user, fault handler overrides, flow labels and
  entity IDs.
- **Fault Location** (§5.2.2, §5.2.3) — encoded and decoded directly by the EOF
  and Finished codecs, which refuse to emit a fault condition without it.
- **File checksums** (§4.2) — the mandatory modular (type 0) and null (type 15)
  algorithms, streaming and segment-order independent, selectable by the
  checksum type carried in the Metadata PDU.
- **PDU CRC** (§4.1) — the CCSDS TC CRC-16 (CCSDS 232.0-B-3 §4.2.1.3),
  appended by `cfdp_pdu_crc_append()` and checked by `cfdp_pdu_crc_verify()`;
  `cfdp_pdu_payload_size()` excludes the trailer from the payload length so the
  CRC octets are never decoded as file data or as part of a TLV chain.

### Not Implemented

- **Optional checksum types** 1–14 (§4.2.2.5) — reported as unsupported by
  `cfdp_checksum_type_supported()`, so the caller can apply the §4.2.2.8
  fallback.
- **Transaction procedures** (§4.3–§4.12) and **user operations** (§6).

### Design Principles

- **No heap usage** — every buffer is caller-supplied.
- **No external dependencies** — C99, standard library headers only.
- **Round-trip symmetry** — every `_serialize` has a matching `_deserialize`.
- **Big-endian on the wire**, native-endian in the API.

## Project Structure

```
EmbeddedCFDP/
├── include/
│   ├── cfdp.h              # Umbrella header
│   ├── cfdp_common.h       # Enums, constants, shared helpers
│   ├── cfdp_endian.h       # Big-endian integer helpers
│   ├── cfdp_checksum.h     # Modular and null file checksums
│   ├── cfdp_crc.h          # 16-bit PDU CRC
│   ├── cfdp_pdu.h          # Fixed PDU header + File Data PDU
│   ├── cfdp_directive.h    # File Directive PDUs
│   └── cfdp_tlv.h          # LV and TLV parameters
├── src/
│   ├── cfdp_checksum.c
│   ├── cfdp_crc.c
│   ├── cfdp_pdu.c
│   ├── cfdp_directive.c
│   └── cfdp_tlv.c
├── examples/
│   └── example.c           # Build-and-parse a small-file transfer
├── tests/
│   ├── cunit.h             # Minimal test framework
│   ├── test_runners.h      # Per-module test runner declarations
│   ├── test_cfdp_checksum.c
│   ├── test_cfdp_crc.c
│   ├── test_cfdp_pdu.c
│   ├── test_cfdp_directive.c
│   ├── test_cfdp_tlv.c
│   └── unit_tests.c        # Test entry point
├── docs/
│   └── 727x0b5e1.pdf       # CCSDS 727.0-B-5 Blue Book
├── tools/
│   └── coverage-html.sh    # Coverage report
├── build/                  # Build artifacts
├── Makefile
└── README.md
```

## Building

### Build Everything

```bash
make
```

Builds the static library, the example binary and the test binary in `build/`.

The C standard, include paths and warning set are fixed in the `Makefile` and apply to the
library, example and tests alike. Only the optimisation/instrumentation flags are meant to be
overridden, via `OPT`:

```bash
make OPT="-O0 -g"
```

### Build Library Only

```bash
make lib
# Produces: build/lib/libcfdp.a
```

### Build and Run the Example

```bash
make example
./build/examples/example
```

### Run Tests

```bash
make run
```

### Coverage (HTML)

Requires `gcovr`:

```bash
pip install gcovr
make coverage-html
# Prints a line/branch summary
# Output: build/coverage/index.html
```

The script rebuilds with `OPT="-O0 -g --coverage"`, so instrumentation is the only difference
from a normal build.

### Clean

```bash
make clean
```

## Quick Start

### Step 1 — Serialise a header and an EOF PDU, with a CRC

```c
#include "cfdp.h"

uint8_t buf[64];

cfdp_pdu_header_t hdr = {0};
hdr.version = CFDP_PROTOCOL_VERSION;
hdr.pdu_type = CFDP_PDU_TYPE_DIRECTIVE;
hdr.direction = CFDP_DIRECTION_TOWARD_RECEIVER;
hdr.transmission_mode = CFDP_TRANS_MODE_UNACKNOWLEDGED;
hdr.crc_flag = CFDP_CRC_PRESENT;
hdr.large_file_flag = CFDP_FILE_SIZE_SMALL;
hdr.entity_id_length = 1;
hdr.transaction_seq_length = 2;
hdr.source_entity_id = 1;
hdr.transaction_seq_number = 42;
hdr.destination_entity_id = 2;

cfdp_eof_pdu_t eof = {0};
eof.condition_code = CFDP_COND_NO_ERROR;
eof.file_checksum = cfdp_checksum_compute(file, file_len);
eof.file_size = file_len;

size_t hlen = cfdp_pdu_header_size(&hdr);
size_t plen = cfdp_eof_serialize(&eof, hdr.large_file_flag, buf + hlen, sizeof(buf) - hlen);
hdr.data_field_length = (uint16_t)(plen + CFDP_PDU_CRC_LEN); /* the CRC counts (§4.1.3.2) */
cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf));
size_t total = cfdp_pdu_crc_append(buf, hlen + plen, sizeof(buf)); /* bytes to transmit */
```

Leave `crc_flag` clear, size `data_field_length` for the payload alone and skip
`cfdp_pdu_crc_append()` to send without a CRC.

### Step 2 — Parse a received PDU

```c
if (!cfdp_pdu_crc_verify(rx, rx_len)) {
    /* §4.1.2: discard. Passes any PDU whose CRC flag is clear. */
    return;
}

cfdp_pdu_header_t hdr;
size_t hlen = cfdp_pdu_header_deserialize(rx, rx_len, &hdr);

const uint8_t *payload = rx + hlen;

/* The data field length counts the CRC when the CRC flag is set (§4.1.3.2),
 * so never pass hdr.data_field_length straight to a payload codec. */
size_t payload_len = cfdp_pdu_payload_size(&hdr);

if (hdr.pdu_type == CFDP_PDU_TYPE_DIRECTIVE) {
    cfdp_directive_code_t code;
    cfdp_pdu_directive_code(payload, payload_len, &code);
    /* dispatch on `code` (CFDP_DIRECTIVE_EOF, ...) */
} else {
    cfdp_file_data_pdu_t fd;
    cfdp_file_data_deserialize(payload, payload_len, hdr.large_file_flag,
                               hdr.segment_metadata_flag, &fd);
    /* write fd.file_data_len octets at fd.offset */
}
```

## API Reference

### PDU header (`cfdp_pdu.h`)

```c
size_t cfdp_pdu_header_size(const cfdp_pdu_header_t *hdr);
size_t cfdp_pdu_header_serialize(const cfdp_pdu_header_t *hdr, uint8_t *buf, size_t buf_len);
size_t cfdp_pdu_header_deserialize(const uint8_t *buf, size_t buf_len, cfdp_pdu_header_t *hdr);
size_t cfdp_pdu_payload_size(const cfdp_pdu_header_t *hdr);
size_t cfdp_pdu_crc_append(uint8_t *buf, size_t pdu_len, size_t buf_len);
bool cfdp_pdu_crc_verify(const uint8_t *buf, size_t buf_len);
```

`cfdp_pdu_crc_append()` expects the header to have been serialised with the CRC
flag set and `data_field_length` already counting the two CRC octets, and
refuses otherwise — forgetting the `+ CFDP_PDU_CRC_LEN` is the mistake it
exists to catch. `cfdp_pdu_crc_verify()` implements §4.1.2 as a single
accept/discard decision and ignores octets beyond the PDU the header describes,
so a padded link frame can be handed to it directly.

### File Data (`cfdp_pdu.h`)

```c
size_t cfdp_file_data_serialize(const cfdp_file_data_pdu_t *fd, cfdp_large_file_flag_t large,
                                cfdp_seg_metadata_flag_t seg_meta, uint8_t *buf, size_t buf_len);
size_t cfdp_file_data_deserialize(const uint8_t *buf, size_t buf_len, cfdp_large_file_flag_t large,
                                  cfdp_seg_metadata_flag_t seg_meta, cfdp_file_data_pdu_t *fd);
```

Both take the header's large-file and segment-metadata flags, which select the
data field layout (§5.3). Pass the same values carried in the header actually
sent or received: the serialiser rejects a payload whose segment metadata
disagrees with the flag, since that mismatch would silently shift the offset
field at the peer.

### Directives (`cfdp_directive.h`)

`cfdp_{eof,finished,ack,metadata,nak,prompt,keep_alive}_{serialize,deserialize}()`
— each returns the number of octets written or consumed, or `0` on error.

Pass `cfdp_pdu_payload_size(&hdr)` as the data field length when decoding, not
`hdr.data_field_length`, so a trailing CRC is not parsed as PDU content.

The ACK codec accepts only EOF and Finished as the acknowledged directive, the
only two table 5-8 allows, and derives the directive subtype code from it —
`0001` for Finished, `0000` for EOF — so `cfdp_ack_pdu_t` has no subtype field
to set wrongly. A received ACK of any other directive, or with a subtype that
disagrees with its directive code, is rejected.

`cfdp_nak_deserialize()` rejects a data field whose segment requests do not fill
it exactly, and one carrying more requests than `cfdp_nak_pdu_t` can hold —
truncating would tell the sender that gaps it never saw had been satisfied, so
they would never be retransmitted. Raise `CFDP_NAK_MAX_SEGMENT_REQUESTS` (it
sizes the struct: 32 requests is 536 octets on a 64-bit build) on links where the receiver
routinely reports more gaps than the default.

### LV/TLV parameters (`cfdp_tlv.h`)

```c
size_t cfdp_lv_serialize(const char *value, uint8_t value_len, uint8_t *buf, size_t buf_len);
size_t cfdp_tlv_serialize(const cfdp_tlv_t *tlv, uint8_t *buf, size_t buf_len);
size_t cfdp_entity_id_tlv_serialize(uint64_t entity_id, uint8_t id_len,
                                    uint8_t *buf, size_t buf_len);
size_t cfdp_fault_handler_tlv_serialize(cfdp_condition_code_t condition_code,
                                        cfdp_fault_handler_code_t handler_code,
                                        uint8_t *buf, size_t buf_len);
size_t cfdp_filestore_request_tlv_serialize(const cfdp_filestore_request_t *req,
                                            uint8_t *buf, size_t buf_len);
size_t cfdp_filestore_response_tlv_serialize(const cfdp_filestore_response_t *resp,
                                             uint8_t *buf, size_t buf_len);
```

Each has a matching `_deserialize()`. Metadata options and Finished filestore
responses are carried as pre-encoded TLV chains: build one with these codecs,
point the PDU struct at it, and walk a received chain by advancing through
`cfdp_tlv_deserialize()` by its return value.

```c
/* Attach a filestore request to a Metadata PDU. */
uint8_t options[64];
cfdp_filestore_request_t req = {0};
req.action_code = CFDP_FS_ACTION_CREATE_DIRECTORY;
req.first_filename = "logs";
req.first_filename_len = 4;

md.options = options;
md.options_len = (uint16_t)cfdp_filestore_request_tlv_serialize(&req, options, sizeof(options));
```

### Checksum (`cfdp_checksum.h`)

```c
uint32_t cfdp_checksum_update(uint32_t checksum, uint64_t offset, const uint8_t *data, size_t len);
uint32_t cfdp_checksum_compute(const uint8_t *data, size_t len);
bool cfdp_checksum_type_supported(cfdp_checksum_type_t type);
bool cfdp_checksum_update_by_type(cfdp_checksum_type_t type, uint32_t *checksum,
                                  uint64_t offset, const uint8_t *data, size_t len);
```

`cfdp_checksum_update/compute` implement the modular checksum directly.
`cfdp_checksum_update_by_type` applies whichever algorithm a Metadata PDU
names: the modular or the null checksum, or `false` for a type this library does
not implement. §4.2.2.8 then calls for an Unsupported Checksum Type fault, with
the sender falling back to the modular checksum and the receiver to the null
checksum; that choice depends on the entity's role and is left to the caller.

### CRC (`cfdp_crc.h`)

```c
uint16_t cfdp_crc_update(uint16_t crc, const uint8_t *data, size_t len);
uint16_t cfdp_crc_compute(const uint8_t *data, size_t len);
```

The raw CCSDS TC CRC-16 (polynomial `0x1021`, preset `0xFFFF`, no final
inversion; `"123456789"` → `0x29B1`), for callers that assemble PDUs in pieces.
The PDU-level helpers above are the normal entry points.

### Return-value convention

Every `_serialize` / `_deserialize` function returns the number of octets
written or consumed, and `0` on any error (NULL argument, buffer too small, or
malformed input).

## Memory Usage (Estimated)

- **Library (stripped)**: a few kilobytes of `.text`; no static state.
- **No heap usage**: all allocations are caller-supplied.
- **Serialization buffers**: caller-sized; a full PDU header is at most
  `CFDP_PDU_HEADER_MAX_LEN` (28) octets.

## Limitations

- Optional TLV parameters (fault location, filestore requests/responses,
  messages to user) are not encoded or decoded.
- No transaction state machine, timers or retransmission logic.

## References

- CCSDS 727.0-B-5, *CCSDS File Delivery Protocol (CFDP)*, Blue Book.
- CCSDS 720.1-G-4, *CFDP — Part 1: Introduction and Overview*, Green Book.

## License

See [LICENSE](LICENSE) file.
