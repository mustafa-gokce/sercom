#!/usr/bin/env python3

import argparse
import os
import random
import select
import time


def corrupt_byte(byte: int, bit_flips: int) -> int:
    for _ in range(bit_flips):
        byte ^= 1 << random.randint(0, 7)
    return byte


def process(data: bytes, args) -> bytes:
    output = bytearray()

    for byte in data:

        # random byte drop
        if random.random() < args.drop:
            continue

        # random bit corruption
        if random.random() < args.flip:
            byte = corrupt_byte(byte, random.randint(1, args.max_bit_flips))

        output.append(byte)

        # duplicate byte
        if random.random() < args.duplicate:
            output.append(byte)

        # insert random byte
        if random.random() < args.insert:
            output.append(random.randint(0, 255))

    return bytes(output)


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("input")
    parser.add_argument("output")

    parser.add_argument(
        "--drop",
        type=float,
        default=0.0,
        help="Probability of dropping each byte"
    )

    parser.add_argument(
        "--flip",
        type=float,
        default=0.0,
        help="Probability of corrupting each byte"
    )

    parser.add_argument(
        "--insert",
        type=float,
        default=0.0,
        help="Probability of inserting a random byte"
    )

    parser.add_argument(
        "--duplicate",
        type=float,
        default=0.0,
        help="Probability of duplicating each byte"
    )

    parser.add_argument(
        "--max-bit-flips",
        type=int,
        default=1,
        help="Maximum number of bits flipped in a corrupted byte"
    )

    parser.add_argument(
        "--delay",
        type=float,
        default=0.0,
        help="Delay in seconds for each received chunk"
    )

    parser.add_argument(
        "--burst",
        type=float,
        default=0.0,
        help="Probability of starting a corruption burst"
    )

    parser.add_argument(
        "--burst-length",
        type=int,
        default=10,
        help="Number of bytes affected by a corruption burst"
    )

    args = parser.parse_args()

    input_fd = os.open(args.input, os.O_RDWR | os.O_NOCTTY)
    output_fd = os.open(args.output, os.O_RDWR | os.O_NOCTTY)

    burst_remaining = 0

    while True:
        readable, _, _ = select.select([input_fd], [], [], 1.0)

        if input_fd not in readable:
            continue

        data = os.read(input_fd, 4096)

        if not data:
            break

        output = bytearray()

        for byte in data:

            if burst_remaining == 0 and random.random() < args.burst:
                burst_remaining = args.burst_length

            in_burst = burst_remaining > 0

            if in_burst:
                burst_remaining -= 1

            drop = args.drop
            flip = args.flip
            insert = args.insert
            duplicate = args.duplicate

            if in_burst:
                drop = min(1.0, drop * 10.0)
                flip = min(1.0, flip * 10.0)
                insert = min(1.0, insert * 10.0)
                duplicate = min(1.0, duplicate * 5.0)

            # drop
            if random.random() < drop:
                continue

            # flip bits
            if random.random() < flip:
                byte = corrupt_byte(
                    byte,
                    random.randint(1, args.max_bit_flips)
                )

            output.append(byte)

            # duplicate
            if random.random() < duplicate:
                output.append(byte)

            # insert garbage
            if random.random() < insert:
                output.append(random.randint(0, 255))

        if args.delay > 0:
            time.sleep(args.delay)

        if output:
            os.write(output_fd, output)


if __name__ == "__main__":
    main()
