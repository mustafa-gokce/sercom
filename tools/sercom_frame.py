"""Build, decode and check sercom frames with a second implementation of the frame format.

    python3 tools/sercom_frame.py encode --type TYPE [--counter COUNTER] [--flags FLAGS] [--payload HEX] [--binary]
                                         [--version 2 --source SOURCE --destination DESTINATION [--ttl TTL]]
    python3 tools/sercom_frame.py decode CAPTURE
    python3 tools/sercom_frame.py self-test

The C++ library is the production implementation, this one cross-checks it and works from a shell without a build.
Every multi-byte field is little-endian, docs/PROTOCOL.md specifies the frame:

    A5 C3 | VERSION u8 | FLAGS u8 | MESSAGE_COUNTER u16 | MESSAGE_TYPE u16 | PAYLOAD_LENGTH u8 | PAYLOAD | CRC32 u32

The routed frame of version 2 adds SOURCE u8 | DESTINATION u8 | TTL u8 between PAYLOAD_LENGTH and PAYLOAD.
The CRC32 is the IEEE 802.3 and zlib CRC (binascii.crc32) from VERSION through the last payload byte.
A capture file holds raw bytes, - reads them from standard input.
"""

import argparse
import binascii
import random
import struct
import sys

SYNC_BYTE_1 = 0xA5
SYNC_BYTE_2 = 0xC3
PROTOCOL_VERSION = 1
ROUTED_PROTOCOL_VERSION = 2
SYNC_SIZE = 2
HEADER_SIZE = 9
ROUTED_HEADER_SIZE = 12
CRC_SIZE = 4
DEFAULT_TIME_TO_LIVE = 8
MAXIMUM_PAYLOAD_SIZE = 255
CRC32_MASK = 0xFFFFFFFF

# layout of the header fields after the sync word
HEADER_FORMAT = '<BBHHB'

# layout of the route header of a routed frame
ROUTE_HEADER_FORMAT = '<BBB'

# layout of the CRC32 field
CRC_FORMAT = '<I'

# frame flag types by their bit
FRAME_FLAG_TYPES = {0x01: 'ACK_REQUEST', 0x02: 'ACK', 0x04: 'NACK', 0x08: 'RETRANSMISSION', 0x10: 'ROUTE_CAPABLE'}

# header sizes by frame version
HEADER_SIZES = {PROTOCOL_VERSION: HEADER_SIZE, ROUTED_PROTOCOL_VERSION: ROUTED_HEADER_SIZE}

# decode error types of a rejected candidate
INVALID_VERSION = 'INVALID_VERSION'
PAYLOAD_TOO_LONG = 'PAYLOAD_TOO_LONG'
CRC_MISMATCH = 'CRC_MISMATCH'


def calculate_crc32(data):
    """Return the CRC32 of the given bytes."""
    return binascii.crc32(data) & CRC32_MASK


def encode(flags, message_counter, message_type, payload=b'', route=None):
    """Return the bytes of one frame, a routed frame when route holds source, destination and TTL."""
    if len(payload) > MAXIMUM_PAYLOAD_SIZE:
        raise ValueError('payload of %d bytes does not fit into the one byte length field' % len(payload))
    version = PROTOCOL_VERSION if route is None else ROUTED_PROTOCOL_VERSION
    body = struct.pack(HEADER_FORMAT, version, flags, message_counter, message_type, len(payload))
    if route is not None:
        body += struct.pack(ROUTE_HEADER_FORMAT, *route)
    body += bytes(payload)
    return bytes([SYNC_BYTE_1, SYNC_BYTE_2]) + body + struct.pack(CRC_FORMAT, calculate_crc32(body))


class Decoder:
    """Byte stream decoder with the resynchronization rules of the C++ FrameDecoder."""

    def __init__(self, maximum_payload_size=MAXIMUM_PAYLOAD_SIZE):
        self.maximum_payload_size = maximum_payload_size
        self.buffer = bytearray()
        self.discarded_byte_counter = 0

    def on_received_data(self, data):
        """Add bytes and return the results as ('frame', fields) and ('error', decode error type) pairs."""
        self.buffer.extend(data)
        results = []
        while True:
            result = self.process_buffer()
            if result is None:
                return results
            if result != 'progress':
                results.append(result)

    def discard(self, count):
        """Drop bytes from the front of the buffer."""
        del self.buffer[:count]
        self.discarded_byte_counter += count

    def process_buffer(self):
        """Take one step on the buffer: None waits for bytes, 'progress' skipped bytes, else a result."""
        buffer = self.buffer
        if not buffer:
            return None

        # skip to the next first sync byte
        if buffer[0] != SYNC_BYTE_1:
            index = buffer.find(bytes([SYNC_BYTE_1]))
            self.discard(len(buffer) if index < 0 else index)
            return 'progress'
        if len(buffer) < SYNC_SIZE:
            return None

        # a second first sync byte stays as the next candidate
        if buffer[1] != SYNC_BYTE_2:
            self.discard(1)
            return 'progress'
        if len(buffer) < HEADER_SIZE:
            return None

        # check the header of the candidate, the payload length sits at the same offset in both versions
        version, flags, message_counter, message_type, payload_length = struct.unpack_from(
            HEADER_FORMAT, buffer, SYNC_SIZE)
        if version not in HEADER_SIZES:
            self.discard(1)
            return ('error', INVALID_VERSION)
        if payload_length > self.maximum_payload_size:
            self.discard(1)
            return ('error', PAYLOAD_TOO_LONG)

        # check the CRC32, a rejected candidate loses its first byte only
        header_size = HEADER_SIZES[version]
        frame_size = header_size + payload_length + CRC_SIZE
        if len(buffer) < frame_size:
            return None
        body = bytes(buffer[SYNC_SIZE:header_size + payload_length])
        (received_crc,) = struct.unpack_from(CRC_FORMAT, buffer, header_size + payload_length)
        if calculate_crc32(body) != received_crc:
            self.discard(1)
            return ('error', CRC_MISMATCH)

        # consume the frame, a routed frame with its route
        fields = {
            'version': version,
            'flags': flags,
            'counter': message_counter,
            'type': message_type,
            'route': None,
            'payload': bytes(buffer[header_size:header_size + payload_length]),
        }
        if version == ROUTED_PROTOCOL_VERSION:
            fields['route'] = struct.unpack_from(ROUTE_HEADER_FORMAT, buffer, HEADER_SIZE)
        del buffer[:frame_size]
        return ('frame', fields)


def get_flags_text(flags):
    """Return the names of the set flags joined by |, NONE without a flag."""
    names = [name for bit, name in FRAME_FLAG_TYPES.items() if flags & bit]
    return '|'.join(names) if names else 'NONE'


def parse_integer(text):
    """Parse a decimal or 0x prefixed integer."""
    return int(text, 0)


def self_test():
    """Check the golden frame, a chunked round trip and the recovery from damage, return the exit code."""
    failures = []

    # golden frame of docs/PROTOCOL.md and tests/test_Frame.cpp
    golden_frame = bytes.fromhex('A5C3010107000100' + '04E8030000' + '2112D063')
    if encode(0x01, 7, 0x0001, struct.pack('<I', 1000)) != golden_frame:
        failures.append('golden frame mismatch')
    if calculate_crc32(b'123456789') != 0xCBF43926:
        failures.append('CRC32 check value mismatch')

    # golden routed frame from the host to node 3 and its copy forwarded by node 2, docs/PROTOCOL.md section 8.11
    golden_routed_frame = bytes.fromhex('A5C3021107000100' + '04010308' + 'E8030000' + '2BAB4A47')
    forwarded_routed_frame = bytes.fromhex('A5C3021102000100' + '04010307' + 'E8030000' + 'EA4BB95D')
    if encode(0x11, 7, 0x0001, struct.pack('<I', 1000), (1, 3, DEFAULT_TIME_TO_LIVE)) != golden_routed_frame:
        failures.append('golden routed frame mismatch')
    if encode(0x11, 2, 0x0001, struct.pack('<I', 1000), (1, 3, DEFAULT_TIME_TO_LIVE - 1)) != forwarded_routed_frame:
        failures.append('forwarded routed frame mismatch')

    # decode 300 frames fed in random chunks
    generator = random.Random(5)
    stream = b''.join(encode(0, counter, 0x0100, bytes([counter & 0xFF]) * (counter % 50)) for counter in range(300))
    decoder = Decoder()
    frames = []
    position = 0
    while position < len(stream):
        size = generator.randint(1, 40)
        results = decoder.on_received_data(stream[position:position + size])
        frames += [value for kind, value in results if kind == 'frame']
        position += size
    if [frame['counter'] for frame in frames] != list(range(300)):
        failures.append('round trip lost frames')

    # survive a garbage byte, a repeated sync byte and a corrupted frame
    damaged_frame = bytearray(encode(0, 1, 0x0100, b'\x01\x02\x03'))
    damaged_frame[10] ^= 0x01
    stream = b'\x00\xA5' + bytes(damaged_frame) + encode(0, 2, 0x0100, b'') + encode(0, 3, 0x0100, b'\xA5\xC3')
    results = Decoder().on_received_data(stream)
    counters = [value['counter'] for kind, value in results if kind == 'frame']
    errors = [value for kind, value in results if kind == 'error']
    if counters != [2, 3]:
        failures.append('recovered message counters %s instead of [2, 3]' % counters)
    if errors != [CRC_MISMATCH]:
        failures.append('decode errors %s instead of [%s]' % (errors, CRC_MISMATCH))

    # decode a routed frame between direct frames and reject version 3
    unknown_version_frame = bytearray(encode(0, 5, 0x0100, b''))
    unknown_version_frame[2] = ROUTED_PROTOCOL_VERSION + 1
    stream = (encode(0, 4, 0x0100, b'') + golden_routed_frame + bytes(unknown_version_frame)
              + encode(0, 6, 0x0100, b'\x01'))
    results = Decoder().on_received_data(stream)
    routes = [value['route'] for kind, value in results if kind == 'frame']
    errors = [value for kind, value in results if kind == 'error']
    if routes != [None, (1, 3, DEFAULT_TIME_TO_LIVE), None]:
        failures.append('decoded routes %s instead of [None, (1, 3, 8), None]' % routes)
    if errors != [INVALID_VERSION]:
        failures.append('decode errors %s instead of [%s]' % (errors, INVALID_VERSION))

    # notify the user
    for failure in failures:
        print('self-test failed: %s' % failure)
    if failures:
        return 1
    print('self-test passed')
    return 0


def print_encoded_frame(arguments):
    """Print one frame as hex or write its raw bytes, return the exit code."""
    payload = bytes.fromhex(arguments.payload.replace(' ', ''))
    route = None
    if arguments.version == ROUTED_PROTOCOL_VERSION:
        route = (arguments.source, arguments.destination, arguments.ttl)
    frame = encode(arguments.flags, arguments.counter, arguments.type, payload, route)
    if arguments.binary:
        sys.stdout.buffer.write(frame)
        return 0
    print(' '.join('%02X' % byte for byte in frame))
    return 0


def print_decoded_capture(arguments):
    """Print every frame and decode error of a capture, return the exit code."""
    if arguments.capture == '-':
        data = sys.stdin.buffer.read()
    else:
        with open(arguments.capture, 'rb') as capture:
            data = capture.read()

    # print the results
    decoder = Decoder()
    for kind, value in decoder.on_received_data(data):
        if kind == 'frame':
            route_text = ''
            if value['route'] is not None:
                route_text = ' source=%u destination=%u ttl=%u' % value['route']
            print('FRAME counter=%-5u type=0x%04X flags=%-28s length=%-4u payload=%s%s' % (
                value['counter'], value['type'], get_flags_text(value['flags']), len(value['payload']),
                value['payload'].hex().upper(), route_text))
        else:
            print('ERROR %s' % value)
    print('SUMMARY bytes=%u discarded=%u trailing=%u' % (len(data), decoder.discarded_byte_counter,
                                                          len(decoder.buffer)))
    return 0


def main():
    """Run the command of the command line."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command')
    commands.required = True

    # encode command
    encode_parser = commands.add_parser('encode', help='print one frame as hex')
    encode_parser.add_argument('--flags', type=parse_integer, default=0, help='value of the FLAGS field')
    encode_parser.add_argument('--counter', type=parse_integer, default=0, help='value of the MESSAGE_COUNTER field')
    encode_parser.add_argument('--type', type=parse_integer, required=True, help='value of the MESSAGE_TYPE field')
    encode_parser.add_argument('--payload', default='', help='payload as hex, spaces allowed')
    encode_parser.add_argument('--binary', action='store_true', help='write the raw bytes to standard output')
    encode_parser.add_argument('--version', type=parse_integer, default=PROTOCOL_VERSION,
                               choices=sorted(HEADER_SIZES), help='1 for a direct frame, 2 for a routed frame')
    encode_parser.add_argument('--source', type=parse_integer, default=0, help='source address of a routed frame')
    encode_parser.add_argument('--destination', type=parse_integer, default=0,
                               help='destination address of a routed frame')
    encode_parser.add_argument('--ttl', type=parse_integer, default=DEFAULT_TIME_TO_LIVE,
                               help='time to live of a routed frame (default %d)' % DEFAULT_TIME_TO_LIVE)

    # decode command
    decode_parser = commands.add_parser('decode', help='decode a raw capture file')
    decode_parser.add_argument('capture', help='file name or - for standard input')

    # self-test command
    commands.add_parser('self-test', help='run the built-in checks')

    # run the command
    arguments = parser.parse_args()
    if arguments.command == 'self-test':
        return self_test()
    if arguments.command == 'encode':
        return print_encoded_frame(arguments)
    return print_decoded_capture(arguments)


if __name__ == '__main__':
    sys.exit(main())
