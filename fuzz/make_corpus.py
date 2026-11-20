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

# message types of the seeds
PING_REQUEST = 0x0001
SET_LED_REQUEST = 0x0100

# chunk size selectors of the seeds, the fuzzer adds one
PING_CHUNK_SELECTOR = 7
TWO_FRAMES_CHUNK_SELECTOR = 2


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
