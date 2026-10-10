"""Authored transport/privacy tests; optional native executable runs real decoding."""
import argparse
import copy
import importlib.util
import io
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import threading
import types
import unittest
from unittest import mock

MODULE_PATH=pathlib.Path(__file__).with_name('validate_model_forms.py')
spec=importlib.util.spec_from_file_location('model_forms_validator',MODULE_PATH)
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
NATIVE=None

def triangle():
    data=bytearray(128)
    struct.pack_into('<HBB',data,0x10,0,1,255)
    data[0x25]=4;data[0x26]=1
    struct.pack_into('<I',data,0x28,128)
    struct.pack_into('<H',data,0x38,1)
    def code(imm,count,command):data.extend(struct.pack('<I',imm|(count<<16)|(command<<24)))
    def unpack(command,address,rows,fmt):
        code(address,len(rows),command)
        for row in rows:data.extend(struct.pack('<'+fmt,*row))
        while len(data)%4:data.append(0)
    code(3,0,4)
    unpack(0x69,0x8000,[(0,0,0),(16,0,0),(0,0,16)],'3h')
    unpack(0x6a,0x8003,[(0,-127,0)]*3,'3b')
    unpack(0x69,0xc006,[(3,0,3),(3,0,3),(0,0,0),(3,0,0),(6,0,0)],'3H')
    unpack(0x6e,0xc00b,[(0,0,255,3)],'4B')
    unpack(0x6d,0xc00c,[(0,0,0,0)],'4H')
    lo=3|(1<<15)|(1<<46)|(0x15c<<47)|(3<<60)
    unpack(0x6c,0xc00d,[(lo&0xffffffff,lo>>32,0x413,0)],'4I')
    code(0x42,0,0x14);code(0x103,0,1)
    unpack(0x65,0xc081,[(0,0),(16,0),(0,16)],'2H');code(0x101,0,1)
    while len(data)%16:data.append(0)
    struct.pack_into('<I',data,0x2c,len(data))
    return bytes(data),lo

def expected_triangle():
    _data,lo=triangle()
    corners=[]
    for pos,uv in zip([(0,0,0),(16,0,0),(0,0,16)],[(0,0),(16,0),(0,16)]):
        corners.append([[v.bits(x/16) for x in pos],[0,v.bits(-1),0],
                        [v.bits(x/16) for x in uv],[0,0,0,0],[255,0,0,0],1,
                        list(pos),[0,-127,0],list(uv),None])
    return dict(schema=1,status='decoded',flags=0,hidden_mask=0,part_mask=0,attachment_slot=-1,
                has_variants=False,alpha1=0,fix2=0,variants=[[0,0] for _ in range(32)],
                attach_points=[],facing_points=[],facing_terminator_raw=None,effect_segment_raw=None,
                sample_points=[],sample_block_raw=[],meshes=[dict(index=0,visible=True,joint=0,
                bounds_raw=[0]*6,declared_triangles=1,hull_points=[[0,0,0] for _ in range(8)],
                packets=[[0x42,3,3,3,0,3,[[lo,0x413]]]],corners=corners)])

class FakeProcess:
    def __init__(self,output=b'',code=0):self.stdout=io.BytesIO(output);self.code=code;self.killed=False
    def wait(self,timeout=None):return self.code
    def poll(self):return self.code
    def kill(self):self.killed=True;self.code=-9

class BlockingProcess(FakeProcess):
    def __init__(self):
        self.event=threading.Event();self.code=None;self.killed=False
        self.stdout=self
    def read(self,size):self.event.wait(5);return b''
    def close(self):pass
    def kill(self):super().kill();self.event.set()

class ProtocolTests(unittest.TestCase):
    def test_type_sensitive_comparison(self):
        self.assertEqual(v.first_difference({'x':True},{'x':1}),'$.x')
        self.assertEqual(v.first_difference({'x':[1]},{'x':[2]}),'$.x[]')
        self.assertIsNone(v.first_difference(expected_triangle(),copy.deepcopy(expected_triangle())))
    def test_missing_extra_fields_rejected(self):
        original=expected_triangle();changed=copy.deepcopy(original);changed['meshes'][0].pop('corners')
        self.assertEqual(v.first_difference(original,changed),'$.meshes[].keys')
    def test_geometry_changes_detected(self):
        for index in range(10):
            original=expected_triangle();changed=copy.deepcopy(original)
            changed['meshes'][0]['corners'][0][index]=None if index!=9 else [0,0,1]
            self.assertIsNotNone(v.first_difference(original,changed))
    def test_refusal_exit_contract(self):
        data=b'{"schema":1,"status":"refused","reason":"format"}\r\n'
        self.assertEqual(v.decode_response(data,2)['status'],'refused')
        for code in [0,1,3,-6]:
            with self.assertRaises(v.ValidationError):v.decode_response(data,code)
    def test_decoded_exit_contract(self):
        data=json.dumps(expected_triangle()).encode()
        self.assertEqual(v.decode_response(data,0),expected_triangle())
        with self.assertRaises(v.ValidationError):v.decode_response(data,2)
    def test_malformed_transport(self):
        for data in [b'',b'null',b'[]',b'{"schema":true}',b'{"schema":1,"schema":1}',
                     b'{"schema":1,"status":"decoded","value":NaN}',b'\xff',b'{}\n{}']:
            with self.assertRaises(v.ValidationError):v.decode_response(data,0)
    def test_output_bound(self):
        p=FakeProcess(b'x'*16)
        with mock.patch.object(v.subprocess,'Popen',return_value=p):
            with self.assertRaisesRegex(v.ValidationError,'output_limit'):v.run_native('native','private',max_output=8)
    def test_timeout_kills_child(self):
        p=BlockingProcess()
        with mock.patch.object(v.subprocess,'Popen',return_value=p):
            with self.assertRaisesRegex(v.ValidationError,'native_timeout'):v.run_native('native','private',timeout=.01)
        self.assertTrue(p.killed)
    def test_no_native_error_path_leak(self):
        with mock.patch.object(v.subprocess,'Popen',side_effect=OSError('/private/disc/secret.vif')):
            with self.assertRaisesRegex(v.ValidationError,'^native_unavailable$'):v.run_native('native','private')
    def test_reference_pin_checked_before_execution(self):
        result=types.SimpleNamespace(stdout=b'raise RuntimeError("must not execute")')
        with mock.patch.object(v.subprocess,'run',return_value=result):
            with self.assertRaisesRegex(v.ValidationError,'reference_pin_mismatch'):v.load_reference('private')
    def test_repository_inputs_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d);(p/'.git').mkdir();(p/'inputs').mkdir()
            with self.assertRaisesRegex(v.ValidationError,'outside_repositories'):v.validate('native',p/'inputs',None)
    def test_empty_inputs_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(v.ValidationError,'input_count'):v.validate('native',d,None)
    def test_case_insensitive_scan_and_symbolic_entries(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d);inside=root/'inputs';inside.mkdir()
            (inside/'upper.VIF').write_bytes(b'one')
            nested=inside/'nested';nested.mkdir();(nested/'mixed.ViF').write_bytes(b'two')
            outside=root/'outside';outside.mkdir();(outside/'unselected.vif').write_bytes(b'three')
            try:
                (inside/'linked.vif').symlink_to(outside/'unselected.vif')
                (inside/'linked-dir').symlink_to(outside,target_is_directory=True)
            except OSError:
                self.skipTest('symbolic links unavailable on this host')
            paths,skipped=v.discover_inputs(inside)
            self.assertEqual({p.name for p in paths},{'upper.VIF','mixed.ViF'})
            self.assertEqual(skipped,2)
            with self.assertRaisesRegex(v.ValidationError,'input_directory_is_symlink'):
                v.validate('native',inside/'linked-dir',None)
    def test_scan_errors_are_not_silently_omitted(self):
        def failed_walk(*args, **kwargs):
            kwargs['onerror'](OSError('private path'))
            return []
        with mock.patch.object(v.os,'walk',side_effect=failed_walk):
            with self.assertRaisesRegex(v.ValidationError,'^input_scan_failed$'):
                v.discover_inputs(pathlib.Path('private'))
    def test_uppercase_files_are_compared(self):
        reference=types.SimpleNamespace(FormatError=ValueError)
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d);(p/'a.VIF').write_bytes(b'authored')
            with mock.patch.object(v,'reference_document',return_value=expected_triangle()),mock.patch.object(v,'run_native',return_value=expected_triangle()):
                report=v.validate('native',p,reference)
            self.assertEqual(report['counts']['matched'],1)
            self.assertEqual(v.comparison_exit(report),0)
    def test_oversize_only_cannot_pass_without_comparison(self):
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d);(p/'large.vif').write_bytes(b'12345')
            with mock.patch.object(v,'MAX_INPUT',4),mock.patch.object(v,'run_native') as native:
                report=v.validate('native',p,None)
            native.assert_not_called()
            self.assertEqual(report['counts']['skipped_limit'],1)
            self.assertEqual(report['counts']['unique'],0)
            self.assertEqual(report['status'],'no_matches')
            self.assertEqual(v.comparison_exit(report),1)
    def test_all_native_refusals_are_not_semantic_success(self):
        reference=types.SimpleNamespace(FormatError=ValueError)
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d);(p/'a.vif').write_bytes(b'authored')
            with mock.patch.object(v,'reference_document',return_value=expected_triangle()),mock.patch.object(v,'run_native',return_value={'status':'refused'}):
                report=v.validate('native',p,reference)
            self.assertEqual(report['counts']['refused'],1)
            self.assertEqual(report['status'],'no_matches')
            self.assertEqual(v.comparison_exit(report),1)
    def test_aggregate_privacy_and_dedup(self):
        reference=types.SimpleNamespace(FormatError=ValueError)
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d);(p/'private-character-name.vif').write_bytes(b'author-one');(p/'private-copy.vif').write_bytes(b'author-one');(p/'other-name.vif').write_bytes(b'author-two')
            expected=expected_triangle()
            with mock.patch.object(v,'reference_document',return_value=expected),mock.patch.object(v,'run_native',return_value=expected):report=v.validate('native',p,reference)
            self.assertEqual(report['counts']['matched'],2);self.assertEqual(report['counts']['duplicates'],1)
            encoded=json.dumps(report)
            for secret in [d,'private-character','other-name','author-one','corners']:self.assertNotIn(secret,encoded.replace('matched_corners',''))
    def test_mismatch_is_not_acceptance_credit(self):
        reference=types.SimpleNamespace(FormatError=ValueError)
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d);(p/'a.vif').write_bytes(b'authored');wrong=expected_triangle();wrong['meshes'][0]['corners'][0][6][0]=1
            with mock.patch.object(v,'reference_document',return_value=expected_triangle()),mock.patch.object(v,'run_native',return_value=wrong):report=v.validate('native',p,reference)
            self.assertEqual(report['counts']['decoded'],1);self.assertEqual(report['counts']['matched'],0);self.assertEqual(report['counts']['mismatched'],1)
    def test_refused_and_reference_failure_distinct(self):
        reference=types.SimpleNamespace(FormatError=ValueError)
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d);(p/'a.vif').write_bytes(b'authored')
            with mock.patch.object(v,'reference_document',side_effect=ValueError),mock.patch.object(v,'run_native',return_value=expected_triangle()):report=v.validate('native',p,reference)
            self.assertEqual(report['counts']['unverified_native'],1);self.assertEqual(report['counts']['matched'],0)
            with mock.patch.object(v,'reference_document',return_value=expected_triangle()),mock.patch.object(v,'run_native',return_value={'status':'refused'}):report=v.validate('native',p,reference)
            self.assertEqual(report['counts']['refused'],1);self.assertEqual(report['counts']['mismatched'],0)
            self.assertEqual(report['counts']['reference_accepted_native_refused'],1)
            with mock.patch.object(v,'reference_document',side_effect=ValueError),mock.patch.object(v,'run_native',return_value={'status':'refused'}):report=v.validate('native',p,reference)
            self.assertEqual(report['counts']['both_refused'],1)
            self.assertEqual(report['counts']['reference_accepted_native_refused'],0)

class NativeTests(unittest.TestCase):
    def setUp(self):
        if NATIVE is None:self.skipTest('native transport not selected')
    def test_complete_authored_geometry_and_unicode_path(self):
        with tempfile.TemporaryDirectory() as d:
            path=pathlib.Path(d)/'authored-模型-雪.vif';path.write_bytes(triangle()[0])
            actual=v.run_native(NATIVE,path)
            self.assertIsNone(v.first_difference(expected_triangle(),actual))
    def test_malformed_native_refusal(self):
        with tempfile.TemporaryDirectory() as d:
            path=pathlib.Path(d)/'bad.vif';path.write_bytes(b'bad')
            self.assertEqual(v.run_native(NATIVE,path),{'schema':1,'status':'refused','reason':'format'})
    def test_native_missing_file_is_transport_failure(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(v.ValidationError,'transport_failed'):v.run_native(NATIVE,pathlib.Path(d)/'missing.vif')
    def test_native_argument_failure_no_path_disclosure(self):
        r=subprocess.run([str(NATIVE)],capture_output=True,timeout=10)
        self.assertEqual(r.returncode,3);self.assertEqual(json.loads(r.stdout)['reason'],'arguments')

if __name__=='__main__':
    parser=argparse.ArgumentParser(add_help=False);parser.add_argument('--native',type=pathlib.Path);args,remaining=parser.parse_known_args()
    NATIVE=args.native.resolve() if args.native else None
    unittest.main(argv=[sys.argv[0],*remaining])
