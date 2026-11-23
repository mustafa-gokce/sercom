# 27 Chain in One Process

This example runs a daisy chain of three nodes in one program, joined in memory: the host client with the host
address 1, node 2 with two ports and node 3 at the end. The host discovers the chain, pings node 3 and reads its device
info through node 2, then sends two requests that fail on the way and reads the router statistics of node 2. No serial
port and no board are needed.

## What you will learn

- How a `DeviceApplication` with a secondary transport becomes a router that passes frames on to the next node.
- How the `DeviceClient` sends its requests to a destination address, and how a ping to the broadcast address reaches
  every node.
- Why the first hop's acknowledgement does not mean that the destination has a request, and how a route failure report
  of node 2 turns into the failure of the request at the host.
- What happens to a request whose time to live runs out.

## Before you start

1. Watch lectures L113 "Addressed applications: replies, reports and the --to option" and L112 "The router:
   forwarding, TTL, broadcast and failure reports" first, and run example 21 "Device in One Process", the same idea
   with one link.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_27_chain_in_one_process`.

To build only this example, run `cmake --build build --target sercom_example_27_chain_in_one_process`.

## Step 1: Build the chain

Open `examples/27_chain_in_one_process.cpp` and find the comment `// join the host, node 2 with two ports and node 3 at
the end in memory`. Four mock transports carry the bytes: the host to node 2, node 2 up to the host, node 2 down to
node 3 and node 3 up to node 2. Node 2 is built with the constructor that takes a secondary transport, here
`&nodeSecondaryTransport`; node 3 has no secondary transport and is an end node. `getRouterConfiguration` gives each
node its address and the time to live of its own frames.

Find `// create the device client of the host with the host address`. The client gets the host address 1, without an
address it could only send direct frames to its neighbour. `exchangeData` hands the waiting bytes along the chain, one
millisecond per hand over, until no byte waits.

## Step 2: Run the program

```
build/sercom_example_27_chain_in_one_process
```

You see:

```
host pings every node of the chain
  node 2 answered after 2 ms
  node 3 answered after 6 ms
host pings node 3 and asks it for its device info
  node 3 answered after 5 ms
  device example-node-3, protocol version 2, node address 3
host pings node 9 that is not in the chain
  request #3 PING_REQUEST failed: REJECTED, UNREACHABLE
host pings node 3 with a time to live of 1
  request #0 PING_REQUEST failed: REJECTED, EXPIRED
router statistics of node 2
  local frames                          0
  forwarded frames                      7
  broadcast frames                      1
  expired frames                        1
  unreachable frames                    0
  failed forwards                       1
  route failure reports sent            1
```

## Step 3: Discover the chain

Find `// discover the chain with a ping request to every node`. `setDestinationAddress(BROADCAST_ADDRESS)` sends the
ping to every node. Node 2 delivers it and passes a copy with one link less to live to node 3, both nodes answer the
host. The ping stays pending in the client after the first answer, so the round trip time of every node is measured:
node 3 is farther away and answers after 6 ms instead of 2 ms. `getSourceAddress()` of each ping response names the
node.

## Step 4: Reach node 3 through node 2

Find `// ping node 3 and ask it for its device info`. With the destination 3 both requests travel as routed frames.
Node 2 does not deliver them, it forwards them on its secondary port and answers the host at once. Node 3 answers to
the source of each request, the host, and node 2 passes the answers up. Node 3 has an address, so its device info
response carries the two appended fields of version 2: protocol version 2 and node address 3. A device without an
address sends the device info of version 1, and the fields read as version 1 and address 0.

## Step 5: Two requests that fail on the way

Find `// ping node 9 that is not in the chain`. Node 2 takes the ping and acknowledges it, so the host only knows that
node 2 holds it. Node 3 has no other port and rejects the frame with the status `UNREACHABLE`. Node 2 then sends a
route failure report back to the host, the source of the ping, with the first payload bytes of the ping as a quote.
The client finds the request that the report quotes and reports `REJECTED, UNREACHABLE` for it.

Find `// ping node 3 with one link to live from a second device client`. The second client sends its frames with a
time to live of 1. Node 2 would have to forward the frame with no link left, so it drops it and answers at once with
the status `EXPIRED`: the host's link gets the NACK directly, no report is needed.

## Step 6: Read the router statistics

Find `// print the router statistics of node 2`. Node 2 delivered no frame of its own (local frames 0) but one
broadcast, forwarded seven frames (the broadcast copy, three requests down and three answers up), dropped the
expired frame, saw one forwarded frame fail at the next hop and sent one report. Find `// host must have the answers
of both nodes, the device info and both failures`: the program exits with 0 when the host got everything it
expected.

## Try it yourself

1. Give node 3 no address: replace `getRouterConfiguration(endNodeAddress, Settings::DEFAULT_TIME_TO_LIVE)` by
   `Router::RouterConfiguration()`. Node 3 then drops every routed frame, the broadcast reaches node 2 only and the
   requests for node 3 fail with `UNREACHABLE`. The program exits with 1.
2. Change the time to live of the second client from 1 to 2. The ping reaches node 3 and the line `failed: REJECTED,
   EXPIRED` disappears, because a node delivers a frame for itself whatever its time to live.
3. Run `tests/e2e/chain_test.sh build/sercom_cli build/sercom_device_sim` to see the same steps with three device
   simulators over pseudo terminals and with noise between two of them.

## Where this is used

- `core/include/Router.h` and `core/src/Router.cpp`: the rules of the routed frames, the forwarding and the reports.
- `app/include/DeviceApplication.h` and `app/src/DeviceApplication.cpp`: the device on a router with one or two ports,
  the answers to the source and the appended device info.
- `app/include/DeviceClient.h` and `app/src/DeviceClient.cpp`: the destination address, the broadcast ping and the
  mapping of a report to its request.
- `host/src/CliApplication.cpp`: `sercom_cli --to`, `--ttl` and `discover` do the same on a serial port.
- `docs/PROTOCOL.md` section 8: the specification of the routed frames.
- Lectures: L112 "The router: forwarding, TTL, broadcast and failure reports" and L113 "Addressed applications:
  replies, reports and the --to option".
