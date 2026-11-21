# noise.py — Serial Protocol Noise Injector

`noise.py` sits between two serial ports (or PTYs) and forwards every byte it
receives, injecting configurable corruption on the way through: dropped bytes, flipped
bits, inserted garbage, duplicated bytes, delays, and error bursts.

It simulates **protocol-level corruption** — the stream your parser actually sees —
rather than UART electrical noise on the wire. Single bit flips model electrical noise;
drops, insertions, and bursts model lost characters, glitching connectors, and bad
cables.

## Data path

```
Sender ──> INPUT port ──> noise.py ──> OUTPUT port ──> Receiver
                             (noise)
```

Forwarding is **one-directional**: bytes are read from `INPUT` and written to `OUTPUT`.
For a full-duplex link, run a second instance for the return path — with the same
options for noise in both directions, or with none for a clean return channel — or
pass `--duplex`: one instance then also forwards `OUTPUT` back to `INPUT` with the same
options and the seed plus one, which is what two instances with `SEED` and `SEED + 1`
do. Windows needs `--duplex`, because a COM port opens only once.

## Usage

```bash
python3 tools/noise.py [INPUT] [OUTPUT] [options]
```

`INPUT` and `OUTPUT` are serial device or PTY paths; `-` or an omitted argument means
standard input or standard output, so the tool also works as a filter in a pipe. Paths are opened
`O_RDWR | O_NOCTTY`; data is read in chunks of up to 4096 bytes and processed
byte by byte. A Windows COM port name such as `COM11` or `\\.\COM11` opens the port
through pyserial (`python -m pip install pyserial`) at `--baud` bits per second;
`COM0COM.md` shows the com0com pairs that take the place of the PTYs.

The script does not configure the ports (no baud rate, no raw mode). Set the line
discipline before starting it — with PTYs created by `socat`, use `raw,echo=0`.

### Example: noisy virtual link
1. Go to the source directory.
2. Create the PTY pair of application A, which opens `/tmp/appA`: `socat -d -d pty,raw,echo=0,link=/tmp/appA pty,raw,echo=0,link=/tmp/linkA &`
3. Create the PTY pair of application B, which opens `/tmp/appB`: `socat -d -d pty,raw,echo=0,link=/tmp/appB pty,raw,echo=0,link=/tmp/linkB &`
4. Corrupt the A -> B direction: `python3 tools/noise.py /tmp/linkA /tmp/linkB --drop 0.01 --flip 0.01 &`
5. Forward the B -> A direction without noise: `python3 tools/noise.py /tmp/linkB /tmp/linkA`

## Options

| Option | Default | Effect |
|--------|---------|--------|
| `--drop P` | `0.0` | Probability of silently discarding each byte |
| `--flip P` | `0.0` | Probability of corrupting each byte (1 to `--maximum-bit-flips` bits flipped) |
| `--maximum-bit-flips N` | `1` | Maximum number of bits flipped in one corrupted byte |
| `--insert P` | `0.0` | Probability of inserting one random byte after the current byte |
| `--duplicate P` | `0.0` | Probability of repeating the current byte |
| `--delay S` | `0.0` | Sleep `S` seconds before forwarding each received chunk |
| `--burst P` | `0.0` | Probability (per byte) of entering a corruption burst |
| `--burst-length N` | `10` | Number of bytes affected once a burst starts |
| `--seed N` | none | Seed the random generator so a run can be replayed exactly |
| `--duplex` | off | Also forward `OUTPUT` to `INPUT`, the return direction takes the seed plus one |
| `--baud N` | `115200` | Baud rate of a COM port; PTYs and standard streams ignore it |

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

### 2 · Bit flip — `--flip`, `--maximum-bit-flips`

Flips one or more random bits in a byte.

```
Original:   0x35 = 0011 0101
Corrupted:  0x31 = 0011 0001
                         ^
```

Exercises: CRC checking, plus header, length, command, payload, and sequence-number
corruption. Of all the modes it models electrical noise on the wire.

Guideline: `--maximum-bit-flips 1` for relatively realistic corruption, `2` for stronger
testing, `4` for aggressive robustness/fuzz runs.

### 3 · Random byte insertion — `--insert`

Injects a completely random byte into the stream.

```
Original:  A5 01 10 20 30 7F
Noisy:     A5 01 10 F3 20 30 7F
                    ^^ inserted
```

Exercises the key property of a sync-word framed protocol: when garbage appears
between (or inside) frames, the receiver must skip it and lock onto the next valid
sync word.

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

For the sercom frame `A5 C3 | VERSION | FLAGS | MESSAGE_COUNTER | MESSAGE_TYPE |
PAYLOAD_LENGTH | PAYLOAD | CRC32` (see `docs/PROTOCOL.md`), make sure at least these
cases are covered:

1. Corrupted payload
2. Corrupted header fields
3. Corrupted length field, shorter and longer than the real payload
4. Corrupted CRC
5. Dropped first or second sync byte
6. Truncated frame followed by a valid frame
7. Garbage inserted before the sync word
8. Garbage inserted inside a frame
9. Duplicated byte
10. Several consecutive bytes dropped
11. One entire frame corrupted
12. Several consecutive frames corrupted
13. A corrupted frame followed immediately by a valid frame

Every case is automated in `tests/test_NoiseChecklist.cpp`
(`build/sercom_test "[NoiseChecklist]"` from the source directory).

**Acceptance criterion:** after receiving a corrupted frame, the decoder must
eventually find the next valid sync word and successfully decode the next valid frame.
Noise may cost individual frames; it must never wedge the stream.

## Recommended profiles

`tools/noisy_link.sh --profile PROFILE` runs both directions with one of these
profiles; its `clean` profile forwards without noise.

### `moderate` — the starting point, everything enabled

```bash
python3 tools/noise.py input output \
    --drop 0.01 \
    --flip 0.01 \
    --insert 0.005 \
    --duplicate 0.002 \
    --maximum-bit-flips 2 \
    --delay 0.001 \
    --burst 0.001 \
    --burst-length 20
```

### `aggressive` — robustness and fuzz testing

```bash
python3 tools/noise.py input output \
    --drop 0.05 \
    --flip 0.05 \
    --insert 0.02 \
    --duplicate 0.01 \
    --maximum-bit-flips 4 \
    --delay 0.005 \
    --burst 0.01 \
    --burst-length 50
```

### `realistic` — a plausibly noisy UART

```bash
python3 tools/noise.py input output \
    --drop 0.001 \
    --flip 0.001 \
    --insert 0.0005 \
    --duplicate 0.0001 \
    --maximum-bit-flips 1 \
    --delay 0.0001 \
    --burst 0.0001 \
    --burst-length 5
```

## Behaviour notes

- One direction per instance, two with `--duplex` on two threads.
- The input is polled with `select()` (1 s timeout); the tool exits when `read()` on
  `INPUT` returns end-of-file (e.g. the peer PTY closed). A COM port waits up to 1 s for
  its first byte and takes every waiting byte with it; the tool exits when pyserial
  reports a failed port.
- Each direction has its own random generator, so the same seed gives the same noise
  with one instance per direction and with `--duplex`.
- Without `--seed` the RNG is seeded from the system, so runs are not reproducible.
  Pass `--seed N` for repeatable regression tests, and use a different seed for each
  direction so the two directions do not fail in lockstep.
- No flow control and no termios handling — configure the ports yourself.
