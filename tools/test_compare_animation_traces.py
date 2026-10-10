import contextlib
import copy
import io
import json
from pathlib import Path
import tempfile
import unittest

import compare_animation_traces as c

COMMIT = "1234567890abcdef1234567890abcdef12345678"


def valid(platform):
    records = []
    positions = ((1,-2,.5),(2.5,-2.75,.875),(5,-4,1.5),(5.125,-3.9375,1.5),
                 (7,-3,1.5),(11,-1,1.5),(5.5,-3.75,1.5),(1,-2,.5))
    for case, count, ticks in c.CASES:
        for sample, tick in enumerate(ticks):
            joints = []
            for i in range(count):
                position = list(positions[sample]) if case == "channels" and i == 0 else [0,0,0]
                if case == "mask63" and i == 63 and sample == 0:
                    position = [-512,32767/64,-1/64]
                joints.append(dict(joint=i, rotation_valid=case != "zero_quaternion",
                                   drives_joint=not ((case == "channels" and i == 1) or (case == "mask63" and i == 63)),
                                   position=position, quaternion_wxyz=[0,0,0,0] if case == "zero_quaternion" else [1,0,0,0]))
            records.append(dict(case=case, sample=sample, tick=tick, joints=joints))
    return dict(schema=1, fixture=c.FIXTURE, platform=platform, source_commit=COMMIT, records=records)


class TraceTests(unittest.TestCase):
    def setUp(self):
        self.traces = [valid(p) for p in sorted(c.PLATFORMS)]

    def reject(self, traces=None):
        with self.assertRaises(ValueError):
            c.compare(self.traces if traces is None else traces, COMMIT)

    def test_all_five_nonempty(self):
        self.assertEqual(c.compare(self.traces, COMMIT), 7924)

    def test_missing_extra_duplicate(self):
        self.reject([]); self.reject(self.traces[:-1]); self.reject(self.traces + [self.traces[0]])
        self.traces[-1] = self.traces[0]; self.reject()

    def test_missing_empty_extra_records(self):
        for records in ([], self.traces[0]["records"][:-1], self.traces[0]["records"] + [self.traces[0]["records"][0]]):
            changed = copy.deepcopy(self.traces); changed[0]["records"] = records; self.reject(changed)

    def test_source_schema_case_sample(self):
        for field, value in (("source_commit", "f"*40), ("schema", True), ("fixture", "other"), ("platform", "unknown")):
            changed = copy.deepcopy(self.traces); changed[0][field] = value; self.reject(changed)
        for field, value in (("case", "missing"), ("sample", True), ("tick", True), ("tick", 0.25)):
            changed = copy.deepcopy(self.traces); changed[0]["records"][0][field] = value; self.reject(changed)
        with self.assertRaises(ValueError): c.compare(self.traces, "short")

    def test_joint_coverage_and_boolean_types(self):
        for field, value in (("joint", True), ("joint", 63), ("rotation_valid", 1), ("drives_joint", False)):
            changed = copy.deepcopy(self.traces); changed[0]["records"][0]["joints"][0][field] = value; self.reject(changed)
        self.traces[0]["records"][0]["joints"].pop(); self.reject()

    def test_discrete_mask_mismatch(self):
        self.traces[0]["records"][8]["joints"][63]["drives_joint"] = True; self.reject()

    def test_nonnumeric_and_nonfinite(self):
        for value in (True, "0", None, float("nan"), float("inf"), -float("inf"), 10**1000):
            changed = copy.deepcopy(self.traces); changed[0]["records"][1]["joints"][2]["position"][0] = value
            self.reject(changed)

    def test_bad_vector_and_quaternion(self):
        for field, value in (("position", []), ("quaternion_wxyz", [0,0,0,0]), ("quaternion_wxyz", [10**200,0,0,0])):
            changed = copy.deepcopy(self.traces); changed[0]["records"][1]["joints"][2][field] = value; self.reject(changed)

    def test_tolerance_accepts_small_difference(self):
        self.traces[0]["records"][1]["joints"][2]["position"][0] += 1e-14
        self.assertGreater(c.compare(self.traces, COMMIT), 0)

    def test_tolerance_rejects_disagreement(self):
        self.traces[0]["records"][1]["joints"][2]["position"][0] += 1e-4; self.reject()

    def test_repeated_query_must_repeat(self):
        self.traces[0]["records"][7]["joints"][2]["position"][0] = 1; self.reject()

    def test_identical_placeholder_traces_fail(self):
        for trace in self.traces:
            for row in trace["records"]:
                for joint in row["joints"]: joint["position"] = [0,0,0]
        self.reject()

    def test_unexpected_fields(self):
        self.traces[0]["extra"] = 1; self.reject()

    def test_json_duplicate_nonfinite_and_invalid(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/"trace.json"
            for content in ('{"schema":1,"schema":1}', '{"x":NaN}', '{"x":Infinity}', '{'):
                path.write_text(content)
                with self.assertRaises(ValueError): c.load_trace(path)
            path.write_text(' ' * (c.MAX_FILE_BYTES + 1))
            with self.assertRaises(ValueError): c.load_trace(path)

    def test_cli_five_files_and_failure_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            argv = ["--trace-dir", directory, "--source-commit", COMMIT]
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(c.main(argv), 1)
                for trace in self.traces: (root/(trace["platform"]+".json")).write_text(json.dumps(trace))
                self.assertEqual(c.main(argv), 0)
                (root/"extra.json").write_text('{}')
                self.assertEqual(c.main(argv), 1)


if __name__ == "__main__":
    unittest.main()
