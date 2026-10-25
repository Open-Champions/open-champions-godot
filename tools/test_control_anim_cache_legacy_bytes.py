"""Old writers compared against the actual immutable integration-base bytes.
This is a compatibility regression, separate from the Python animation oracle.
"""
import argparse,hashlib,json,pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
def run(exe):
 m=json.loads((ROOT/'native/tests/vectors/control_anim_cache_legacy_bytes.json').read_text())
 if (m['baseline_commit']!='c538b65175363d2ed232cc94ae46ed8b94b815eb' or m['blocks']!=192 or
     m['baseline_native_save_sha256']!='21063d2668b6c37551c6388321992a14e98b3bccd8accf5d74afa50cbf0c0b47' or
     m['aggregate_sha256']!='e24cf59c79c3653a755706c25dd8d1962d897ffc65edd199f4b35f60022823fb' or
     m['aggregate_bytes']!=360326752):raise ValueError('legacy identity')
 if hashlib.sha256((ROOT/'native/tests/control_anim_cache_legacy_bytes.cpp').read_bytes()).hexdigest()!=m['probe_sha256']:raise ValueError('legacy probe identity')
 scratch=ROOT/'work/control-anim-cache';scratch.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='legacy-',dir=scratch)as directory:
  path=pathlib.Path(directory)/'bytes.bin'
  subprocess.run([str(exe.resolve()),str(path)],check=True,timeout=600)
  h=hashlib.sha256();blocks=0
  with path.open('rb')as stream:
   while count:=stream.read(4):
    if len(count)!=4:raise ValueError('truncated compatibility block')
    h.update(count);n=int.from_bytes(count,'little')
    if n<24 or n>32*1024*1024:raise ValueError('compatibility block bounds')
    data=stream.read(n)
    if len(data)!=n:raise ValueError('truncated compatibility bytes')
    h.update(data);blocks+=1
  if blocks!=m['blocks'] or path.stat().st_size!=m['aggregate_bytes'] or h.hexdigest()!=m['aggregate_sha256']:raise ValueError('disabled save byte contract changed')
  print(f'{blocks} disabled save blocks identical to {m["baseline_commit"]}; SHA256={h.hexdigest()}')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--exe',required=True,type=pathlib.Path);run(p.parse_args().exe)
