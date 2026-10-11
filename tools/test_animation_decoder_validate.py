"""Optional private-input mode exercised solely with authored synthetic files."""
import json
import pathlib
import struct
import subprocess
import tempfile
import unittest
import sys
NATIVE = None

def fixture():
    result = bytearray(0x80)
    struct.pack_into('<5I', result, 0, 1, 20, 0x80, 0x78, 0x70)
    bits = []
    def put(v,n): bits.extend((v >> i) & 1 for i in range(n-1,-1,-1))
    put(7,4)
    for v in (64,0,0): put(v,8)
    put(13,4)
    for v in (4096,0,0,0): put(v,14)
    put(255,8)
    while len(bits)%32: bits.append(0)
    for i in range(0,len(bits),16):
        word=0
        for bit in bits[i:i+16]: word=(word<<1)|bit
        result.extend(struct.pack('<H',word))
    return bytes(result)

class PrivateDecoder(unittest.TestCase):
    def setUp(self):
        if NATIVE is None: self.skipTest('native private validation path supplied by dedicated CTest')
        self.temp=tempfile.TemporaryDirectory(prefix='Chämpions 测试 ')
        self.addCleanup(self.temp.cleanup)
        self.root=pathlib.Path(self.temp.name)
        (self.root/'one.ANM').write_bytes(fixture())
        (self.root/'bad.anm').write_bytes(b'bad')
        (self.root/'ignored.txt').write_bytes(b'not animation')
    def run_cli(self,*args):
        return subprocess.run([str(NATIVE),'--game','rta','--input-dir',str(self.root),*args],capture_output=True,timeout=60)
    def test_private_counts_no_paths(self):
        result=self.run_cli();self.assertEqual(result.returncode,0,result.stderr.decode())
        report=json.loads(result.stdout);self.assertEqual((report['files'],report['decoded'],report['refused'],report['samples']),(2,1,1,3))
        self.assertNotIn(str(self.root).encode(),result.stdout)
        self.assertEqual((self.root/'one.ANM').read_bytes(),fixture())
    def test_observations_retained_owner_and_literal_samples(self):
        result=self.run_cli('--observations','--fpu-config','15');self.assertEqual(result.returncode,0,result.stderr.decode())
        lines=result.stdout.splitlines();self.assertEqual(len(lines),3)
        valid=next(json.loads(line) for line in lines[:-1] if json.loads(line)[1]=='ok')
        self.assertEqual(valid[0],b'one.ANM'.hex());self.assertEqual(len(valid[3]),3)
        for i,step in enumerate(valid[3],1):
            self.assertEqual(step[1],'ok');self.assertEqual(step[2],[[[0x3f800000,0,0],[0,0,0,0x3f800000]]])
            tracks,slots,order=step[3];self.assertEqual(len(tracks),i);self.assertEqual(sorted(order),[1,2]);self.assertEqual(slots[tracks[-1][1][1]-1][4],i)
        self.assertEqual(json.loads(lines[-1][1:])['fpu_config'],15)
    def test_unknown_game_and_configuration_refused(self):
        for args in [('--game','norrath'),('--fpu-config','16'),('--fpu-config','-1'),('--unknown','1')]:
            with self.subTest(args=args): self.assertNotEqual(self.run_cli(*args).returncode,0)
    def test_empty_directory_has_explicit_zero_coverage(self):
        empty=self.root/'empty';empty.mkdir();result=self.run_cli('--input-dir',str(empty));self.assertEqual(result.returncode,0)
        self.assertEqual(json.loads(result.stdout)['files'],0)
    def test_symlink_input_refused_without_following(self):
        link=self.root/'link.anm'
        try: link.symlink_to(self.root/'one.ANM')
        except (OSError,NotImplementedError): self.skipTest('symlink unavailable on this host')
        result=self.run_cli();self.assertEqual(result.returncode,0)
        self.assertEqual(json.loads(result.stdout)['refused'],2)
    def test_filesystem_error_does_not_expose_private_path(self):
        link=self.root/'private-loop'
        try: link.symlink_to(link)
        except (OSError,NotImplementedError): self.skipTest('symlink unavailable on this host')
        result=self.run_cli('--input-dir',str(link))
        self.assertNotEqual(result.returncode,0)
        self.assertNotIn(str(self.root).encode(),result.stderr)
        self.assertNotIn(b'private-loop',result.stderr)
    def test_final_flush_failure_refused(self):
        destination=pathlib.Path('/dev/full')
        if not destination.exists(): self.skipTest('write-failure device unavailable on this host')
        with destination.open('wb') as output:
            result=subprocess.run([str(NATIVE),'--game','rta','--input-dir',str(self.root)],stdout=output,stderr=subprocess.PIPE,timeout=60)
        self.assertNotEqual(result.returncode,0)
        self.assertIn(b'report write failure',result.stderr)
    def test_small_report_file_size_limit_refused(self):
        try:
            import resource
            import signal
        except ImportError: self.skipTest('POSIX file-size limits unavailable on this host')
        def limit_output():
            signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE,(1,1))
        with (self.root/'report').open('wb') as output:
            result=subprocess.run([str(NATIVE),'--game','rta','--input-dir',str(self.root)],stdout=output,stderr=subprocess.PIPE,preexec_fn=limit_output,restore_signals=False,timeout=60)
        self.assertNotEqual(result.returncode,0)
        self.assertIn(b'report write failure',result.stderr)

if __name__=='__main__':
    if len(sys.argv)>1 and not sys.argv[1].startswith('-'): NATIVE=pathlib.Path(sys.argv.pop(1)).resolve()
    unittest.main()
