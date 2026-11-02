"""Cross-check both compiled C transmit formats against the passive PPS probe."""
import datetime
import importlib.util
import pathlib
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('interop_probe', ROOT / 'tools' / 'board_link_test.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class ProtocolInteropTests(unittest.TestCase):
    def test_each_selected_format_pairs_with_consecutive_synthetic_pps(self):
        first = datetime.datetime(2024, 2, 29, 23, 59, 58)
        for protocol in ('ZDA', 'RMC'):
            with self.subTest(protocol=protocol):
                executable = ROOT / 'build' / ('test_time_protocol_' + protocol.lower())
                wire = subprocess.check_output([str(executable), '--emit'], timeout=5)
                self.assertEqual(wire.count(b'\r\n'), 6)
                sentences = wire.splitlines()
                self.assertEqual(len(sentences), 6)
                other = 'RMC' if protocol == 'ZDA' else 'ZDA'
                for sequence, raw in enumerate(sentences, 100):
                    self.assertEqual(probe.valid_sentence(raw, protocol),
                                     probe.expected_utc(first, 100, sequence))
                    self.assertIsNone(probe.valid_sentence(raw, other))


if __name__ == '__main__':
    unittest.main()
