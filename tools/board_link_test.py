#!/usr/bin/env python3
"""Passive orbvi-4p receiver for the N32 demo GPS PPS + RMC stream.

Never writes UART, CLOCK_REALTIME or RTC; validates PPS/RMC pairing only.
"""
import argparse
import datetime
import os
import re
import select
import termios
import time


def pps_sequence(assert_line):
    return int(assert_line.rsplit('#', 1)[1])


def valid_rmc(raw):
    # Exactly the GP, V/no-fix, empty location/speed/course/magvar, N-mode
    # sentence emitted by this firmware. Do not treat V as a position fix.
    match = re.fullmatch(rb'\$GPRMC,(\d{6})\.00,V,,,,,,,(\d{6}),,,N\*([0-9A-F]{2})', raw)
    if not match:
        return None
    checksum = 0
    for byte in raw[1:raw.index(b'*')]:
        checksum ^= byte
    if checksum != int(match.group(3), 16):
        return None
    date = match.group(2)
    clock = match.group(1)
    try:
        return datetime.datetime(2000 + int(date[4:]), int(date[2:4]), int(date[:2]),
                                 int(clock[:2]), int(clock[2:4]), int(clock[4:]))
    except ValueError:
        return None


def expected_utc(first_utc, first_sequence, sequence):
    return first_utc + datetime.timedelta(seconds=sequence - first_sequence)


def probe(port, pps, expect_first, duration):
    path = '/sys/class/pps/' + pps + '/assert'
    if not os.path.isfile(path):
        raise RuntimeError('PPS assert node missing: ' + path)
    fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        settings = termios.tcgetattr(fd)
        try:
            raw = termios.tcgetattr(fd)
            raw[0] = 0
            raw[1] = 0
            raw[2] = (raw[2] & ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)) | termios.CS8 | termios.CLOCAL | termios.CREAD
            raw[3] = 0
            raw[4] = termios.B115200
            raw[5] = termios.B115200
            raw[6][termios.VMIN] = 0
            raw[6][termios.VTIME] = 0
            termios.tcsetattr(fd, termios.TCSANOW, raw)
            # Deliberately do not flush RX: capture traffic already on the wire.
            previous = open(path, encoding='ascii').read().strip()
            initial_sequence = pps_sequence(previous)
            current_sequence = initial_sequence
            print('PPS initial', previous, '(listening only)', flush=True)
            deadline = time.monotonic() + duration
            rx = bytearray()
            rmc_count = pps_count = matched = 0
            last_matched = initial_sequence
            first_utc = first_sequence = None
            mismatches = 0
            while time.monotonic() < deadline:
                now = open(path, encoding='ascii').read().strip()
                if now != previous:
                    print('PPS', now, flush=True)
                    previous = now
                    next_sequence = pps_sequence(now)
                    if next_sequence != current_sequence + 1:
                        print('PPS sequence gap', current_sequence, next_sequence, flush=True)
                        mismatches += 1
                    current_sequence = next_sequence
                    pps_count += 1
                if select.select([fd], [], [], 0.02)[0]:
                    try:
                        rx.extend(os.read(fd, 1024))
                    except BlockingIOError:
                        pass
                    while b'\n' in rx:
                        line, _, tail = rx.partition(b'\n')
                        rx = bytearray(tail)
                        line = line.rstrip(b'\r')
                        decoded = valid_rmc(line)
                        if decoded and first_utc is None and current_sequence > initial_sequence:
                            first_utc, first_sequence = decoded, current_sequence
                        target = (expected_utc(first_utc, first_sequence, current_sequence)
                                  if first_utc is not None else None)
                        paired = (decoded is not None and decoded == target and
                                  current_sequence > last_matched and current_sequence > initial_sequence and
                                  (expect_first is None or first_utc == expect_first))
                        print('UART', repr(bytes(line)),
                              ('UTC=' + decoded.isoformat()) if decoded else 'INVALID',
                              'PPS#%d' % current_sequence, 'MATCH' if paired else 'MISMATCH', flush=True)
                        rmc_count += bool(decoded)
                        if paired:
                            matched += 1
                            last_matched = current_sequence
                        else:
                            mismatches += 1
                    if len(rx) > 256:
                        print('UART discarded overlong partial line', flush=True)
                        rx.clear()
            print('SUMMARY pps_edges=%d valid_rmc=%d paired=%d mismatches=%d' %
                  (pps_count, rmc_count, matched, mismatches), flush=True)
            return 0 if pps_count >= 2 and matched >= 2 and mismatches == 0 else 1
        finally:
            termios.tcsetattr(fd, termios.TCSANOW, settings)
    finally:
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='/dev/ttyS1')
    parser.add_argument('--pps', default='pps1')
    parser.add_argument('--expect-first', help='Require first observed RMC at next PPS = YYYYMMDDhhmmss (start before N32 reset)')
    parser.add_argument('--duration', type=float, default=5)
    args = parser.parse_args()
    try:
        first = datetime.datetime.strptime(args.expect_first, '%Y%m%d%H%M%S') if args.expect_first else None
        if args.expect_first and len(args.expect_first) != 14:
            parser.error('first UTC must contain 14 digits')
        if args.duration < 2:
            parser.error('duration must be >= 2 s')
    except ValueError:
        parser.error('invalid first UTC')
    return probe(args.port, args.pps, first, args.duration)


if __name__ == '__main__':
    raise SystemExit(main())
