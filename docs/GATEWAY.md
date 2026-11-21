# Gateway Design Note

A gateway is a board between the host and a second board: the host talks to device A,
and device A forwards to device B what is meant for it. This note explains why version 1
of the protocol has no gateway, and what a gateway would need. The firmware of this
repository stops at the controller role of [HARDWARE.md](HARDWARE.md#11-device-to-device),
where one board queries another one directly.

## Why it does not fit version 1

1. A frame has no address. Every field of the header (`VERSION`, `FLAGS`,
   `MESSAGE_COUNTER`, `MESSAGE_TYPE`, `PAYLOAD_LENGTH`) describes the message, none of
   them names a receiver, so device A cannot tell whether a `PING_REQUEST` is meant for
   itself or for device B.
2. The link layer works hop by hop. The acknowledgements, retransmissions and the
   duplicate history of [PROTOCOL.md](PROTOCOL.md) belong to one serial link with its own
   message counters. Device A would acknowledge the host before device B has seen the
   request, so the host's `ACK` would no longer mean that the request arrived.
3. `DeviceApplication` answers within `onReceivedMessage`. Its handlers build the
   response at once from the local platform; a forwarded request is answered only when
   device B's response arrives, many maintenance ticks later. Forwarding needs a table of
   requests in flight, a timeout per entry and a way to answer the host's request late.
4. Device A must stay a device for the host and become a controller for device B at the
   same time, with two links, two windows of pending frames and two duplicate histories.

## What a gateway would need

- An addressed envelope instead of a header change: a new message type pair, for
  example `FORWARD_REQUEST = 0x0008` with `u8 deviceAddress` followed by the inner
  message type and payload, and `FORWARD_RESPONSE = 0x0009` carrying the inner response
  back. Older devices answer the unknown type with NACK `UNSUPPORTED_MESSAGE_TYPE`, so
  the frame format and version 1 stay as they are.
- A forwarding table in device A: the host's message counter, the counter of the
  request on the link to device B and its send time, sized by `MaximumPendingFrames`.
- Late responses: device A acknowledges the `FORWARD_REQUEST` at once and sends the
  `FORWARD_RESPONSE` as its own acknowledged message when device B answers, or a
  `FORWARD_RESPONSE` with a failure status when the request to device B failed.
- A role on device A that owns both a `DeviceApplication` towards the host and a
  `DeviceClient` towards device B, the `DeviceRole` and the `ControllerRole` of
  `firmware/common` in one object.

## What a plain byte bridge does instead

Device A can also copy the bytes of one UART to the other without decoding them. The
host then talks to device B end to end, with the link layer of both ends intact, and
device A is invisible, exactly like the USB-serial adapter of section 2 of
[HARDWARE.md](HARDWARE.md). This is the right tool for reaching a board without a USB
port; it gives no access to device A itself.
