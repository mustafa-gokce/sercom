# noise.py — Serial Protocol Noise Injector

`noise.py` sits between two serial ports (or PTYs) and forwards every byte it
receives, injecting configurable corruption on the way through: dropped bytes, flipped
bits, inserted garbage, duplicated bytes, delays, and error bursts.

It simulates **protocol-level corruption** — the stream your parser actually sees —
rather than UART electrical noise on the wire. Of all the modes, single bit flips come
closest to genuine electrical noise; drops, insertions, and bursts model lost
characters, glitching connectors, and bad cables.

## Data path

```
Sender ──> INPUT port ──> noise.py ──> OUTPUT port ──> Receiver
                             (noise)
```

Forwarding is **one-directional**: bytes are read from `INPUT` and written to `OUTPUT`.
For a full-duplex link, run a second instance for the return path — with the same
options for noise in both directions, or with none for a clean return channel.

## Usage

```bash
./noise.py INPUT OUTPUT [options]
```

`INPUT` and `OUTPUT` are serial device or PTY paths. Both are opened
`O_RDWR | O_NOCTTY`; data is read in chunks of up to 4096 bytes and processed
byte by byte.

The script does not configure the ports (no baud rate, no raw mode). Set the line
discipline before starting it — with PTYs created by `socat`, use `raw,echo=0`.

### Example: noisy virtual link

```bash
# Two PTY pairs; the applications open /tmp/appA and /tmp/appB
socat -d -d pty,raw,echo=0,link=/tmp/appA pty,raw,echo=0,link=/tmp/linkA &
socat -d -d pty,raw,echo=0,link=/tmp/appB pty,raw,echo=0,link=/tmp/linkB &

# Corrupt the A -> B direction
./noise.py /tmp/linkA /tmp/linkB --drop 0.01 --flip 0.01

# Return path B -> A (clean)
./noise.py /tmp/linkB /tmp/linkA
```

## Options

| Option | Default | Effect |
|--------|---------|--------|
| `--drop P` | `0.0` | Probability of silently discarding each byte |
| `--flip P` | `0.0` | Probability of corrupting each byte (1 to `--max-bit-flips` bits flipped) |
| `--max-bit-flips N` | `1` | Maximum number of bits flipped in one corrupted byte |
| `--insert P` | `0.0` | Probability of inserting one random byte after the current byte |
| `--duplicate P` | `0.0` | Probability of repeating the current byte |
| `--delay S` | `0.0` | Sleep `S` seconds before forwarding each received chunk |
| `--burst P` | `0.0` | Probability (per byte) of entering a corruption burst |
| `--burst-length N` | `10` | Number of bytes affected once a burst starts |

All probabilities are evaluated **per byte**, independently of each other. Per byte the
order is drop → flip → duplicate → insert, so a dropped byte cannot also be flipped,
duplicated, or trigger an insertion, and a duplicated byte is copied as forwarded
(i.e. after any bit flips).

## Noise types

### 1 · Byte drop — `--drop`

Randomly removes a received byte from the stream.

```
Original:  A5 01 10 20 30 7F
Noisy:     A5 01 10    30 7F
                   ^^ dropped
```

Exercises: missing payload bytes, broken frame length, CRC failure, handling of
incomplete frames, and parser resynchronization. One of the most important modes for
a framed protocol.

### 2 · Bit flip — `--flip`, `--max-bit-flips`

Flips one or more random bits in a byte.

```
Original:   0x35 = 0011 0101
Corrupted:  0x31 = 0011 0001
                         ^
```

Exercises: CRC checking, plus header, length, command, payload, and sequence-number
corruption. The closest of all modes to actual electrical noise.

Guideline: `--max-bit-flips 1` for relatively realistic corruption, `2` for stronger
testing, `4` for aggressive robustness/fuzz runs.

### 3 · Random byte insertion — `--insert`

Injects a completely random byte into the stream.

```
Original:  A5 01 10 20 30 7F
Noisy:     A5 01 10 F3 20 30 7F
                    ^^ inserted
```

Exercises the key property of an SOF-framed protocol: when garbage appears between
(or inside) frames, the receiver must skip it and lock onto the next valid SOF.

### 4 · Byte duplication — `--duplicate`

Repeats a byte that was actually sent.

```
Original:  A5 01 10 20 30
Noisy:     A5 01 10 20 20 30
                       ^^ duplicated
```

Different from insertion: the extra byte is valid-looking data, not random garbage.
Exercises parser state handling, length handling, CRC, duplicate commands, and stream
synchronization.

### 5 · Delay — `--delay`

Sleeps before forwarding. The delay is applied **once per received chunk** (up to
4096 bytes), not per byte — it approximates link latency rather than per-byte pacing.

Exercises: timeouts (inter-byte and per-frame), race conditions, blocking reads, and
the classic wrong assumption that one `read()` returns exactly one complete frame.

### 6 · Burst corruption — `--burst`, `--burst-length`

Real links don't only produce independent single-byte errors; a bad cable or connector
produces a *cluster* of them. Each byte has probability `P` of starting a burst; for
the next `--burst-length` bytes the base probabilities are amplified:

| During a burst | Multiplier |
|----------------|------------|
| `--drop`, `--flip`, `--insert` | ×10 (capped at 1.0) |
| `--duplicate` | ×5 (capped at 1.0) |

Two consequences worth knowing:

- `--burst` does nothing on its own — it amplifies the base probabilities, so at least
  one of them must be non-zero.
- A burst can span chunk boundaries; the burst counter persists across reads.

Exercises: multiple simultaneous errors, whole-frame loss, several consecutive damaged
frames, and recovery into a valid frame immediately after a damaged one.

## Test checklist for the framed protocol

For the frame layout `SOF … header … HEADER_CRC16 … payload … PAYLOAD_CRC32 … EOF`
(SOF `0xA5 0x3A`, EOF `0xC5 0x5A`), make sure at least these cases are covered:

1. Corrupted payload
2. Corrupted header
3. Corrupted length field
4. Corrupted CRC (header CRC16 and payload CRC32)
5. Dropped SOF
6. Dropped EOF
7. Garbage inserted before SOF
8. Garbage inserted inside a frame
9. Duplicated byte
10. Several consecutive bytes dropped
11. One entire frame corrupted
12. Several consecutive frames corrupted
13. A corrupted frame followed immediately by a valid frame

**Acceptance criterion:** after receiving a corrupted frame, the parser must
eventually find the next valid SOF and successfully decode the next valid frame.
Noise may cost individual frames; it must never wedge the stream.

## Recommended profiles

### Starting point — moderate, everything enabled

```bash
./noise.py input output \
    --drop 0.01 \
    --flip 0.01 \
    --insert 0.005 \
    --duplicate 0.002 \
    --max-bit-flips 2 \
    --delay 0.001 \
    --burst 0.001 \
    --burst-length 20
```

### Aggressive — robustness / fuzz testing

```bash
./noise.py input output \
    --drop 0.05 \
    --flip 0.05 \
    --insert 0.02 \
    --duplicate 0.01 \
    --max-bit-flips 4 \
    --delay 0.005 \
    --burst 0.01 \
    --burst-length 50
```

### Near-realistic — a plausibly noisy UART

```bash
./noise.py input output \
    --drop 0.001 \
    --flip 0.001 \
    --insert 0.0005 \
    --duplicate 0.0001 \
    --max-bit-flips 1 \
    --delay 0.0001 \
    --burst 0.0001 \
    --burst-length 5
```

## Behaviour notes

- One direction per instance; run two instances for full duplex.
- The input is polled with `select()` (1 s timeout); the tool exits when `read()` on
  `INPUT` returns end-of-file (e.g. the peer PTY closed).
- The RNG is not seeded, so runs are not reproducible. For repeatable regression
  tests, seed `random` (e.g. add a `--seed` option).
- No flow control and no termios handling — configure the ports yourself.
