"""Give one serial port of QEMU a pseudo terminal that behaves like the USB-serial bridge of a board.

    python3 tools/uart_bridge.py SOCKET LINK [--retry-delay SECONDS]

QEMU serves the serial port on the unix socket SOCKET, LINK becomes a symbolic link to a new pseudo terminal.
QEMU's own pty backend notices a program that opens its pseudo terminal only about once a second and holds the bytes
of that second back, so the first request of every host command would go out again as a retransmission.
This bridge owns the pseudo terminal and passes every byte at once in both directions.
The bytes a program writes right before it closes the pseudo terminal still reach the chip, as from a USB bridge.
Bytes of the chip are dropped while no program holds the pseudo terminal open, as a USB bridge without a reader does.
The bridge connects to the socket again when QEMU restarts, so the pseudo terminal stays like the port of a board.
QSerialPort opens a port in exclusive mode and ends the mode when it closes the port. A pseudo terminal keeps the mode
of a killed program and refuses every other program, so the bridge then links a new pseudo terminal in its place, as
the port of a board is free again once the program is gone.
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

# error of a pseudo terminal in the exclusive mode of a program
EXCLUSIVE_ERROR = errno.EBUSY


def parse_arguments():
    """Parse the command line."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('socket', help='unix socket of the QEMU serial port, -chardev socket,server=on,path=SOCKET')
    parser.add_argument('link', help='symbolic link to the pseudo terminal that the bridge creates')
    parser.add_argument('--retry-delay', type=float, default=0.02,
                        help='seconds between two connection attempts to the socket (default 0.02)')
    return parser.parse_args()


def open_terminal(link):
    """Open a raw pseudo terminal, replace the link with its name and return the master descriptor and the name."""
    master, slave = os.openpty()
    tty.setraw(slave)
    name = os.ttyname(slave)
    os.close(slave)
    os.set_blocking(master, False)
    new_link = '%s.new' % link
    if os.path.lexists(new_link):
        os.unlink(new_link)
    os.symlink(name, new_link)
    os.replace(new_link, link)
    return master, name


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


def get_terminal_exclusive(master, name):
    """Return whether no program holds the pseudo terminal open and it still refuses a new one."""
    try:
        descriptor = os.open(name, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    except OSError as error:
        if error.errno != EXCLUSIVE_ERROR:
            raise
        return not get_terminal_open(master)
    os.close(descriptor)
    return False


def pass_program_bytes(master, connection):
    """Pass the bytes that wait in the pseudo terminal to the chip, return whether there were any."""
    try:
        data = os.read(master, CHUNK_SIZE)
    except OSError as error:
        if error.errno in TERMINAL_ERRORS:
            return False
        raise
    try:
        connection.sendall(data)
    except OSError:
        pass
    return len(data) > 0


def drain_terminal(master, connection):
    """Pass every byte that a closed pseudo terminal still holds to the chip."""
    passed = True
    while passed:
        passed = pass_program_bytes(master, connection)


def main():
    """Forward the bytes between the socket and the pseudo terminal until the bridge is stopped."""
    arguments = parse_arguments()
    master, name = open_terminal(arguments.link)
    connection = connect_socket(arguments.socket, arguments.retry_delay)
    terminal_was_open = False
    while True:
        # pass the last bytes of a program that closed the pseudo terminal before the bridge reacts to the hangup
        terminal_open = get_terminal_open(master)
        if not terminal_open:
            drain_terminal(master, connection)

        # link a new pseudo terminal when the program that held it open left it exclusive
        if terminal_was_open and not terminal_open and get_terminal_exclusive(master, name):
            os.close(master)
            master, name = open_terminal(arguments.link)
        terminal_was_open = terminal_open

        # wait for bytes of the chip and, with a reader, for bytes of the program
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
                pass_program_bytes(master, connection)
    return 0


if __name__ == '__main__':
    sys.exit(main())
