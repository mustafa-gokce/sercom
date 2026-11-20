"""Forward a byte stream from one port to another and inject noise on the way.

    python3 tools/noise.py [INPUT] [OUTPUT] [--drop P] [--flip P] [--maximum-bit-flips N] [--insert P]
                           [--duplicate P] [--delay S] [--burst P] [--burst-length N] [--seed N]

INPUT and OUTPUT are serial ports or pseudo terminals, - or an omitted argument stands for standard input or output.
Every probability is evaluated per byte, tools/NOISE.md describes the noise types and the recommended profiles.
"""

import argparse
import os
import random
import select
import sys
import time

# number of bytes read from the input at once
CHUNK_SIZE = 4096

# seconds one wait for input may take
POLL_TIMEOUT = 1.0

# number of bits in one byte
BYTE_SIZE = 8

# largest value of one byte
MAXIMUM_BYTE_VALUE = 255

# multipliers of the base probabilities during a burst
BURST_MULTIPLIER = 10.0
BURST_DUPLICATE_MULTIPLIER = 5.0


def parse_arguments():
    """Parse the command line."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('input', nargs='?', default='-', help='input port path, - or omitted for standard input')
    parser.add_argument('output', nargs='?', default='-', help='output port path, - or omitted for standard output')
    parser.add_argument('--drop', type=float, default=0.0, help='probability of dropping each byte')
    parser.add_argument('--flip', type=float, default=0.0, help='probability of corrupting each byte')
    parser.add_argument('--maximum-bit-flips', type=int, default=1,
                        help='maximum number of bits flipped in a corrupted byte')
    parser.add_argument('--insert', type=float, default=0.0, help='probability of inserting a random byte')
    parser.add_argument('--duplicate', type=float, default=0.0, help='probability of duplicating each byte')
    parser.add_argument('--delay', type=float, default=0.0, help='delay in seconds for each received chunk')
    parser.add_argument('--burst', type=float, default=0.0, help='probability of starting a corruption burst')
    parser.add_argument('--burst-length', type=int, default=10,
                        help='number of bytes affected by a corruption burst')
    parser.add_argument('--seed', type=int, default=None, help='seed of the random generator for reproducible runs')
    return parser.parse_args()


def open_port(path, standard_descriptor):
    """Open a port for reading and writing, - gives the standard descriptor."""
    if path == '-':
        return standard_descriptor
    return os.open(path, os.O_RDWR | os.O_NOCTTY)


def corrupt_byte(byte, bit_flips):
    """Flip the given number of random bits of one byte."""
    for _ in range(bit_flips):
        byte ^= 1 << random.randint(0, BYTE_SIZE - 1)
    return byte


class NoiseInjector:
    """Noise of one direction, a burst lasts across chunks."""

    def __init__(self, arguments):
        self.arguments = arguments
        self.burst_remaining = 0

    def process_chunk(self, data):
        """Return the noisy copy of one received chunk."""
        output = bytearray()
        for byte in data:
            # start a burst or count the running one down
            if self.burst_remaining == 0 and random.random() < self.arguments.burst:
                self.burst_remaining = self.arguments.burst_length
            in_burst = self.burst_remaining > 0
            if in_burst:
                self.burst_remaining -= 1

            # amplify the base probabilities during a burst
            drop = self.arguments.drop
            flip = self.arguments.flip
            insert = self.arguments.insert
            duplicate = self.arguments.duplicate
            if in_burst:
                drop = min(1.0, drop * BURST_MULTIPLIER)
                flip = min(1.0, flip * BURST_MULTIPLIER)
                insert = min(1.0, insert * BURST_MULTIPLIER)
                duplicate = min(1.0, duplicate * BURST_DUPLICATE_MULTIPLIER)

            # drop the byte
            if random.random() < drop:
                continue

            # flip bits of the byte
            if random.random() < flip:
                byte = corrupt_byte(byte, random.randint(1, self.arguments.maximum_bit_flips))
            output.append(byte)

            # duplicate the byte
            if random.random() < duplicate:
                output.append(byte)

            # insert a garbage byte
            if random.random() < insert:
                output.append(random.randint(0, MAXIMUM_BYTE_VALUE))
        return output


def main():
    """Forward the input to the output until the input closes."""
    arguments = parse_arguments()

    # seed the noise, each direction takes its own seed
    if arguments.seed is not None:
        random.seed(arguments.seed)

    # open the ports
    input_descriptor = open_port(arguments.input, sys.stdin.fileno())
    output_descriptor = open_port(arguments.output, sys.stdout.fileno())

    # forward every chunk until the input closes
    injector = NoiseInjector(arguments)
    while True:
        readable, _, _ = select.select([input_descriptor], [], [], POLL_TIMEOUT)
        if input_descriptor not in readable:
            continue
        data = os.read(input_descriptor, CHUNK_SIZE)
        if not data:
            break
        output = injector.process_chunk(data)
        if arguments.delay > 0:
            time.sleep(arguments.delay)
        if output:
            os.write(output_descriptor, output)
    return 0


if __name__ == '__main__':
    sys.exit(main())
