"""Synthetic byte-level regressions for the shared diagnostic renderer."""
import unittest
from core_trace_contract import expected_records, expected_trace
from trace_diagnostics import tsv_diagnostic


class Diagnostics(unittest.TestCase):
    def test_first_field_and_bounded_matching_window(self):
        left = [f'case\top\targs\tok\t[[{i}]]\n'.encode() for i in range(7)]
        right = left.copy()
        right[5] = b'case\top\targs\tok\t[[99]]\n'
        right[6] = b'case\top\targs\tbad\t[[99]]\n'
        message = tsv_diagnostic('state', iter(left), iter(right), ('linux-x64', 'macos-x64'))
        for expected in ('record index=5', 'field=state[0][0]', 'linux-x64=5', 'macos-x64=99',
                         'record index=2', 'record index=3', 'record index=4'):
            self.assertIn(expected, message)
        self.assertNotIn('record index=1', message)
        self.assertNotIn('record index=6', message)

    def test_missing_record_and_format_only_difference(self):
        rows = [b'1\tadd\t0000000000000000\n']
        self.assertIn('<missing>', tsv_diagnostic('arithmetic', rows, [], ('a', 'b')))
        self.assertIn('field=raw_line[0]', tsv_diagnostic(
            'arithmetic', rows, [rows[0].replace(b'\n', b'\r\n')], ('a', 'b')))
        self.assertIn('field=result', tsv_diagnostic('arithmetic', rows,
                       [b'1\tadd\t0000000000000001\n'], ('a', 'b')))

    def test_header_difference_uses_raw_line(self):
        message = tsv_diagnostic('math', [b'# rta-core-math-trace-v1\n'],
                                 [b'# wrong header\n'], ('a', 'b'))
        self.assertIn('field=raw_line[0]', message)
        self.assertIn('record index=0', message)

    def test_math_result_label(self):
        message = tsv_diagnostic('math',
            [b'# rta-core-math-trace-v1\n', b'sin16\tps2:0:0\t00000000\t34800000\n'],
            [b'# rta-core-math-trace-v1\n', b'sin16\tps2:0:0\t00000000\t34800001\n'], ('a', 'b'))
        self.assertIn('record index=0', message)
        self.assertIn('field=result', message)

    def test_json_formatting_remains_exact(self):
        self.assertIn('field=state', tsv_diagnostic('state',
            [b'c\to\ta\tok\t[1,2]\n'], [b'c\to\ta\tok\t[1, 2]\n'], ('a', 'b')))

    def test_expected_records_preserve_contract(self):
        import hashlib
        for suite, vectors, data in (
            ('arithmetic', [b'ps2\t0\t0\tadd\t0\t0\t00000001\n'], b'1\tadd\t0000000000000001\n'),
            ('math', [b'sin16\tps2\t0\t0\t00000000\t34800000\n'],
             b'# rta-core-math-trace-v1\nsin16\tps2:0:0\t00000000\t34800000\n'),
            ('state', [b'# skip\n', b'c\to\ta\tr\ts\n'], b'c\to\ta\tr\ts\n')):
            self.assertEqual(b''.join(expected_records(suite, vectors)), data)
            self.assertEqual(expected_trace(suite, vectors), (1, hashlib.sha256(data).hexdigest()))


if __name__ == '__main__':
    unittest.main()
