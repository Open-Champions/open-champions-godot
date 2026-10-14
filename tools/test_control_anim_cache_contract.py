"""New cache fields cannot disappear behind a successful comparison artifact."""
import ast, copy, gzip, hashlib, json, pathlib, re, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
def parse(s):
 return ast.literal_eval(re.sub(r'(?<=[\[,])([bs][0-9a-f]*)(?=[,\]])',r"'\1'",s).replace('_','None'))
def shape(v,n):
 if not isinstance(v,list) or len(v)!=n:raise ValueError('control_anim_cache complete shape')
def state(s):
 shape(s,6);shape(s[0],3);shape(s[1],7);c=s[2];shape(c,10)
 shape(c[0],4);shape(c[0][1],4);shape(c[0][1][0],6)
 r=c[1];shape(r,3);shape(r[0],4);shape(r[0][0],6)
 shape(r[1],1);shape(r[1][0],2);shape(r[1][0][1],5)
 d=r[2];shape(d,6);shape(d[0],2);shape(d[1],6)
 for clip in d[2]:shape(clip,12)
 for track in d[3]:shape(track,2);shape(track[1],9)
 for slot in d[4]:
  shape(slot,7);shape(slot[6],64)
  for joint in slot[6]:shape(joint,16)
 shape(c[2],1);shape(c[2][0],6);shape(c[2][0][0],4)
 def decoded(v):
  if v is None:return
  shape(v,4)
  for j in v[3]:shape(j,2);shape(j[0],3);shape(j[1],4)
 decoded(c[2][0][5])
 for op in c[7]:shape(op,12);shape(op[8],2);decoded(op[10])
 for command in s[4]:shape(command,6)
 def clips(v):
  for clip in v:shape(clip,12)
 clips(s[1][0])
def apply(s,d):
 s=copy.deepcopy(s)
 for entry in d:
  path=entry[0];value=entry[1]
  if len(entry)==3:
   t=s
   for i in path:t=t[i]
   raw=bytearray.fromhex(t[1:]);payload=bytes.fromhex(entry[2][1:]);raw[value:value+len(payload)]=payload;value='b'+raw.hex()
  if not path:s=copy.deepcopy(value);continue
  t=s
  for i in path[:-1]:t=t[i]
  t[path[-1]]=copy.deepcopy(value)
 return s
class Contract(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.path=ROOT/'native/tests/vectors/control_anim_cache.tsv.gz'
  cls.manifest=json.loads(cls.path.with_name('control_anim_cache.json').read_text())
  cls.packed=cls.path.read_bytes();cls.data=gzip.decompress(cls.packed)
  cls.rows=[line.split('\t')for line in cls.data.decode().splitlines()[1:]]
 def test_provenance(self):
  m=self.manifest
  self.assertEqual(m['suite'],'control_anim_cache');self.assertEqual(m['policy'],'rta-control-anim-cache-v1')
  for key,value in [('compressed_sha256',self.packed),('vector_sha256',self.data),('generator_sha256',(ROOT/'tools/generate_control_anim_cache_vectors.py').read_bytes())]:
   self.assertEqual(m[key],hashlib.sha256(value).hexdigest())
  for name,pin in m['helpers'].items():
   path=ROOT/(pin['path']if isinstance(pin,dict)else 'tools/'+name+'.py')
   self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),pin['sha256']if isinstance(pin,dict)else pin)
  self.assertEqual(m['references']['anim']['commit'],'f516ac798bf28375554fe53ffb31b74802844136')
  self.assertEqual(m['references']['core']['commit'],'41e847af5360724ab62d208594f8027ea3c1b686')
  self.assertEqual(m['specification']['commit'],'f2b996dded898f1b0b558b901cab875f775cfdc7')
 def test_coverage_and_all_current_states(self):
  expected={(n,str(f))for n in ('lifecycle','paused_pending','reset','key_failure','history_failure','full_cap')for f in range(16)}
  self.assertEqual(len(self.rows),96);self.assertEqual({tuple(r[:2])for r in self.rows},expected)
  for row in self.rows:
   initial,steps=parse(row[2]);state(initial);current=initial
   for _,delta in steps:current=apply(current,delta);state(current)
 def test_each_new_initial_field_omission_is_rejected(self):
  original=parse(self.rows[0][2])[0]
  paths=[[],[1],[2],[2,0],[2,0,1],[2,0,1,0],[2,1],[2,1,0],[2,1,0,0],
         [2,1,1,0],[2,1,1,0,1],[2,1,2],[2,1,2,0],[2,1,2,1],
         [2,1,2,2,0],[2,1,2,3,0],[2,1,2,3,0,1],[2,1,2,4,0],
         [2,1,2,4,0,6,63],[2,2,0],[2,2,0,0]]
  for path in paths:
   target=original
   for i in path:target=target[i]
   for k in range(len(target)):
    altered=copy.deepcopy(original);t=altered
    for i in path:t=t[i]
    del t[k]
    with self.subTest(path=path,field=k):
     with self.assertRaises((ValueError,IndexError,TypeError)):state(altered)
if __name__=='__main__':unittest.main()
