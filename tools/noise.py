"""Forward a byte stream from one port to another and inject noise on the way.

    python3 tools/noise.py [INPUT] [OUTPUT] [--drop P] [--flip P] [--maximum-bit-flips N] [--insert P]
                           [--duplicate P] [--delay S] [--burst P] [--burst-length N] [--seed N]
                           [--duplex] [--baud N]

INPUT and OUTPUT are serial ports or pseudo terminals, - or an omitted argument stands for standard input or output.
A Windows COM port name such as COM11 opens the port through pyserial (python -m pip install pyserial).
With --duplex one process forwards both directions, which Windows needs because a COM port opens only once.
Every probability is evaluated per byte, tools/NOISE.md describes the noise types and the recommended profiles.
"""

import argparse
import os
import random
import re
import select
import sys
import threading
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

# baud rate of a COM port, a pseudo terminal has none
DEFAULT_BAUD_RATE = 115200

# name of a Windows COM port, with or without the device path prefix
COM_PORT_NAME = re.compile(r'^(\\\\\.\\)?COM[0-9]+$', re.IGNORECASE)


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
    parser.add_argument('--duplex', action='store_true',
                        help='forward OUTPUT back to INPUT too, the return direction takes the seed plus one')
    parser.add_argument('--baud', type=int, default=DEFAULT_BAUD_RATE,
                        help='baud rate of a COM port (default %d)' % DEFAULT_BAUD_RATE)
    return parser.parse_args()


class DescriptorPort:
    """A pseudo terminal, a serial device or a standard stream as a file descriptor."""

    def __init__(self, descriptor):
        self.descriptor = descriptor

    def read(self):
        """Return the next chunk, an empty chunk at the end of the stream and None after a quiet wait."""
        readable, _, _ = select.select([self.descriptor], [], [], POLL_TIMEOUT)
        if self.descriptor not in readable:
            return None
        return os.read(self.descriptor, CHUNK_SIZE)

    def write(self, data):
        """Write a chunk."""
        os.write(self.descriptor, data)


class ComPort:
    """A Windows COM port opened through pyserial, for example one end of a com0com pair."""

    def __init__(self, name, baud_rate):
        try:
            import serial
        except ImportError:
            sys.exit('COM ports need pyserial: python -m pip install pyserial')
        self.serial = serial
        self.port = serial.Serial(name, baud_rate, timeout=POLL_TIMEOUT)

    def read(self):
        """Return the waiting bytes, an empty chunk when the port fails and None after a quiet wait."""
        try:
            data = self.port.read(1)
            if not data:
                return None
            return data + self.port.read(self.port.in_waiting)
        except self.serial.SerialException:
            return b''

    def write(self, data):
        """Write a chunk."""
        self.port.write(data)


def open_port(path, standard_descriptor, baud_rate):
    """Open a port for reading and writing, - gives the standard descriptor and a COM port name pyserial."""
    if path == '-':
        return DescriptorPort(standard_descriptor)
    if COM_PORT_NAME.match(path):
        return ComPort(path, baud_rate)
    return DescriptorPort(os.open(path, os.O_RDWR | os.O_NOCTTY))


def corrupt_byte(generator, byte, bit_flips):
    """Flip the given number of random bits of one byte."""
    for _ in range(bit_flips):
        byte ^= 1 << generator.randint(0, BYTE_SIZE - 1)
    return byte


class NoiseInjector:
    """Noise of one direction with its own random generator, a burst lasts across chunks."""

    def __init__(self, arguments, seed):
        self.arguments = arguments
        self.generator = random.Random(seed)
        self.burst_remaining = 0

    def process_chunk(self, data):
        """Return the noisy copy of one received chunk."""
        output = bytearray()
        for byte in data:
            # start a burst or count the running one down
            if self.burst_remaining == 0 and self.generator.random() < self.arguments.burst:
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
            if self.generator.random() < drop:
                continue

            # flip bits of the byte
            if self.generator.random() < flip:
                byte = corrupt_byte(self.generator, byte,
                                    self.generator.randint(1, self.arguments.maximum_bit_flips))
            output.append(byte)

            # duplicate the byte
            if self.generator.random() < duplicate:
                output.append(byte)

            # insert a garbage byte
            if self.generator.random() < insert:
                output.append(self.generator.randint(0, MAXIMUM_BYTE_VALUE))
        return output


def forward(input_port, output_port, injector, delay):
    """Forward every chunk of the input port to the output port until the input closes."""
    while True:
        data = input_port.read()
        if data is None:
            continue
        if not data:
            break
        output = injector.process_chunk(data)
        if delay > 0:
            time.sleep(delay)
        if output:
            output_port.write(output)


def main():
    """Forward the input to the output until the input closes, in both directions with --duplex."""
    arguments = parse_arguments()

    # open the ports
    input_port = open_port(arguments.input, sys.stdin.fileno(), arguments.baud)
    output_port = open_port(arguments.output, sys.stdout.fileno(), arguments.baud)

    # forward the return direction with the next seed on its own thread
    if arguments.duplex:
        return_seed = None if arguments.seed is None else arguments.seed + 1
        return_injector = NoiseInjector(arguments, return_seed)
        return_thread = threading.Thread(target=forward, args=(output_port, input_port, return_injector,
                                                               arguments.delay), daemon=True)
        return_thread.start()

    # forward every chunk until the input closes
    forward(input_port, output_port, NoiseInjector(arguments, arguments.seed), arguments.delay)
    return 0


if __name__ == '__main__':
    sys.exit(main())
