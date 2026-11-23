# sercom Protocol Specification — Versions 1 and 2

This document specifies the sercom packet-based binary serial protocol. It is
normative for the frame layer and the link layer, and describes the demo
message set used by the course device. The C++ library in `core/` and the
Python reference in `tools/sercom_frame.py` implement it.

The key words MUST, MUST NOT, SHOULD and MAY are used as in RFC 2119.

## 1. Scope and assumptions

- Point-to-point, full-duplex byte stream: UART (any physical layer), USB CDC,
  or a pseudo terminal. No addressing, exactly two peers. Version 2 (§8) adds
  addresses for a chain of nodes; each link of a chain is a version 1 link layer
  of its own.
- The byte stream may drop, corrupt, insert or duplicate bytes. Bytes are never
  reordered.
- 8 data bits, no parity, 1 stop bit (8N1). Default line speed 115200 baud.

## 2. Frame format

All multi-byte fields are **little-endian** (least significant byte first).

| Offset | Size | Field             | Description |
|-------:|-----:|-------------------|-------------|
| 0      | 1    | `SYNC_BYTE_1`     | Always `0xA5` (`0b10100101`) |
| 1      | 1    | `SYNC_BYTE_2`     | Always `0xC3` (`0b11000011`) |
| 2      | 1    | `VERSION`         | Protocol version, `0x01` for this section, `0x02` for the routed frame of §8 |
| 3      | 1    | `FLAGS`           | Bit field, see §3 |
| 4      | 2    | `MESSAGE_COUNTER` | Sender-assigned sequence number, see §5 |
| 6      | 2    | `MESSAGE_TYPE`    | Identifies the payload layout, see §6 |
| 8      | 1    | `PAYLOAD_LENGTH`  | Number of payload bytes `N`, 0 to 255 |
| 9      | N    | `PAYLOAD`         | Message body |
| 9 + N  | 4    | `CRC32`           | CRC over offsets `2 .. 9 + N - 1` |

- Header size (`HEADER_SIZE`): 9 bytes. Fixed overhead per frame (`FRAME_OVERHEAD`): 13 bytes.
  Largest frame of version 1 (`MAXIMUM_DIRECT_FRAME_SIZE`): 268 bytes.
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
| 4   | `0x10` | `ROUTE_CAPABLE`  | The sender reads routed frames, see §8.2 |
| 5–7 | `0xE0` | reserved         | Senders MUST send 0, receivers MUST ignore |

## 4. Receiver rules (frame layer)

A receiver processes the byte stream as follows:

1. Discard bytes until `0xA5` is found. This byte starts a *candidate*.
2. If the next byte is not `0xC3`, discard only the first byte of the candidate
   and continue at step 1 (so `A5 A5 C3` is found).
3. When 9 header bytes are available:
   - `VERSION` other than `0x01` (and `0x02` for a version 2 receiver, §8.5):
     reject the candidate (`INVALID_VERSION`).
   - `PAYLOAD_LENGTH` above the local limit: reject the candidate (`PAYLOAD_TOO_LONG`).
4. When the frame size of its version (`13 + PAYLOAD_LENGTH` or
   `16 + PAYLOAD_LENGTH`) is available, compute the CRC. A mismatch
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
| 6     | `UNREACHABLE`              | The receiver cannot pass the frame on to its destination (§8.8) |
| 7     | `EXPIRED`                  | The time to live of the frame ran out (§8.8) |

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
| `0x0004` | `DEVICE_INFO_RESPONSE`         | device → host | `u8 firmwareVersionMajor, u8 firmwareVersionMinor, u32 uptime, u32 freeHeapSize, str deviceName` [, `u8 protocolVersion`, `u8 nodeAddress`] |
| `0x0005` | `LOG_REPORT_RESPONSE`          | device → host | `u8 logLevelType, str text` |
| `0x0006` | `ECHO_REQUEST`                 | host → device | any bytes |
| `0x0007` | `ECHO_RESPONSE`                | device → host | the request bytes unchanged |
| `0x0008` | `ROUTE_FAILURE_REPORT_RESPONSE` | node → source of a failed frame | see §8.8 |
| `0x0100` | `SET_LED_REQUEST`              | host → device | `u8 ledOn` (0 or 1, anything else is malformed) |
| `0x0101` | `SET_LED_RESPONSE`             | device → host | `u8 ledOn` |
| `0x0102` | `SET_TELEMETRY_PERIOD_REQUEST` | host → device | `u16 telemetryPeriod` (0 stops, 1 to 19 is rejected with `INVALID_ARGUMENT`) |
| `0x0103` | `TELEMETRY_REPORT_RESPONSE`    | device → host | `u16 sequence, u32 uptime, u32 freeHeapSize, i16 temperature` |

`str` is a `u8` length followed by that many bytes, no terminator.
Times (`requestTimestamp`, `uptime`, `telemetryPeriod`) are milliseconds, sizes
(`freeHeapSize`) are bytes and `temperature` is in 0.01 °C steps.
A device without a temperature reading sends `-32768` (-327.68 °C, below absolute zero).
`logLevelType`: 0 `UNKNOWN`, 1 `DEBUG`, 2 `WARNING`.
A node with an address appends `protocolVersion` and `nodeAddress` to its
`DEVICE_INFO_RESPONSE`, a parser reads them only when present (defaults 1 and 0).

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
- A device sends its log and telemetry reports to the source of the latest
  request, direct after a direct one; a device with an address starts with the host.
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
- Addressing: see §8.

## 8. Version 2: routed frames

Version 2 lets boards form a chain, for example host, node 2, node 3 and node 4,
each board wired to the next one by its own serial link. Every link of the chain
is a complete link layer of §5 with its own message counter, window,
retransmissions and duplicate history. A frame for a node further down carries
the address of its source and of its destination and a time to live; each node
on the way takes the frame and sends it again on its next link.

### 8.1 Routed frame

A frame with `VERSION = 0x02` is a *routed frame*, a frame with `VERSION = 0x01`
a *direct frame*. The routed frame keeps the nine bytes of the direct header at
offsets 0 to 8 and adds three routing bytes behind `PAYLOAD_LENGTH`:

| Offset | Size | Field                 | Description |
|-------:|-----:|-----------------------|-------------|
| 0      | 1    | `SYNC_BYTE_1`         | Always `0xA5` |
| 1      | 1    | `SYNC_BYTE_2`         | Always `0xC3` |
| 2      | 1    | `VERSION`             | `0x02` |
| 3      | 1    | `FLAGS`               | §3 together with `ROUTE_CAPABLE` (§8.2) |
| 4      | 2    | `MESSAGE_COUNTER`     | The counter of this link, §5 |
| 6      | 2    | `MESSAGE_TYPE`        | §6 |
| 8      | 1    | `PAYLOAD_LENGTH`      | Number of payload bytes `N`, 0 to 255 |
| 9      | 1    | `SOURCE_ADDRESS`      | The node that started the frame |
| 10     | 1    | `DESTINATION_ADDRESS` | The node the frame is for, or `0xFF` |
| 11     | 1    | `TTL`                 | Links the frame may still cross |
| 12     | N    | `PAYLOAD`             | Message body |
| 12 + N | 4    | `CRC32`               | CRC over offsets `2 .. 12 + N - 1` |

- Header size (`ROUTED_HEADER_SIZE`): 12 bytes. Fixed overhead (`ROUTED_FRAME_OVERHEAD`):
  16 bytes. Largest frame (`MAXIMUM_FRAME_SIZE`): 271 bytes, the largest direct
  frame stays 268 bytes (`MAXIMUM_DIRECT_FRAME_SIZE`).
- The CRC32 of §2.1 covers the routing bytes, so a damaged `TTL` or address is a
  CRC mismatch. A node that changes `TTL` writes a new CRC32.
- Offsets 0 to 8 are the same in both versions, so a receiver reads
  `PAYLOAD_LENGTH` at offset 8 before it knows the frame size.

### 8.2 Route capable flag

| Bit | Mask   | Name            | Meaning |
|----:|--------|-----------------|---------|
| 4   | `0x10` | `ROUTE_CAPABLE` | The sender reads routed frames |

A node with an address MUST set the flag on every frame it sends, direct and
routed, ACK and NACK included. A node without an address MUST NOT set it. A
version 1 receiver ignores the bit as a reserved bit (§3).

### 8.3 Addresses

| Value            | Name                 | Meaning |
|------------------|----------------------|---------|
| `0x00`           | `UNASSIGNED_ADDRESS` | No address; never the source or the destination of a routed frame |
| `0x01`           | `HOST_ADDRESS`       | The host at the top of the chain |
| `0x02` to `0xFE` | node addresses       | One per node of a chain, unique (`MINIMUM_NODE_ADDRESS` to `MAXIMUM_NODE_ADDRESS`) |
| `0xFF`           | `BROADCAST_ADDRESS`  | Every node of the chain, only as a destination |

A node without an address sends and accepts direct frames only and sends the
bytes of version 1.

### 8.4 Time to live

- `TTL` counts the links a routed frame may still cross. A sender writes its
  configured value, `DEFAULT_TIME_TO_LIVE` (8) unless configured otherwise, and
  MUST NOT send `TTL = 0`.
- A node delivers a frame for its own address whatever its `TTL`.
- A node forwards a frame only with `TTL >= 2` and writes `TTL - 1`. A frame it
  would have to forward with `TTL <= 1` is dropped (`EXPIRED`).

### 8.5 Versions on a link

1. A version 2 receiver accepts `VERSION` 1 and 2 in §4 step 3 and rejects every
   other value as `INVALID_VERSION`. The frame size of §4 step 4 is `13 + N` for
   version 1 and `16 + N` for version 2.
2. A version 1 receiver rejects a routed frame as `INVALID_VERSION`, which is the
   unchanged behaviour of version 1.
3. Every link of a version 2 node keeps the *peer version* `UNKNOWN` (at start),
   `VERSION_1` or `VERSION_2`. Every valid frame received on the link sets it:
   `VERSION_2` if the frame is routed or carries `ROUTE_CAPABLE`, `VERSION_1`
   otherwise. A routed message whose delivery timed out (§5.2) sets it back to
   `UNKNOWN`, because a peer that never answered it may not read routed frames.
4. ACK and NACK frames are direct frames, also when they answer a routed frame.
5. A routed frame MUST NOT be sent on a link whose peer version is `VERSION_1`; it
   MAY be sent while the peer version is `UNKNOWN`.
6. A message for the neighbour itself (no destination address) is a direct frame.

| Peers | Result |
|-------|--------|
| version 1 node and version 2 node without an address | the bytes of version 1 in both directions |
| version 1 host and version 2 node with an address | works; the start-up reports of the node are routed until the first request of the host and are dropped by the host as noise; from then on the node answers and reports in direct frames |
| version 2 host sending to a destination and version 1 device | the routed requests time out; a direct frame of the device sets the peer version to `VERSION_1`, then the routed sends fail with `NO_ROUTE`; requests without a destination work |
| version 2 router and version 1 device below it | works through the stand-in of §8.9 |

### 8.6 Nodes, ports and own frames

A node has a *primary port*: the serial port of the host, and the port towards
the host of every other node. A router has a *secondary port* too, the port away
from the host. There is no routing table: in a chain the other port is the only
route. A node sends its own frames on its primary port:

1. No destination address: a direct frame.
2. A node without an address and a destination address: the send fails (`NO_ROUTE`).
3. The destination is the address of the node itself: the send fails (`INVALID_ARGUMENT`).
4. The primary peer version is `VERSION_1`: a frame for the host is sent as a
   direct frame, because that peer is the host; any other destination fails
   (`NO_ROUTE`).
5. Otherwise a routed frame: source the own address, destination as given
   (`BROADCAST_ADDRESS` included), the configured `TTL`.

### 8.7 Receiving a message frame

After the link layer has handled ACK, NACK and duplicates (§5, unchanged but for
the `BUSY` of a routed frame below), a message frame that arrives on port P of
node N with address A is handled by the first rule that matches. The last column
is the status of the ACK or NACK when the frame requested one.

| Rule | Frame | Action | Answer |
|------|-------|--------|--------|
| D1 | direct, P is the primary port | deliver to the application | the status of the application |
| D2 | direct, P is the secondary port, N has a proxy address | pass up as a routed frame (§8.9) | as R10 |
| D3 | direct, otherwise | drop | `UNREACHABLE` |
| R1 | routed, A is unassigned | drop | `UNREACHABLE` |
| R2 | routed, source unassigned or broadcast, or destination unassigned | drop | `UNREACHABLE` |
| R3 | routed, source equals A | drop, count a loop | `UNREACHABLE` |
| R4 | routed, destination equals A | deliver | the status of the application |
| R5 | routed, destination broadcast | deliver, and with `TTL >= 2` pass a copy to the other port | the status of the application |
| R6 | routed, destination the host, P is the primary port | drop | `UNREACHABLE` |
| R7 | routed, `TTL <= 1` | drop | `EXPIRED` |
| R8 | routed, N has no other port | drop | `UNREACHABLE` |
| R9 | routed, the peer version of the other port is `VERSION_1`, or the destination is the proxy address of N, the other port is its secondary port and its peer version is `UNKNOWN` | for the proxy address on the secondary port: send down as a direct frame (§8.9); otherwise drop | as R10, or `UNREACHABLE` |
| R10 | routed, otherwise | send on the other port with `TTL - 1` | sent: `ACCEPTED`, window full: `BUSY`, otherwise `UNREACHABLE` |

- A node that forwards a frame builds a new frame for the next link: the same
  source, destination, message type, payload and `ACK_REQUEST`, `TTL - 1`, the
  counter and flags of the next link and a new CRC32. An acknowledged frame waits
  in the window of the next link and only that link retransmits it.
- The answer to the previous hop is given at once. `ACCEPTED` means that the
  node now holds the frame, not that the destination has it.
- `BUSY` means that the receiver took nothing, so the sender still holds the
  frame. For a routed frame for a node, `BUSY` does not end the delivery (§5.2):
  the sender keeps the frame in its window and retransmits it after
  `retransmitTimeout` as if no answer had come, within the same
  `maximumRetransmissions`, and the delivery fails (`REJECTED`) only when the
  last retransmission is answered with `BUSY` too. The receiver delivers such a
  retransmission again instead of repeating its stored `BUSY` (§5.3). A direct
  frame and a broadcast end their delivery at the first `BUSY` as in version 1,
  because a broadcast copy may have left already.
- A broadcast copy (R5) is best effort: its failure is counted, never answered or
  reported.
- A frame never leaves on the port it arrived on, a frame for the host never
  leaves on a secondary port and a frame of the own source is dropped, so the
  `TTL` only has to bound what a wrong wiring leaves.

### 8.8 Statuses and the route failure report

New message statuses (§5.5): 6 `UNREACHABLE` (the receiver cannot pass the
frame on to its destination) and 7 `EXPIRED` (the `TTL` of the frame ran out).
New send status of an implementation: `NO_ROUTE`. A version 1 peer never
receives them, because it sends no routed frames.

`ROUTE_FAILURE_REPORT_RESPONSE`, type `0x0008`, is fire and forget and travels as
a routed frame from the reporting node to the source of the failed frame:

| Offset | Size          | Field                 | Meaning |
|-------:|--------------:|-----------------------|---------|
| 0      | 1             | `destinationAddress`  | Destination of the failed frame |
| 1      | 2             | `messageType`         | Message type of the failed frame |
| 3      | 1             | `deliveryFailureType` | 1 `TIMED_OUT`, 2 `REJECTED` |
| 4      | 1             | `messageStatusType`   | The NACK status of the next hop, 0 after a timeout |
| 5      | 1             | `quoteLength`         | 0 to 8 (`MAXIMUM_ROUTE_FAILURE_QUOTE_SIZE`) |
| 6      | `quoteLength` | `quote`               | The first payload bytes of the failed frame |

- A node that passed on an acknowledged frame (rules D2, R9 and R10) remembers it
  until its next hop answers.
- When that delivery fails, the node sends a report on the port the frame arrived
  on, to the source of the frame, with its configured `TTL`.
- No report is sent about a report or a broadcast, to a source that is
  unassigned, broadcast or the proxy address, or on a port whose peer version is
  `VERSION_1`.
- A report is never acknowledged and never answered.

### 8.9 A neighbour without an address

A router MAY have a *proxy address* X and stand in for one neighbour without an
address on its secondary port, for example a device that runs version 1. It
keeps a *proxy report address*, `HOST_ADDRESS` at start.

- R9 sends a routed frame for X down as a direct frame with the same message
  type, payload and `ACK_REQUEST`, and sets the proxy report address to the
  source of the frame.
- D2 passes a direct frame of the secondary port up as a routed frame with source
  X, the proxy report address as destination and the configured `TTL`.
- A node without an address drops every routed frame (R1), so a frame for X can
  only reach that neighbour as a direct frame; the peer version of a new
  neighbour is `UNKNOWN` until its first frame.

### 8.10 End-to-end matching (informative)

Message counters change on every hop, so nothing end to end may rely on them.
The demo messages carry their own correlation: the ping response its
`requestTimestamp`, the echo its sequence number, info, LED and telemetry their
message type and the source address of the response. The device client of the
reference keeps its last 8 requests (`REQUEST_HISTORY_SIZE`) with type,
destination, counter and the first 8 payload bytes, and turns a report that
matches the newest entry with the same type, destination and quote into the
failure of that request. A ping to the broadcast address stays pending until it
is pushed out of the ping history, so the response of every node is measured.
A response that a hop finally fails, after its retransmissions and the `BUSY`
answers of §8.7, is reported to its source, the device, and never reaches the
client. A client that needs every answer sends a request again when its response
is overdue, as the stress command of the reference does with a destination.

### 8.11 Example

The golden frame of §2.2 as a routed frame from the host (1) to node 3, counter
7, `ACK_REQUEST` and `ROUTE_CAPABLE`, `TTL` 8:

```
A5 C3 | 02 | 11 | 07 00 | 01 00 | 04 | 01 | 03 | 08 | E8 03 00 00 | 2B AB 4A 47
```

- `02` version, `11` flags, `01` source, `03` destination, `08` time to live,
  `2B AB 4A 47` CRC32 `0x474AAB2B`, 20 bytes.

The same request as node 2 forwards it on its next link with that link's counter 2:

```
A5 C3 | 02 | 11 | 02 00 | 01 00 | 04 | 01 | 03 | 07 | E8 03 00 00 | EA 4B B9 5D
```

- `02 00` counter of the next link, `07` time to live, `EA 4B B9 5D` CRC32 `0x5DB94BEA`.

A node with an address sends the direct ping with its flag:
`A5 C3 01 11 07 00 01 00 04 E8 03 00 00 C9 93 7E 44`, the version 1 golden frame
`A5 C3 01 01 07 00 01 00 04 E8 03 00 00 21 12 D0 63` stays unchanged.

### 8.12 Not part of version 2

- End-to-end acknowledgement: an ACK confirms one hop, the response confirms a request.
- Routing tables: a node has at most two ports; trees and stars need a table.
- Answers to requests from below: a node sends its own frames up, so a request
  that reaches it from below is delivered but its answer cannot return.
- Fragmentation and security, as in §7.
