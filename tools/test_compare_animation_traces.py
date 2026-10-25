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
                rotation_valid = case != "zero_quaternion" and not (case == "invalid_ancestor" and i == 0)
                transform_valid = case != "zero_quaternion" and not (case == "invalid_ancestor" and i < 3)
                drives = not ((case in ("channels", "invalid_ancestor") and i == 1)
                              or (case == "mask63" and i == 63) or (case == "rotated_hierarchy" and i == 0))
                identity = [1,0,0,0, 0,1,0,0, 0,0,1,0] if transform_valid else [0]*12
                joints.append(dict(joint=i, rotation_valid=rotation_valid,
                                   drives_joint=drives, transform_valid=transform_valid, transform_drives_joint=drives,
                                   model_matrix=list(c.MODEL_ANCHORS.get((case,i),identity)),
                                   skin_matrix=list(c.SKIN_ANCHORS.get((case,i),identity)),
                                   position=position, quaternion_wxyz=[0,0,0,0] if not rotation_valid else [1,0,0,0]))
            records.append(dict(case=case, sample=sample, tick=tick, joints=joints))
    skin = [dict(case=name,mesh_index=index,tick=12.5,positions=[list(p) for p in points]) for name,index,points in c.SKIN_CASES]
    return dict(schema=3, fixture=c.FIXTURE, platform=platform, source_commit=COMMIT, records=records, skinning_records=skin)


class TraceTests(unittest.TestCase):
    def setUp(self):
        self.traces = [valid(p) for p in sorted(c.PLATFORMS)]

    def reject(self, traces=None):
        with self.assertRaises(ValueError):
            c.compare(self.traces if traces is None else traces, COMMIT)

    def test_all_five_nonempty(self):
        self.assertEqual(c.compare(self.traces, COMMIT), 36248)

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

    def test_old_contract_rejected(self):
        self.traces[0]["schema"] = 1; self.traces[0]["fixture"] = "rta-pose-authored-v1"
        self.reject()

    def test_matrix_fields_required(self):
        for field in ("model_matrix", "skin_matrix", "transform_valid", "transform_drives_joint"):
            changed = copy.deepcopy(self.traces); del changed[0]["records"][0]["joints"][0][field]
            self.reject(changed)

    def test_matrix_shape_and_finiteness(self):
        for value in ([], [0]*11, [0]*13, [True]*12, [float("nan")]*12, [float("inf")]*12):
            for field in ("model_matrix", "skin_matrix"):
                changed = copy.deepcopy(self.traces); changed[0]["records"][1]["joints"][2][field] = value
                self.reject(changed)

    def test_matrix_peer_disagreement_and_tolerance(self):
        self.traces[0]["records"][1]["joints"][2]["skin_matrix"][3] += 1e-14
        self.assertEqual(c.compare(self.traces,COMMIT),36248)
        self.traces[0]["records"][1]["joints"][2]["model_matrix"][4] += 1e-4
        self.reject()

    def test_invalid_ancestor_and_independent_root(self):
        for index, field, value in ((1,"transform_valid",True), (2,"transform_valid",1),
                                    (3,"transform_valid",False), (1,"transform_drives_joint",True)):
            changed = copy.deepcopy(self.traces); changed[0]["records"][16]["joints"][index][field] = value
            self.reject(changed)
        self.traces[0]["records"][16]["joints"][1]["model_matrix"][0] = 1
        self.reject()

    def test_identical_wrong_hierarchy_bind_or_matrices_fail(self):
        for field, index, component, value in (("model_matrix",1,0,1), ("skin_matrix",1,3,-5),
                                              ("model_matrix",1,7,2), ("skin_matrix",0,3,1)):
            changed = copy.deepcopy(self.traces)
            for trace in changed: trace["records"][15]["joints"][index][field][component] = value
            self.reject(changed)

    def test_transform_only_contract_rejected(self):
        self.traces[0]["schema"] = 2; self.traces[0]["fixture"] = "rta-transforms-authored-v2"
        del self.traces[0]["skinning_records"]
        self.reject()

    def test_skinning_required_nonempty_and_complete(self):
        for value in (None, [], self.traces[0]["skinning_records"][:-1], self.traces[0]["skinning_records"]*2):
            changed=copy.deepcopy(self.traces); changed[0]["skinning_records"]=value; self.reject(changed)
        del self.traces[0]["skinning_records"]; self.reject()

    def test_skinning_discrete_fields_and_duplicate_case(self):
        for field,value in (("case","rigid"),("mesh_index",True),("mesh_index",0),("tick",0),("tick",True)):
            changed=copy.deepcopy(self.traces); changed[0]["skinning_records"][1][field]=value; self.reject(changed)

    def test_skinning_bad_corner_coverage_or_values(self):
        for value in ([], [[1,2,3]]*2, [[1,2,3]]*4, [[float("inf"),0,0]]*3, [[0,0]]*3, [[True,0,0]]*3):
            changed=copy.deepcopy(self.traces); changed[0]["skinning_records"][0]["positions"]=value; self.reject(changed)

    def test_skinning_equal_wrong_positions_fail(self):
        for trace in self.traces:
            for row in trace["skinning_records"]: row["positions"]=[[0,0,0] for _ in row["positions"]]
        self.reject()

    def test_skinning_weight_normalization_joint63_and_seam(self):
        for case,corner,value in ((1,0,[1,4,6.5]),(3,0,[448/256,257/256,455.5/256]),
                                  (4,0,[1,2,3]),(6,3,[99,99,99])):
            changed=copy.deepcopy(self.traces)
            for trace in changed: trace["skinning_records"][case]["positions"][corner]=value
            self.reject(changed)

    def test_skinning_peer_difference_even_when_both_near_anchor(self):
        for trace in self.traces:
            if trace["platform"] == "linux-x64": trace["skinning_records"][1]["positions"][0][0] += 1e-12
            else: trace["skinning_records"][1]["positions"][0][0] -= 1e-12
            c.validate(trace,COMMIT)
        self.reject()

    def test_diagnostic_tick_field_values_and_context(self):
        peer = next(t for t in self.traces if t["platform"] == "macos-x64")
        peer["records"][3]["joints"][2]["position"][0] = 99
        with self.assertRaises(ValueError) as error:
            c.compare(self.traces, COMMIT)
        message = str(error.exception)
        for expected in ("suite=animation/records", "record index=3", "tick=4.25",
                         "field=joints[2].position[0]", "linux-x64=0", "macos-x64=99",
                         "preceding matching state", "record index=0", "record index=2"):
            self.assertIn(expected, message)

    def test_diagnostic_discrete_and_skinning_fields(self):
        for skin in (False, True):
            traces = copy.deepcopy(self.traces)
            peer = next(t for t in traces if t["platform"] == "macos-x64")
            if skin:
                peer["skinning_records"][2]["positions"][1][2] = 99
                field, suite = "positions[1][2]", "animation/skinning_records"
            else:
                peer["records"][0]["joints"][0]["drives_joint"] = False
                field, suite = "joints[0].drives_joint", "animation/records"
            with self.assertRaises(ValueError) as error:
                c.compare(traces, COMMIT)
            self.assertIn("field=" + field, str(error.exception))
            self.assertIn("suite=" + suite, str(error.exception))

    def test_earliest_record_across_peers(self):
        by_platform = {t["platform"]: t for t in self.traces}
        by_platform["macos-arm64"]["records"][3]["joints"][2]["position"][0] = 99
        by_platform["windows-x64"]["records"][1]["joints"][2]["position"][0] = 42
        with self.assertRaises(ValueError) as error:
            c.compare(self.traces, COMMIT)
        self.assertIn("record index=1", str(error.exception))
        self.assertIn("windows-x64=42", str(error.exception))

    def test_numeric_tick_representation_preserves_equality(self):
        self.traces[0]["records"][0]["tick"] = 0.0
        self.assertEqual(c.compare(self.traces, COMMIT), 36248)

    def test_cancelled_producer_is_incomplete(self):
        argv = ["--trace-dir", "unused", "--source-commit", COMMIT]
        with contextlib.redirect_stderr(io.StringIO()) as output:
            for result in ("cancelled", "skipped", "failure"):
                self.assertEqual(c.main(argv + ["--native-result", result]), 2)
            self.assertIn("INCOMPLETE:", output.getvalue())
            self.assertNotIn("MISMATCH:", output.getvalue())

    def test_cli_five_files_and_failure_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            argv = ["--trace-dir", directory, "--source-commit", COMMIT]
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(c.main(argv), 2)
                for trace in self.traces: (root/(trace["platform"]+".json")).write_text(json.dumps(trace))
                self.assertEqual(c.main(argv), 0)
                (root/"extra.json").write_text('{}')
                self.assertEqual(c.main(argv), 1)


if __name__ == "__main__":
    unittest.main()
