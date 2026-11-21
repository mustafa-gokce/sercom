# sercom Protocol Specification — Version 1

This document specifies the sercom packet-based binary serial protocol. It is
normative for the frame layer and the link layer, and describes the demo
message set used by the course device.

The key words MUST, MUST NOT, SHOULD and MAY are used as in RFC 2119.

## 1. Scope and assumptions

- Point-to-point, full-duplex byte stream: UART (any physical layer), USB CDC,
  or a pseudo terminal. No addressing, exactly two peers.
- The byte stream may drop, corrupt, insert or duplicate bytes. Bytes are never
  reordered.
- 8 data bits, no parity, 1 stop bit (8N1). Default line speed 115200 baud.

## 2. Frame format

All multi-byte fields are **little-endian** (least significant byte first).

| Offset | Size | Field             | Description |
|-------:|-----:|-------------------|-------------|
| 0      | 1    | `SYNC_BYTE_1`     | Always `0xA5` (`0b10100101`) |
| 1      | 1    | `SYNC_BYTE_2`     | Always `0xC3` (`0b11000011`) |
| 2      | 1    | `VERSION`         | Protocol version, `0x01` for this document |
| 3      | 1    | `FLAGS`           | Bit field, see §3 |
| 4      | 2    | `MESSAGE_COUNTER` | Sender-assigned sequence number, see §5 |
| 6      | 2    | `MESSAGE_TYPE`    | Identifies the payload layout, see §6 |
| 8      | 1    | `PAYLOAD_LENGTH`  | Number of payload bytes `N`, 0 to 255 |
| 9      | N    | `PAYLOAD`         | Message body |
| 9 + N  | 4    | `CRC32`           | CRC over offsets `2 .. 9 + N - 1` |

- Header size (`HEADER_SIZE`): 9 bytes. Fixed overhead per frame (`FRAME_OVERHEAD`): 13 bytes.
  Largest frame (`MAXIMUM_FRAME_SIZE`): 268 bytes.
- `PAYLOAD_LENGTH` MAY be 0.
- The one byte length field limits payloads to 255 bytes. An implementation MAY
  declare a lower limit `MAXIMUM_PAYLOAD_SIZE` (the reference and ESP32 builds use
  255). Peers MUST agree on a limit, a frame above the receiver's limit is
  rejected. Larger messages are split by the application (see §7).

### 2.1 CRC32

- Algorithm: CRC-32/ISO-HDLC, the same CRC as IEEE 802.3 Ethernet, zlib and
  Python's `binascii.crc32`.
  - Polynomial `0x04C11DB7` (reflected `0xEDB88320`, `CRC32_POLYNOMIAL`), initial
    value `0xFFFFFFFF` (`CRC32_INITIAL_VALUE`), input and output reflected, final
    XOR `0xFFFFFFFF` (`CRC32_FINAL_XOR_VALUE`).
  - Check value: CRC of the ASCII string `123456789` is `0xCBF43926`.
- Coverage: from `VERSION` through the last payload byte. The sync word is not
  covered, because the receiver already compared it literally.
- Stored little-endian.

### 2.2 Example

`PING_REQUEST`, message counter 7, `ACK_REQUEST` set, payload = `requestTimestamp` 1000 (`u32`):

```
A5 C3 | 01 | 01 | 07 00 | 01 00 | 04 | E8 03 00 00 | 21 12 D0 63
```

- `A5 C3` sync word, `01` version, `01` flags, `07 00` message counter,
  `01 00` message type, `04` payload length, `E8 03 00 00` payload,
  `21 12 D0 63` CRC32 `0x63D01221`.

## 3. Flags

| Bit | Mask   | Name             | Meaning |
|----:|--------|------------------|---------|
| 0   | `0x01` | `ACK_REQUEST`    | Sender wants an ACK or NACK for this frame |
| 1   | `0x02` | `ACK`            | This frame positively acknowledges a frame |
| 2   | `0x04` | `NACK`           | This frame rejects a frame, payload carries the reason |
| 3   | `0x08` | `RETRANSMISSION` | This frame repeats an earlier transmission |
| 4–7 | `0xF0` | reserved         | Senders MUST send 0, receivers MUST ignore |

## 4. Receiver rules (frame layer)

A receiver processes the byte stream as follows:

1. Discard bytes until `0xA5` is found. This byte starts a *candidate*.
2. If the next byte is not `0xC3`, discard only the first byte of the candidate
   and continue at step 1 (so `A5 A5 C3` is found).
3. When 9 header bytes are available:
   - `VERSION` other than `0x01`: reject the candidate (`INVALID_VERSION`).
   - `PAYLOAD_LENGTH` above the local limit: reject the candidate (`PAYLOAD_TOO_LONG`).
4. When `13 + PAYLOAD_LENGTH` bytes are available, compute the CRC. A mismatch
   rejects the candidate (`CRC_MISMATCH`). A match yields a valid frame, and all
   its bytes are consumed.
5. **Rejecting a candidate discards only its first byte.** The remaining bytes
   are scanned again from step 1, because the start of the real next frame may
   lie inside the rejected candidate (for example when a corrupted length made
   the receiver wait too long).
6. **Inter-byte timeout:** if a candidate is incomplete and no byte arrives for
   `interByteTimeout` (default `DEFAULT_INTER_BYTE_TIMEOUT_MSEC`, 50 ms), the
   buffered bytes are discarded (`INTER_BYTE_TIMEOUT`). Timestamps are 64-bit
   milliseconds that do not wrap around.

The names in parentheses are the decode error types a receiver counts.
Corrupted frames are never answered. Recovery is left to retransmission (§5).

## 5. Link layer

### 5.1 Message counter

- Every frame a peer sends, except ACK and NACK frames, carries the next value
  of a 16-bit counter that starts at 0 and wraps from 65535 to 0.
- ACK and NACK frames copy `MESSAGE_COUNTER` and `MESSAGE_TYPE` from the frame
  they answer, so they consume no counter value.

### 5.2 Acknowledged messages

- A frame with `ACK_REQUEST` MUST be answered by exactly one of:
  - an **ACK** frame: `FLAGS = ACK`, same counter and type, empty payload, or
  - a **NACK** frame: `FLAGS = NACK`, same counter and type, payload of one byte
    holding a `MessageStatusType` (§5.5).
- The sender keeps a copy of every acknowledged message until it is answered.
  At most `MAXIMUM_PENDING_FRAMES` (4) messages may be pending.
- If no answer arrives within `retransmitTimeout` (default
  `DEFAULT_RETRANSMIT_TIMEOUT_MSEC`, 250 ms), the sender retransmits the frame
  unchanged except that the `RETRANSMISSION` flag is set. After
  `maximumRetransmissions` (default `DEFAULT_MAXIMUM_RETRANSMISSIONS`, 5) retries
  the delivery fails (`TIMED_OUT`).
- A NACK ends the delivery immediately (`REJECTED`) and is not retried by the
  link layer.

### 5.3 Duplicate suppression

- A receiver remembers the counter, type and returned status of at least
  `DEFAULT_DUPLICATE_HISTORY_SIZE` (16) recently delivered acknowledged messages.
- A frame with both `ACK_REQUEST` and `RETRANSMISSION` set whose counter and type
  are in this history is a duplicate: the receiver repeats the stored answer
  and MUST NOT deliver the message again.
- A frame **without** `RETRANSMISSION` is always new, even if its counter is in
  the history. This keeps a peer that restarted (and starts counting at 0 again)
  from having its first messages dropped.

### 5.4 Spread limit

A receiver can only recognise a retransmission it still remembers. Therefore a
sender MUST NOT send a new acknowledged message while its oldest pending message
was followed by `MAXIMUM_ACKNOWLEDGED_SPREAD` or more newer acknowledged messages:

```
MAXIMUM_ACKNOWLEDGED_SPREAD = DEFAULT_DUPLICATE_HISTORY_SIZE - MAXIMUM_PENDING_FRAMES + 1 = 13
```

Fire-and-forget messages (no `ACK_REQUEST`) are never retransmitted and are not
limited.

### 5.5 Message status (NACK payload)

| Value | Name                       | Meaning |
|------:|----------------------------|---------|
| 0     | `UNKNOWN`                  | Reason not given or not understood |
| 1     | `ACCEPTED`                 | Never sent in a NACK, an ACK is used instead |
| 2     | `UNSUPPORTED_MESSAGE_TYPE` | The receiver does not implement this type |
| 3     | `MALFORMED_PAYLOAD`        | The payload does not match the type's layout |
| 4     | `BUSY`                     | Temporarily unable to handle it, try again later |
| 5     | `INVALID_ARGUMENT`         | A field of the payload is out of range |

### 5.6 Ordering

With more than one pending message and retransmissions, acknowledged messages
may be delivered out of order. Applications that need ordering use a window of 1
or carry their own sequence numbers.

## 6. Message types

| Range             | Use |
|-------------------|-----|
| `0x0000`          | Invalid, never sent |
| `0x0001 – 0x00FF` | System messages |
| `0x0100 – 0xFFFF` | Application messages |

### 6.1 Demo device message set

| Type     | Name                           | Direction     | Payload |
|----------|--------------------------------|---------------|---------|
| `0x0001` | `PING_REQUEST`                 | host → device | `u32 requestTimestamp` |
| `0x0002` | `PING_RESPONSE`                | device → host | `u32 requestTimestamp, u32 uptime` |
| `0x0003` | `DEVICE_INFO_REQUEST`          | host → device | empty |
| `0x0004` | `DEVICE_INFO_RESPONSE`         | device → host | `u8 firmwareVersionMajor, u8 firmwareVersionMinor, u32 uptime, u32 freeHeapSize, str deviceName` |
| `0x0005` | `LOG_REPORT_RESPONSE`          | device → host | `u8 logLevelType, str text` |
| `0x0006` | `ECHO_REQUEST`                 | host → device | any bytes |
| `0x0007` | `ECHO_RESPONSE`                | device → host | the request bytes unchanged |
| `0x0100` | `SET_LED_REQUEST`              | host → device | `u8 ledOn` (0 or 1, anything else is malformed) |
| `0x0101` | `SET_LED_RESPONSE`             | device → host | `u8 ledOn` |
| `0x0102` | `SET_TELEMETRY_PERIOD_REQUEST` | host → device | `u16 telemetryPeriod` (0 stops, 1 to 19 is rejected with `INVALID_ARGUMENT`) |
| `0x0103` | `TELEMETRY_REPORT_RESPONSE`    | device → host | `u16 sequence, u32 uptime, u32 freeHeapSize, i16 temperature` |

`str` is a `u8` length followed by that many bytes, no terminator.
Times (`requestTimestamp`, `uptime`, `telemetryPeriod`) are milliseconds, sizes
(`freeHeapSize`) are bytes and `temperature` is in 0.01 °C steps.
A device without a temperature reading sends `-32768` (-327.68 °C, below absolute zero).
`logLevelType`: 0 `UNKNOWN`, 1 `DEBUG`, 2 `WARNING`.

Rules of the demo application:

- A response is sent acknowledged when its request was acknowledged.
- `TELEMETRY_REPORT_RESPONSE` and `LOG_REPORT_RESPONSE` are fire-and-forget.
- The acknowledgement of a `SET_TELEMETRY_PERIOD_REQUEST` is its only response.
  A telemetry period from 1 to 19 ms is answered with NACK `INVALID_ARGUMENT`
  and the device keeps its current telemetry period.
- A device name has at most 31 characters. The device does not send a log text
  above 127 characters.
- A device that cannot send a reliable response because its window is full
  answers the request with NACK `BUSY` before acting on it.
- Payload parsers MUST accept trailing bytes after the known fields, so a newer
  peer may append fields without breaking an older one.

## 7. Not part of version 1

- Sessions: version 1 has no reset handshake. A receiver matches a retransmission
  against its duplicate history (§5.3) without knowing whether the sender
  restarted. A reset handshake is a course exercise.
- Security: version 1 has no authentication or encryption. CRC32 detects
  transmission errors, not attacks.
- Fragmentation: a message fits into one frame. Larger data is split by the
  application.
