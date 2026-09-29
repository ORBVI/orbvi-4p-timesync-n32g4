"""Host-side smoke tests for the board's strict RMC/PPS passive probe."""
import datetime
import importlib.util
import pathlib
import unittest

path = pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'board_link_test.py'
spec = importlib.util.spec_from_file_location('board_link_test', path)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def sentence(body):
    checksum = 0
    for byte in body.encode('ascii'):
        checksum ^= byte
    return ('$%s*%02X' % (body, checksum)).encode('ascii')


class ProbeTests(unittest.TestCase):
    def test_valid_sentence_and_year_mapping(self):
        self.assertEqual(probe.valid_rmc(b'$GPRMC,123000.00,V,,,,,,,240926,,,N*76'),
                         datetime.datetime(2026, 9, 24, 12, 30))
        for year in (0, 25, 99):
            raw = sentence('GPRMC,235959.00,V,,,,,,,3112%02d,,,N' % year)
            self.assertEqual(probe.valid_rmc(raw), datetime.datetime(2000 + year, 12, 31, 23, 59, 59))

    def test_bad_checksum_date_time(self):
        self.assertIsNone(probe.valid_rmc(b'$GPRMC,123000.00,V,,,,,,,240926,,,N*00'))
        for body in ('GPRMC,123000.00,V,,,,,,,310226,,,N',
                     'GPRMC,246000.00,V,,,,,,,240926,,,N',
                     'GPRMC,123000.00,V,,,,,,,290223,,,N'):
            self.assertIsNone(probe.valid_rmc(sentence(body)))

    def test_reject_non_demo_fields_and_legacy(self):
        good = 'GPRMC,123000.00,V,,,,,,,240926,,,N'
        for body in (good.replace('GPRMC', 'GNRMC'),
                     good.replace(',V,', ',A,'),
                     good.replace('00,V', '01,V'),
                     good.replace('V,,,,,,,', 'V,1234.5,N,,,,,'),
                     good.replace(',,,N', ',1.0,E,A'),
                     good.replace(',,,N', ',,,A')):
            self.assertIsNone(probe.valid_rmc(sentence(body)))
        self.assertIsNone(probe.valid_rmc(sentence('GPZDA,123000.00,24,09,2026,00,00')))
        self.assertIsNone(probe.valid_rmc(sentence(good) + b'\r'))

    def test_sequence_to_utc_pairing(self):
        first = datetime.datetime(2025, 1, 1)
        self.assertEqual(probe.expected_utc(first, 520, 524),
                         datetime.datetime(2025, 1, 1, 0, 0, 4))
        self.assertEqual(probe.pps_sequence('1790246655.356903875#464'), 464)


if __name__ == '__main__':
    unittest.main()
