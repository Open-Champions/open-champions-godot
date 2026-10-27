"""Synthetic regressions for the player vector harness's fast comparison path."""
import io
import json
import unittest

from test_player_vectors import compare_observations, split_record


def record(expected, label="synthetic", config=0, inputs=None):
    if inputs is None:
        inputs = [[], [], []]
    return (json.dumps([label, config, inputs, expected], separators=(",", ":"))
            + "\n").encode("ascii")


def compare(actual, *vectors):
    offsets = [split_record(line)[3] for line in vectors]
    return compare_observations(io.BytesIO(actual), vectors, offsets)


class PlayerVectorHarnessTests(unittest.TestCase):
    def test_input_slice_preserves_nested_lists_and_integer_boundaries(self):
        inputs = [[[65536, 8, 18446744073709551615]], [],
                  [[31, 4, 0, -9223372036854775808]], [], [[], []]]
        line = record(["expected-only"], 'escaped_"label', 15, inputs)
        label, config, payload, offset = split_record(line)
        self.assertEqual(label, 'escaped_"label')
        self.assertEqual(config, 15)
        self.assertEqual(payload, json.dumps(inputs, separators=(",", ":")))
        self.assertEqual(line[offset:-2], b'["expected-only"]')
        self.assertNotIn("expected-only", payload)

    def test_every_canonical_observation_is_compared(self):
        self.assertEqual(compare(b"[[1],[]]\n[2]\n",
                                 record([[1], []]), record([2])), 2)

    def test_crlf_observations(self):
        self.assertEqual(compare(b"[1]\r\n", record([1])), 1)

    def test_equivalent_json_keeps_the_original_value_comparison(self):
        self.assertEqual(compare(b" [ 1.0, [2] ] \n", record([1, [2]])), 1)

    def test_equivalent_unicode_json(self):
        self.assertEqual(compare('["\u00e9"]\n'.encode("utf-8"), record(["\u00e9"])), 1)

    def test_mismatch_keeps_row_label_configuration_and_difference(self):
        with self.assertRaisesRegex(AssertionError,
                                    r"row 1 changed config=15 full state mismatch"):
            compare(b"[1]\n[[3]]\n", record([1]),
                    record([[2]], "changed", 15))

    def test_missing_observation(self):
        with self.assertRaisesRegex(AssertionError, "missing observation"):
            compare(b"", record([1]))

    def test_extra_observation(self):
        with self.assertRaisesRegex(AssertionError, "extra observation"):
            compare(b"[1]\n[2]\n", record([1]))

    def test_malformed_observation(self):
        with self.assertRaises(json.JSONDecodeError):
            compare(b"not-json\n", record([1]))


if __name__ == "__main__":
    unittest.main()
