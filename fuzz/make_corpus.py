"""Write the seed corpus of the frame decoder fuzzer.

    python3 fuzz/make_corpus.py [--corpus-directory DIRECTORY]

The first byte of every seed selects the chunk size of fuzz/fuzz_FrameDecoder.cpp, the rest is the decoded stream.
Run it again whenever the frame layout changes, a stale seed only reaches the CRC check.
"""

import argparse
import os
import sys

# directory of the frame reference tool
TOOLS_DIRECTORY = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'tools')

# frame flag types of the seeds
FRAME_FLAG_ACK_REQUEST = 0x01
FRAME_FLAG_ACK = 0x02
FRAME_FLAG_ROUTE_CAPABLE = 0x10

# addresses of the routed seeds
HOST_ADDRESS = 0x01
NODE_ADDRESS = 0x03

# message types of the seeds
PING_REQUEST = 0x0001
SET_LED_REQUEST = 0x0100

# chunk size selectors of the seeds, the fuzzer adds one
PING_CHUNK_SELECTOR = 7
TWO_FRAMES_CHUNK_SELECTOR = 2
ROUTED_PING_CHUNK_SELECTOR = 5
LARGEST_ROUTED_FRAME_CHUNK_SELECTOR = 63
MIXED_FRAMES_CHUNK_SELECTOR = 3

# noise bytes between the mixed frames, a stray sync word and a version above the routed frames
MIXED_NOISE = bytes([0x00, 0xA5, 0xC3, 0x03, 0xFF])


def get_seeds(sercom_frame):
    """Return the seed bytes by file name."""
    seeds = {}

    # golden frame of docs/PROTOCOL.md
    ping_request = sercom_frame.encode(FRAME_FLAG_ACK_REQUEST, 7, PING_REQUEST, bytes.fromhex('E8030000'))
    seeds['ping.bin'] = bytes([PING_CHUNK_SELECTOR]) + ping_request

    # two frames with a stray sync byte between them
    set_led_request = sercom_frame.encode(FRAME_FLAG_ACK_REQUEST, 1, SET_LED_REQUEST, bytes([1]))
    acknowledgement = sercom_frame.encode(FRAME_FLAG_ACK, 1, SET_LED_REQUEST)
    stray_sync_byte = bytes([sercom_frame.SYNC_BYTE_1])
    seeds['two_frames.bin'] = (bytes([TWO_FRAMES_CHUNK_SELECTOR]) + set_led_request + stray_sync_byte
                               + acknowledgement)

    # golden routed frame of docs/PROTOCOL.md section 8.11
    route = (HOST_ADDRESS, NODE_ADDRESS, sercom_frame.DEFAULT_TIME_TO_LIVE)
    routed_flags = FRAME_FLAG_ACK_REQUEST | FRAME_FLAG_ROUTE_CAPABLE
    routed_ping_request = sercom_frame.encode(routed_flags, 7, PING_REQUEST, bytes.fromhex('E8030000'), route)
    seeds['routed_ping.bin'] = bytes([ROUTED_PING_CHUNK_SELECTOR]) + routed_ping_request

    # largest routed frame of the reference build
    largest_payload = bytes(range(sercom_frame.MAXIMUM_PAYLOAD_SIZE))
    largest_routed_frame = sercom_frame.encode(routed_flags, 1, SET_LED_REQUEST, largest_payload, route)
    seeds['largest_routed_frame.bin'] = bytes([LARGEST_ROUTED_FRAME_CHUNK_SELECTOR]) + largest_routed_frame

    # routed and direct frames mixed with noise
    routed_acknowledgement = sercom_frame.encode(FRAME_FLAG_ACK | FRAME_FLAG_ROUTE_CAPABLE, 7, PING_REQUEST)
    seeds['mixed_frames.bin'] = (bytes([MIXED_FRAMES_CHUNK_SELECTOR]) + MIXED_NOISE + set_led_request
                                 + routed_ping_request + MIXED_NOISE + routed_acknowledgement + MIXED_NOISE)
    return seeds


def main():
    """Write every seed and print its bytes."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--corpus-directory', default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                                   'corpus'),
                        help='directory of the seed files (default fuzz/corpus)')
    arguments = parser.parse_args()

    # load the frame reference tool
    sys.path.insert(0, TOOLS_DIRECTORY)
    import sercom_frame

    # write the seeds
    os.makedirs(arguments.corpus_directory, exist_ok=True)
    seeds = get_seeds(sercom_frame)
    for name in sorted(seeds):
        with open(os.path.join(arguments.corpus_directory, name), 'wb') as seed_file:
            seed_file.write(seeds[name])
        print('%s: %s' % (name, ' '.join('%02X' % byte for byte in seeds[name])))
    return 0


if __name__ == '__main__':
    sys.exit(main())
