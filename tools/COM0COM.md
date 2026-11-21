# Virtual COM Port Pairs on Windows

Windows has no pseudo terminals, so `socat` of [SOCAT.md](SOCAT.md) has no Windows
version. Its place takes [com0com](https://sourceforge.net/projects/com0com/), a
null-modem emulator driver: each pair is two COM ports joined like a crossed cable.
Bytes written into one port come out of the other one, and the other way round. The
pairs stay until you remove them, so no process has to keep running.

## The pairs of the tools

| Pair | Ports | Used by |
|---|---|---|
| 1 | `COM10` with `COM11` | examples 01 and 24 (`tools/run_examples.ps1`), the late ping device of `tests/e2e/noisy_link_test.ps1`, the first half of the noisy link |
| 2 | `COM12` with `COM13` | the second half of the noisy link |

```
apps open:      COM10                                  COM13
                  │                                      │
              com0com pair 1                       com0com pair 2
                  │                                      │
internal:       COM11 ──── noise.py --duplex ────► COM12        (COM10 → COM13)
                COM11 ◄─── (second thread) ─────── COM12        (COM13 → COM10)
```

This is the wiring of SOCAT.md with com0com pairs in place of the `socat` bridges. A
COM port opens only once, so one `noise.py --duplex` holds both inner ports and carries
both directions, the return direction with the seed plus one. `PORTS` or `--ports`
of the scripts name other ports, for example when `COM10` is taken.

## Install com0com

1. Download the signed com0com 3.0.0.0:
   `https://sourceforge.net/projects/com0com/files/com0com/3.0.0.0/com0com-3.0.0.0-i386-and-x64-signed.zip/download`
2. Unpack it and run its x64 setup program as administrator; accept the driver of the
   publisher when Windows asks.
3. Open PowerShell as administrator in `C:\Program Files (x86)\com0com`.
4. Create the first pair: `./setupc.exe install PortName=COM10 PortName=COM11`
5. Create the second pair: `./setupc.exe install PortName=COM12 PortName=COM13`
6. List the pairs: `./setupc.exe list`
7. Check that Windows knows the ports: `[System.IO.Ports.SerialPort]::GetPortNames()`
   prints `COM10` to `COM13` among the others.

The `windows` job of `.github/workflows/ci.yml` installs com0com this way without
prompts, it trusts the publisher of the installer first. When the Device Manager shows
code 52 for the com0com ports, Windows did not accept the driver signature, which some
machines with Secure Boot do; then use two USB-serial adapters with crossed TX and RX
and a common GND instead of a pair.

## Use the pairs

1. Run example 01 on `COM10` and send it bytes through `COM11` (example 01 guide, steps 3 and 4).
2. Run the device simulator on one end and a command on the other one:
   `Start-Process build/sercom_device_sim.exe -ArgumentList 'COM11'`, then `build/sercom_cli.exe COM10 ping 3`.
3. Start a noisy link between `COM10` and `COM13` in a terminal of its own:
   `tools/noisy_link.ps1 --profile realistic --seed 1`. It prints `Noisy link ready` and runs
   until Ctrl+C; the device simulator then runs on `COM13` and the tools on `COM10`.
4. Run every example: `tools/run_examples.ps1 build`
5. Run the end to end test: `tests/e2e/noisy_link_test.ps1 build/sercom_cli.exe build/sercom_device_sim.exe`
6. Remove a pair when you no longer need it: `./setupc.exe remove 0` for the pair `CNCA0` with `CNCB0`
   of `setupc.exe list`.

`noise.py` and `tests/e2e/late_ping_device.py` open the COM ports through pyserial:
`python -m pip install pyserial`.

## Differences to socat pseudo terminals

- A COM port opens only once. A second program on the same port fails with
  `OPEN_FAILED`, which is why the noisy link needs `--duplex`.
- A pair has no hang-up. When one side closes, the other side sees no error and keeps
  waiting; on Linux the tools report `READ_FAILED` when the far end of a pseudo terminal
  closes, and open the port again once it is back.
- The baud rate is a setting of each side only, com0com passes the bytes at once
  unless the pair emulates the baud rate (`setupc.exe change CNCA0 EmuBR=yes`).
- Ctrl+C reaches a program only in the console it runs in; `tools/run_examples.ps1`
  sends it to the programs of its own console to stop example 01.
