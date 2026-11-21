"""Give one serial port of QEMU a pseudo terminal that behaves like the USB-serial bridge of a board.

    python3 tools/uart_bridge.py SOCKET LINK [--retry-delay SECONDS]

QEMU serves the serial port on the unix socket SOCKET, LINK becomes a symbolic link to a new pseudo terminal.
QEMU's own pty backend notices a program that opens its pseudo terminal only about once a second and holds the bytes
of that second back, so the first request of every host command would go out again as a retransmission.
This bridge owns the pseudo terminal and passes every byte at once in both directions.
Bytes of the chip are dropped while no program holds the pseudo terminal open, as a USB bridge without a reader does.
The bridge connects to the socket again when QEMU restarts, so the pseudo terminal stays like the port of a board.
"""

import argparse
import errno
import os
import select
import socket
import sys
import time
import tty

# number of bytes read at once
CHUNK_SIZE = 4096

# milliseconds one wait for bytes may take while a program holds the pseudo terminal open
OPEN_POLL_TIMEOUT = 10

# milliseconds one wait for bytes may take while no program holds the pseudo terminal open
CLOSED_POLL_TIMEOUT = 1

# errors of a pseudo terminal without a reader or without room
TERMINAL_ERRORS = (errno.EAGAIN, errno.EIO)


def parse_arguments():
    """Parse the command line."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('socket', help='unix socket of the QEMU serial port, -chardev socket,server=on,path=SOCKET')
    parser.add_argument('link', help='symbolic link to the pseudo terminal that the bridge creates')
    parser.add_argument('--retry-delay', type=float, default=0.02,
                        help='seconds between two connection attempts to the socket (default 0.02)')
    return parser.parse_args()


def open_terminal(link):
    """Open a raw pseudo terminal, link its name and return the master descriptor."""
    master, slave = os.openpty()
    tty.setraw(slave)
    name = os.ttyname(slave)
    os.close(slave)
    os.set_blocking(master, False)
    if os.path.lexists(link):
        os.unlink(link)
    os.symlink(name, link)
    return master


def connect_socket(path, retry_delay):
    """Connect to the unix socket of QEMU, waiting while QEMU is not running."""
    while True:
        connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            connection.connect(path)
            return connection
        except OSError:
            connection.close()
            time.sleep(retry_delay)


def get_terminal_open(master):
    """Return whether a program holds the pseudo terminal open, a closed one hangs up."""
    poller = select.poll()
    poller.register(master, select.POLLIN)
    return not any(events & select.POLLHUP for _, events in poller.poll(0))


def main():
    """Forward the bytes between the socket and the pseudo terminal until the bridge is stopped."""
    arguments = parse_arguments()
    master = open_terminal(arguments.link)
    connection = connect_socket(arguments.socket, arguments.retry_delay)
    while True:
        # wait for bytes of the chip and, with a reader, for bytes of the program
        terminal_open = get_terminal_open(master)
        poller = select.poll()
        poller.register(connection, select.POLLIN)
        if terminal_open:
            poller.register(master, select.POLLIN)
        for descriptor, events in poller.poll(OPEN_POLL_TIMEOUT if terminal_open else CLOSED_POLL_TIMEOUT):
            # pass the bytes of the chip to a reader or drop them, connect again after a restart of QEMU
            if descriptor == connection.fileno():
                try:
                    data = connection.recv(CHUNK_SIZE)
                except OSError:
                    data = b''
                if not data:
                    connection.close()
                    connection = connect_socket(arguments.socket, arguments.retry_delay)
                    break
                if terminal_open:
                    try:
                        os.write(master, data)
                    except OSError as error:
                        if error.errno not in TERMINAL_ERRORS:
                            raise
                continue

            # pass the bytes of the program to the chip
            if events & select.POLLIN:
                try:
                    data = os.read(master, CHUNK_SIZE)
                except OSError as error:
                    if error.errno in TERMINAL_ERRORS:
                        continue
                    raise
                try:
                    connection.sendall(data)
                except OSError:
                    pass
    return 0


if __name__ == '__main__':
    sys.exit(main())
