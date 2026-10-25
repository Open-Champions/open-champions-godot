"""Compare recipes against complete independent lossless recovery observations."""
import argparse,gzip,hashlib,json,pathlib,subprocess,tempfile

def run(exe,vectors,trace=None):
    manifest=json.loads(vectors.with_name(vectors.name[:-7]+'.json').read_text())
    root=pathlib.Path(__file__).resolve().parents[1]
    if manifest.get('suite')!='session-recovery' or manifest.get('policy')!='rta-session-recovery-v1':raise ValueError('fixture identity')
    generator=root/'tools/generate_session_recovery_vectors.py'
    if hashlib.sha256(generator.read_bytes()).hexdigest()!=manifest['generator_sha256']:raise ValueError('generator identity')
    for name,pin in manifest['helpers'].items():
        source=root/(pin['path'] if isinstance(pin,dict)else 'tools/'+name+'.py')
        expected=pin['sha256']if isinstance(pin,dict)else pin
        if hashlib.sha256(source.read_bytes()).hexdigest()!=expected:raise ValueError('authored helper identity: '+name)
    compressed=vectors.read_bytes()
    if hashlib.sha256(compressed).hexdigest()!=manifest['compressed_sha256']:raise ValueError('compressed fixture hash')
    data=gzip.decompress(compressed)
    if hashlib.sha256(data).hexdigest()!=manifest['vector_sha256']:raise ValueError('expanded fixture hash')
    lines=data.splitlines(keepends=True)
    if lines[0]!=b'# session_recovery_vectors_v1\n' or len(lines)-1!=manifest['count']:raise ValueError('fixture schema/count')
    keys=[tuple(row.split(b'\t')[:3])for row in lines[1:]]
    expected=set()
    for f in range(16):
        for name in ('ready','ordering','pause','release','reset','stop','direct_health_failure','direct_mana_failure','direct_pending','direct_no_sim'):
            for v in (range(14)if name=='ready'else(0,6,7)if name=='ordering'else(0,)):
                expected.add((name.encode(),str(f).encode(),str(v).encode()))
    if len(keys)!=len(set(keys))or set(keys)!=expected:raise ValueError('incomplete or duplicate recovery coverage')
    with tempfile.TemporaryDirectory(prefix='recovery-')as name:
        root=pathlib.Path(name);inputs=root/'recipes.tsv';outputs=root/'actual.tsv'
        inputs.write_bytes(b''.join(b'\t'.join(row.split(b'\t')[:3])+b'\n' for row in lines[1:]))
        result=subprocess.run([str(exe.resolve()),str(inputs),str(outputs)],capture_output=True,timeout=600)
        if result.returncode:raise ValueError(result.stderr.decode())
        expected=b''.join(lines[1:]);actual=outputs.read_bytes()
        if actual!=expected:
            aa=actual.splitlines();ee=expected.splitlines()
            for k,(a,e)in enumerate(zip(aa,ee)):
                if a!=e:
                    at=next((j for j,(x,y)in enumerate(zip(a,e))if x!=y),min(len(a),len(e)))
                    raise ValueError(f'row{k+1} {e[:60]!r} byte{at}: actual {a[max(0,at-50):at+100]!r} expected {e[max(0,at-50):at+100]!r}')
            raise ValueError('trace row count')
        digest=hashlib.sha256(actual).hexdigest()
        if digest!=manifest['expected_trace_sha256']:raise ValueError('trace hash')
        if trace:trace.write_bytes(actual)
        print(f'{manifest["count"]} exact recovery cases PASS; SHA256={digest}')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--exe',required=True,type=pathlib.Path);p.add_argument('--vectors',type=pathlib.Path,default=pathlib.Path('native/tests/vectors/session_recovery.tsv.gz'));p.add_argument('--trace',type=pathlib.Path);a=p.parse_args();run(a.exe,a.vectors,a.trace)
