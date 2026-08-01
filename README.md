# EmbeddedCFDP

A minimal, dependency-free embedded C implementation of the **CCSDS File
Delivery Protocol (CFDP)** wire format. Part of the OpenSpaceCode initiative —
reusable, standards-aligned components for small-scale space applications.

## Standards Compliance

- **CCSDS 727.0-B-5**: CCSDS File Delivery Protocol (CFDP) — Blue Book.

This library implements the *basic* protocol layer: serialisation and
deserialisation of the fundamental PDUs. The transaction state machine, timers,
retransmission and filestore are out of scope. See
[`docs/ccsds_cfdp.md`](docs/ccsds_cfdp.md) for implementation notes.

## Features

### Core Protocol Implementation

- **Fixed PDU header** (§5.1) — full flag set, variable-length entity IDs and
  transaction sequence numbers (1–8 octets), 32- and 64-bit (large file) modes.
- **File Data PDU** (§5.3) — segment offset plus file data.
- **File Directive PDUs** (§5.2, §5.4) — EOF, Finished, ACK, Metadata, NAK,
  Prompt and Keep Alive.
- **Modular file checksum** (§4.2.2) — streaming, segment-order independent.

### Design Principles

- **No heap usage** — every buffer is caller-supplied.
- **No external dependencies** — C11, standard library headers only.
- **Round-trip symmetry** — every `_serialize` has a matching `_deserialize`.
- **Big-endian on the wire**, native-endian in the API.

## Project Structure

```
EmbeddedCFDP/
├── include/
│   ├── cfdp.h              # Umbrella header
│   ├── cfdp_common.h       # Enums, constants, shared helpers
│   ├── cfdp_endian.h       # Big-endian integer helpers
│   ├── cfdp_checksum.h     # Modular file checksum
│   ├── cfdp_pdu.h          # Fixed PDU header + File Data PDU
│   └── cfdp_directive.h    # File Directive PDUs
├── src/
│   ├── cfdp_checksum.c
│   ├── cfdp_pdu.c
│   └── cfdp_directive.c
├── examples/
│   └── example.c           # Build-and-parse a small-file transfer
├── tests/
│   ├── cunit.h             # Minimal test framework
│   ├── test_runners.h      # Per-module test runner declarations
│   ├── test_cfdp_checksum.c
│   ├── test_cfdp_pdu.c
│   ├── test_cfdp_directive.c
│   └── unit_tests.c        # Test entry point
├── docs/
│   └── ccsds_cfdp.md       # Implementation notes
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

### Step 1 — Serialise a header and an EOF PDU

```c
#include "cfdp.h"

uint8_t buf[64];

cfdp_pdu_header_t hdr = {0};
hdr.version = CFDP_PROTOCOL_VERSION;
hdr.pdu_type = CFDP_PDU_TYPE_DIRECTIVE;
hdr.direction = CFDP_DIRECTION_TOWARD_RECEIVER;
hdr.transmission_mode = CFDP_TRANS_MODE_UNACKNOWLEDGED;
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
hdr.data_field_length = (uint16_t)plen;
cfdp_pdu_header_serialize(&hdr, buf, sizeof(buf));
size_t total = hlen + plen; /* bytes to transmit */
```

### Step 2 — Parse a received PDU

```c
cfdp_pdu_header_t hdr;
size_t hlen = cfdp_pdu_header_deserialize(rx, rx_len, &hdr);

const uint8_t *payload = rx + hlen;
size_t payload_len = hdr.data_field_length;

if (hdr.pdu_type == CFDP_PDU_TYPE_DIRECTIVE) {
    cfdp_directive_code_t code;
    cfdp_pdu_directive_code(payload, payload_len, &code);
    /* dispatch on `code` (CFDP_DIRECTIVE_EOF, ...) */
} else {
    cfdp_file_data_pdu_t fd;
    cfdp_file_data_deserialize(payload, payload_len, hdr.large_file_flag, &fd);
    /* write fd.file_data_len octets at fd.offset */
}
```

## API Reference

### PDU header (`cfdp_pdu.h`)

```c
size_t cfdp_pdu_header_size(const cfdp_pdu_header_t *hdr);
size_t cfdp_pdu_header_serialize(const cfdp_pdu_header_t *hdr, uint8_t *buf, size_t buf_len);
size_t cfdp_pdu_header_deserialize(const uint8_t *buf, size_t buf_len, cfdp_pdu_header_t *hdr);
```

### File Data (`cfdp_pdu.h`)

```c
size_t cfdp_file_data_serialize(const cfdp_file_data_pdu_t *fd, cfdp_large_file_flag_t large,
                                uint8_t *buf, size_t buf_len);
size_t cfdp_file_data_deserialize(const uint8_t *buf, size_t buf_len, cfdp_large_file_flag_t large,
                                  cfdp_file_data_pdu_t *fd);
```

### Directives (`cfdp_directive.h`)

`cfdp_{eof,finished,ack,metadata,nak,prompt,keep_alive}_{serialize,deserialize}()`
— each returns the number of octets written or consumed, or `0` on error.

### Checksum (`cfdp_checksum.h`)

```c
uint32_t cfdp_checksum_update(uint32_t checksum, uint64_t offset, const uint8_t *data, size_t len);
uint32_t cfdp_checksum_compute(const uint8_t *data, size_t len);
```

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
- File Data segment metadata is not supported (the flag must be absent).
- The 16-bit CRC is not computed or checked; the CRC flag is preserved on the
  wire but the trailer is left to the caller.
- No transaction state machine, timers or retransmission logic.

## References

- CCSDS 727.0-B-5, *CCSDS File Delivery Protocol (CFDP)*, Blue Book.
- CCSDS 720.1-G-4, *CFDP — Part 1: Introduction and Overview*, Green Book.

## License

See [LICENSE](LICENSE) file.
