# Full-Duplex Noisy Virtual Serial Link

This wiring creates a virtual serial connection with an **independent noise injector in
each direction**. Applications only ever see the two end ports:

```
/tmp/ttyV1   ← application A opens this
/tmp/ttyV2   ← application B opens this
```

Everything in between — the internal PTYs and the two `noise.py` instances — is
plumbing the applications never touch.

## Prerequisites

- `socat`
- `noise.py` operating as a **one-way stdin → stdout filter**: it reads raw bytes from
  standard input, applies the noise flags, and writes the result to standard output.
  The two-port form documented in `serial_noise.md` (`serial_noise.py INPUT OUTPUT`)
  needs a small adaptation for this — e.g. fall back to stdin/stdout when the port
  arguments are omitted — or use the two-port wiring from that document instead.

## Topology

```
apps open:      /tmp/ttyV1                          /tmp/ttyV2
                     │                                   │
                 socat #1                            socat #2
                     │                                   │
internal:      /tmp/noiseA ──── noise.py #1 ────► /tmp/noiseB      (V1 → V2)
               /tmp/noiseA ◄─── noise.py #2 ───── /tmp/noiseB      (V2 → V1)
```

- `socat #1` bridges the app-facing `/tmp/ttyV1` to the internal `/tmp/noiseA`.
- `socat #2` bridges the app-facing `/tmp/ttyV2` to the internal `/tmp/noiseB`.
- `noise.py #1` reads `/tmp/noiseA` and writes `/tmp/noiseB`: it carries (and
  corrupts) the V1 → V2 direction.
- `noise.py #2` reads `/tmp/noiseB` and writes `/tmp/noiseA`: it carries (and
  corrupts) the V2 → V1 direction.

The two directions never mix: bytes written into a PTY slave go only to its master
(the bridging `socat`), so each filter sees exactly one direction of traffic.

## Setup

```bash
#!/usr/bin/env bash
set -euo pipefail

NOISE_ARGS=(
    --drop 0.01
    --flip 0.01
    --insert 0.005
    --duplicate 0.002
    --max-bit-flips 2
    --delay 0.001
    --burst 0.001
    --burst-length 20
)

rm -f /tmp/ttyV1 /tmp/ttyV2 /tmp/noiseA /tmp/noiseB

pids=()

# App-facing PTY bridged to an internal PTY, one bridge per side
socat PTY,raw,echo=0,link=/tmp/ttyV1 PTY,raw,echo=0,link=/tmp/noiseA & pids+=($!)
socat PTY,raw,echo=0,link=/tmp/ttyV2 PTY,raw,echo=0,link=/tmp/noiseB & pids+=($!)

sleep 0.2   # let socat create the /tmp links before we open them

# One independent noise instance per direction
python3 -u noise.py "${NOISE_ARGS[@]}" < /tmp/noiseA > /tmp/noiseB & pids+=($!)
python3 -u noise.py "${NOISE_ARGS[@]}" < /tmp/noiseB > /tmp/noiseA & pids+=($!)

trap 'kill "${pids[@]}" 2>/dev/null' EXIT

echo "Noisy link ready: /tmp/ttyV1 <-> /tmp/ttyV2  (Ctrl-C to tear down)"
wait
```

Both `noise.py` instances receive the **same arguments**, which makes the channel
symmetric — but their random decisions are still completely independent, so the two
directions fail differently. Passing different flags per instance models an
asymmetric link (e.g. clean command uplink, noisy telemetry downlink).

### One-liner variant

The same wiring as a single copy-paste block:

```bash
rm -f /tmp/ttyV1 /tmp/ttyV2 /tmp/noiseA /tmp/noiseB; \
socat PTY,raw,echo=0,link=/tmp/ttyV1 PTY,raw,echo=0,link=/tmp/noiseA & \
socat PTY,raw,echo=0,link=/tmp/ttyV2 PTY,raw,echo=0,link=/tmp/noiseB & \
sleep 0.2; \
socat -u /tmp/noiseA SYSTEM:"python3 -u noise.py --drop 0.01 --flip 0.01 --insert 0.005 --duplicate 0.002 --max-bit-flips 2 --delay 0.001 --burst 0.001 --burst-length 20 > /tmp/noiseB" & \
socat -u /tmp/noiseB SYSTEM:"python3 -u noise.py --drop 0.01 --flip 0.01 --insert 0.005 --duplicate 0.002 --max-bit-flips 2 --delay 0.001 --burst 0.001 --burst-length 20 > /tmp/noiseA"
```

Each filter stage is `socat -u SOURCE SYSTEM:"…"`: socat reads the source PTY into
`noise.py`'s stdin, and the shell redirect inside the `SYSTEM` string
(`> /tmp/noiseB`) delivers the filtered output to the destination PTY. The
destination cannot be a third socat argument — socat takes exactly two addresses —
which is the one fix here relative to the seemingly obvious
`socat /tmp/noiseA SYSTEM:"…" /tmp/noiseB` form, which socat rejects.

`Ctrl-C` on the one-liner stops only the foreground (last) command; the three
backgrounded processes keep running. Stop everything with:

```bash
pkill -f noise.py; pkill -f 'link=/tmp/ttyV'
```

## Smoke test

```bash
# Terminal 1 — receive
cat /tmp/ttyV2

# Terminal 2 — send a byte stream through the noisy path
while :; do printf 'ping %s\n' "$(date +%T)" > /tmp/ttyV1; sleep 1; done
```

Most lines arrive intact; every so often one shows a mangled, missing, duplicated, or
extra character. Swap the two terminals to exercise the other direction.

## Watching the wire

To hex-dump the traffic live from a third terminal, start one of the bridge `socat`
processes there in the foreground with `-x` instead of backgrounding it:

```bash
socat -x PTY,raw,echo=0,link=/tmp/ttyV1 PTY,raw,echo=0,link=/tmp/noiseA
```

Note the vantage point: on `socat #1` you see V1 → V2 traffic *before* noise and
V2 → V1 traffic *after* noise; on `socat #2` it is the other way around.

To record one direction for later inspection instead, tee the filter output:

```bash
python3 -u noise.py "${NOISE_ARGS[@]}" < /tmp/noiseA | tee /tmp/v1_to_v2.bin > /tmp/noiseB &
# afterwards:
xxd /tmp/v1_to_v2.bin
```

## Teardown

`Ctrl-C` the setup script — the `trap` kills all four processes. `socat` removes its
`/tmp` symlinks on a clean exit; the `rm -f` at the top of the script cleans up
leftovers from crashes.

## Notes

- A single `socat` accepts **exactly two** addresses, so one process cannot form the
  chain `noiseA → filter → noiseB` by itself. The third leg is therefore always a
  shell redirection: plain `< in > out` in the setup script, or the `> /tmp/noiseB`
  inside the `SYSTEM` string in the one-liner.
- `raw,echo=0` on every PTY is essential. Without it the line discipline echoes and
  rewrites bytes, which destroys binary protocol traffic on its own.
- `--delay` applies per forwarded chunk in each direction, so the round-trip latency
  floor is roughly twice the configured delay.
- The internal `/tmp/noiseA` and `/tmp/noiseB` PTYs must not be opened by
  applications; they belong to the noise plumbing.
- Startup order matters only in that the `socat` bridges must exist before the
  filters open the internal PTYs — hence the `sleep 0.2`.

See also: `serial_noise.md` for the noise flags, what each mode tests, and the
recommended profiles.
