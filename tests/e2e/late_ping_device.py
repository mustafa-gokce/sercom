"""Play a device whose answer to the boot ping of sercom_cli could be taken for the answer of the ping command.

    python3 tests/e2e/late_ping_device.py LINK [--mode late|failed] [--timeout SECONDS]

The device opens a pseudo terminal and links its name to LINK. A Windows COM port name such as COM11 opens that
port through pyserial instead, one end of a com0com pair whose other end sercom_cli opens.
In the late mode it acknowledges the boot ping request without an answer, waits for the next ping
request and then answers the boot ping first and the next ping second, with the uptimes 1111 ms and
2222 ms. In the failed mode it answers the boot ping request with a log report only, so that request
fails after its retransmissions, and it answers the next ping request two seconds late with 2222 ms.
In both modes a ping command of one ping must succeed and print the uptime 2222 ms only.
"""

import argparse
import os
import re
import select
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
import sercom_frame

ACK_REQUEST_FLAG = 0x01
ACK_FLAG = 0x02
PING_REQUEST_TYPE = 0x0001
PING_RESPONSE_TYPE = 0x0002
LOG_REPORT_RESPONSE_TYPE = 0x0005
DEBUG_LOG_LEVEL = 1
LATE_UPTIME_MSEC = 1111
COMMAND_UPTIME_MSEC = 2222
READ_SIZE = 4096
POLL_PERIOD_SECONDS = 0.05
BAUD_RATE = 115200
COM_PORT_NAME = re.compile(r'^(\\\\\.\\)?COM[0-9]+$', re.IGNORECASE)


class PseudoTerminal:
    """The master side of a pseudo terminal whose slave side is linked to a name."""

    def __init__(self, link):
        import tty
        self.link = link
        self.master, self.slave = os.openpty()
        tty.setraw(self.slave)
        if os.path.lexists(link):
            os.remove(link)
        os.symlink(os.ttyname(self.slave), link)

    def read(self):
        """Return the received bytes or None after a quiet poll period."""
        readable, _, _ = select.select([self.master], [], [], POLL_PERIOD_SECONDS)
        if not readable:
            return None
        return os.read(self.master, READ_SIZE)

    def write(self, data):
        """Send bytes."""
        os.write(self.master, data)

    def close(self):
        """Remove the link and close both sides."""
        os.remove(self.link)
        os.close(self.master)
        os.close(self.slave)


class ComPort:
    """A Windows COM port opened through pyserial."""

    def __init__(self, name):
        try:
            import serial
        except ImportError:
            sys.exit('COM ports need pyserial: python -m pip install pyserial')
        self.port = serial.Serial(name, BAUD_RATE, timeout=POLL_PERIOD_SECONDS)

    def read(self):
        """Return the received bytes or None after a quiet poll period."""
        data = self.port.read(1)
        if not data:
            return None
        return data + self.port.read(self.port.in_waiting)

    def write(self, data):
        """Send bytes."""
        self.port.write(data)

    def close(self):
        """Close the port."""
        self.port.close()


def get_ping_response(message_counter, request_timestamp, uptime):
    """Return the bytes of a ping response frame."""
    payload = struct.pack('<II', request_timestamp, uptime)
    return sercom_frame.encode(ACK_REQUEST_FLAG, message_counter, PING_RESPONSE_TYPE, payload)


def get_log_report(message_counter, text):
    """Return the bytes of a log report frame without acknowledgement."""
    payload = bytes([DEBUG_LOG_LEVEL, len(text)]) + text.encode('ascii')
    return sercom_frame.encode(0, message_counter, LOG_REPORT_RESPONSE_TYPE, payload)


def get_acknowledgement(message_counter):
    """Return the bytes of the acknowledgement of a ping request."""
    return sercom_frame.encode(ACK_FLAG, message_counter, PING_REQUEST_TYPE)


def run_device(port, arguments):
    """Answer the ping requests on the port until the timeout."""
    decoder = sercom_frame.Decoder()
    boot_ping = None
    command_ping = None
    scheduled = []
    deadline = time.monotonic() + arguments.timeout
    while time.monotonic() < deadline:
        # send the scheduled frames that are due
        for frame in [frame for send_time, frame in scheduled if send_time <= time.monotonic()]:
            port.write(frame)
        scheduled = [(send_time, frame) for send_time, frame in scheduled if send_time > time.monotonic()]

        # take the ping requests of the host
        data = port.read()
        if data is None:
            continue
        for result in decoder.on_received_data(data):
            if result[0] != 'frame':
                continue
            fields = result[1]
            if fields['type'] != PING_REQUEST_TYPE or not fields['flags'] & ACK_REQUEST_FLAG:
                continue
            (request_timestamp,) = struct.unpack_from('<I', fields['payload'])
            print('ping request #%d at %d ms' % (fields['counter'], request_timestamp))
            sys.stdout.flush()

            # answer the boot ping late or let it fail
            if boot_ping is None:
                boot_ping = (fields['counter'], request_timestamp)
                time.sleep(arguments.delay)
                if arguments.mode == 'late':
                    port.write(get_acknowledgement(fields['counter']))
                else:
                    port.write(get_log_report(0, 'booted'))
                continue

            # acknowledge a retransmission of the boot ping in the late mode only
            if fields['counter'] == boot_ping[0]:
                if arguments.mode == 'late':
                    port.write(get_acknowledgement(fields['counter']))
                continue

            # acknowledge a retransmission of the ping of the command again
            port.write(get_acknowledgement(fields['counter']))
            if command_ping is not None:
                continue
            command_ping = (fields['counter'], request_timestamp)

            # answer the boot ping first and the ping of the command second
            if arguments.mode == 'late':
                port.write(get_ping_response(0, boot_ping[1], LATE_UPTIME_MSEC))
                port.write(get_ping_response(1, request_timestamp, COMMAND_UPTIME_MSEC))
                continue

            # answer the ping of the command after the response delay
            send_time = time.monotonic() + arguments.response_delay
            scheduled.append((send_time, get_ping_response(1, request_timestamp, COMMAND_UPTIME_MSEC)))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('link', help='path of the link to the pseudo terminal of the device or a COM port name')
    parser.add_argument('--mode', choices=('late', 'failed'), default='late',
                        help='late answers the boot ping late, failed lets it fail (default late)')
    parser.add_argument('--timeout', type=float, default=10.0, help='run time in seconds (default 10)')
    parser.add_argument('--delay', type=float, default=0.02,
                        help='delay of the first answer to the boot ping in seconds (default 0.02)')
    parser.add_argument('--response-delay', type=float, default=2.0,
                        help='delay of the ping answer of the command in the failed mode in seconds (default 2)')
    arguments = parser.parse_args()

    # open the COM port or the pseudo terminal raw and link its name
    port = ComPort(arguments.link) if COM_PORT_NAME.match(arguments.link) else PseudoTerminal(arguments.link)
    print('Late ping device ready on %s in the %s mode' % (arguments.link, arguments.mode))
    sys.stdout.flush()

    # answer until the timeout
    try:
        run_device(port, arguments)
    finally:
        port.close()


if __name__ == '__main__':
    main()
