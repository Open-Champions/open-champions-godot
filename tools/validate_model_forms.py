#!/usr/bin/env python3
"""Optional private RTA VIF semantic comparison; public reports contain aggregates only.

The reference reader is loaded exclusively from a pinned git object. This checks
rest geometry and preserved metadata, not rendered reflection/shell/facing behavior.
"""
import argparse
import collections
import concurrent.futures
import hashlib
import json
import os
import pathlib
import struct
import subprocess
import types

COMMIT = 'c3d72647df7476f8cb6fa0105cbeccfcf4dc1a8f'
REFERENCE_SHA256 = '9cb38fadec899fde8ed457d7daf050bb7f6ebab80795494e27d697470c088870'
MAX_INPUT = 16 * 1024 * 1024
MAX_OUTPUT = 256 * 1024 * 1024
MAX_FILES = 20000

class ValidationError(Exception):
    """A bounded, privacy-safe transport failure."""

def load_reference(research_dir):
    try:
        result = subprocess.run(['git', '-C', str(research_dir), 'show',
                                 COMMIT + ':tools/model_info.py'],
                                capture_output=True, timeout=30, check=True)
    except (OSError, subprocess.SubprocessError) as exc:
        raise ValidationError('reference_unavailable') from exc
    if hashlib.sha256(result.stdout).hexdigest() != REFERENCE_SHA256:
        raise ValidationError('reference_pin_mismatch')
    module = types.ModuleType('pinned_model_info')
    exec(compile(result.stdout, 'pinned_model_info.py', 'exec'), module.__dict__)
    return module

def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]

def raw_tags(reference, data, chunk):
    """Keep exact GIF words using the reader's independent VIF field walker."""
    offset = chunk.off
    tags = []
    while offset < len(data):
        imm, count, command, _irq = reference.vif_code(data, offset)
        if command == reference.VIF_MSCAL:
            return tags
        if command & 0x60 == 0x60:
            items, used, _addr, _tops, _unsigned = reference.unpack_items(data, offset, command, count, imm)
            if ((command >> 2) & 3, command & 3) == (3, 0):
                tags.extend([[int(row[0]) & 0xffffffff | ((int(row[1]) & 0xffffffff) << 32),
                              int(row[2]) & 0xffffffff | ((int(row[3]) & 0xffffffff) << 32)] for row in items])
            offset += used
        else:
            offset += 4
    raise ValidationError('reference_unterminated_packet')

def reference_document(reference, data):
    m = reference.parse_model(data)
    if len(m.meshes) != 1:
        # Native contract currently exposes one table; never compare only a prefix.
        raise ValidationError('reference_multiple_lod_tables')
    document = dict(schema=1, status='decoded', flags=m.flags, hidden_mask=m.hidden_mask,
                    part_mask=m.part_mask, attachment_slot=m.attach_index,
                    has_variants=bool(m.flags & 2), alpha1=m.alpha1, fix2=m.fix2,
                    variants=[list(row) for row in m.variants], attach_points=[],
                    facing_points=[], facing_terminator_raw=None, effect_segment_raw=None,
                    sample_points=[], sample_block_raw=[], meshes=[])
    if not document['has_variants']:
        document['variants'] = [[0, 0] for _ in range(32)]
    for index, (joint, _matrix) in enumerate(m.attach):
        offset = m.tab['attach'] + index * 0x34
        document['attach_points'].append([joint, list(struct.unpack_from('<12I', data, offset + 4))])
    if m.tab['segment']:
        document['effect_segment_raw'] = list(struct.unpack_from('<6I', data, m.tab['segment']))
    if m.tab['samples']:
        offset = m.tab['samples']
        end = offset + 0x10 + struct.unpack_from('<I', data, offset)[0] * 16
        if end > len(data):
            raise ValidationError('reference_sample_packet_extent')
        document['sample_block_raw'] = list(data[offset:end])
        for index, _point in enumerate(m.samples):
            point = offset + 0x20 + index * 8 + (index // 45) * 0x10
            x, y, z, word = struct.unpack_from('<3hH', data, point)
            document['sample_points'].append([[x, y, z], word])
    if m.tab['facing']:
        # model_info does not decode this table. Compare the documented raw16
        # layout only; no real-disc facing example or evaluation is established.
        offset = m.tab['facing']
        first_mesh = min(row[0] for row in m.offsets)
        end = min([v for v in m.tab.values() if v > offset] + [first_mesh])
        while offset + 16 <= end:
            raw = list(data[offset:offset + 16])
            if raw[0] == 255:
                document['facing_terminator_raw'] = raw
                break
            document['facing_points'].append([raw, list(struct.unpack_from('<3h', data, offset+2)),
                                               list(struct.unpack_from('<3b', data, offset+8)),
                                               struct.unpack_from('<b', data, offset+15)[0]])
            offset += 16
        if document['facing_terminator_raw'] is None:
            raise ValidationError('reference_facing_terminator')
    for index, chunks in enumerate(m.meshes[0]):
        info = m.mesh_info[index]
        mesh = dict(index=index, visible=not bool(m.hidden_mask & (1 << index)), joint=info['joint'],
                    bounds_raw=list(info['bbox']), declared_triangles=info['w8'],
                    hull_points=[list(p) for p in m.hull[index]] if m.hull else [[0,0,0] for _ in range(8)],
                    packets=[], corners=[])
        for chunk in chunks:
            first_corner = len(mesh['corners'])
            triangles, _stitches = reference.chunk_triangles(chunk)
            for triangle in triangles:
                for vertex, strip_position in triangle:
                    position, normal, uv = chunk.pos[vertex], chunk.nrm[vertex], chunk.uv[strip_position]
                    influences = chunk.vertex_bones[vertex]
                    joints = [j for j, _weight in influences]
                    weights = [round(weight * 255) for _j, weight in influences]
                    if len(influences) > 4:
                        raise ValidationError('reference_skin_extent')
                    mesh['corners'].append([[bits(v/16) for v in position], [bits(v/127) for v in normal],
                                             [bits(v/16) for v in uv], joints+[0]*(4-len(joints)),
                                             weights+[0]*(4-len(weights)), len(influences), list(position),
                                             list(normal), list(uv), list(chunk.shell[vertex]) if chunk.shell is not None else None])
            mesh['packets'].append([chunk.program, chunk.uv_stride, chunk.n, chunk.tags[0]['nloop'],
                                    first_corner, len(mesh['corners'])-first_corner, raw_tags(reference, data, chunk)])
        document['meshes'].append(mesh)
    return document

def no_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValidationError('duplicate_transport_key')
        result[key] = value
    return result

def decode_response(data, returncode):
    if len(data) > MAX_OUTPUT:
        raise ValidationError('output_limit')
    try:
        text = data.decode('ascii')
        result = json.loads(text, object_pairs_hook=no_duplicate_keys,
                            parse_constant=lambda _v: (_ for _ in ()).throw(ValidationError('nonfinite_transport_number')))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ValidationError('invalid_transport_json') from exc
    if not isinstance(result, dict) or type(result.get('schema')) is not int or result['schema'] != 1:
        raise ValidationError('transport_schema')
    status = result.get('status')
    if status == 'decoded' and returncode == 0:
        return result
    if status == 'refused' and returncode == 2 and result.get('reason') in ('format','size'):
        if set(result) != {'schema','status','reason'}:
            raise ValidationError('refusal_schema')
        return result
    raise ValidationError('transport_failed')

def run_native(native, path, timeout=30, max_output=MAX_OUTPUT):
    try:
        process = subprocess.Popen([str(native), str(path)], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    except OSError as exc:
        raise ValidationError('native_unavailable') from exc
    def read_bounded():
        parts, total = [], 0
        while True:
            part = process.stdout.read(65536)
            if not part:
                return b''.join(parts)
            total += len(part)
            if total > max_output:
                raise ValidationError('output_limit')
            parts.append(part)
    pool = concurrent.futures.ThreadPoolExecutor(max_workers=1)
    future = pool.submit(read_bounded)
    try:
        data = future.result(timeout=timeout)
        returncode = process.wait(timeout=max(1, timeout))
        return decode_response(data, returncode)
    except (concurrent.futures.TimeoutError, subprocess.TimeoutExpired) as exc:
        raise ValidationError('native_timeout') from exc
    finally:
        if process.poll() is None:
            process.kill()
        process.wait()
        pool.shutdown(wait=True)
        process.stdout.close()

def first_difference(expected, actual, path='$'):
    """Type-sensitive comparison; public diagnostics retain field paths, not values."""
    if type(expected) is not type(actual):
        return path
    if isinstance(expected, dict):
        if expected.keys() != actual.keys():
            return path + '.keys'
        for key in expected:
            diff = first_difference(expected[key], actual[key], path+'.'+key)
            if diff: return diff
    elif isinstance(expected, list):
        if len(expected) != len(actual): return path+'.length'
        for x, y in zip(expected, actual):
            diff = first_difference(x, y, path+'[]')
            if diff: return diff
    elif expected != actual:
        return path
    return None

def outside_repositories(path):
    resolved = path.resolve()
    return not any((parent/'.git').exists() for parent in [resolved, *resolved.parents])

def discover_inputs(input_dir):
    """Scan regular VIF files without following symbolic files or directories."""
    paths = []
    skipped = 0
    def scan_error(_error):
        raise ValidationError('input_scan_failed')
    for directory, dirs, files in os.walk(input_dir, followlinks=False, onerror=scan_error):
        parent = pathlib.Path(directory)
        kept = []
        for name in dirs:
            if (parent/name).is_symlink():
                skipped += 1
            else:
                kept.append(name)
        dirs[:] = sorted(kept)
        for name in sorted(files):
            path = parent/name
            if path.is_symlink():
                skipped += 1
            elif path.suffix.lower() == '.vif' and path.is_file():
                paths.append(path)
                if len(paths) > MAX_FILES:
                    raise ValidationError('input_count')
    return sorted(paths), skipped

def bounded_read(path):
    with path.open('rb') as stream:
        return stream.read(MAX_INPUT + 1)

def comparison_exit(report):
    counts = report['counts']
    return int(counts['matched'] == 0 or any(counts[x] for x in
               ('mismatched', 'unverified_native', 'transport_errors', 'changed_input')))

def validate(native, input_dir, reference, timeout=30):
    input_dir = pathlib.Path(input_dir)
    if input_dir.is_symlink():
        raise ValidationError('input_directory_is_symlink')
    input_dir = input_dir.resolve()
    if not input_dir.is_dir() or not outside_repositories(input_dir):
        raise ValidationError('inputs_must_be_outside_repositories')
    paths, skipped_symlinks = discover_inputs(input_dir)
    if not paths or len(paths) > MAX_FILES:
        raise ValidationError('input_count')
    counts = collections.Counter(files_seen=len(paths), unique=0, duplicates=0, decoded=0,
                                 refused=0, reference_decoded=0, reference_refused=0, matched=0,
                                 mismatched=0, unverified_native=0, transport_errors=0, changed_input=0,
                                 skipped_symlinks=skipped_symlinks, skipped_limit=0,
                                 reference_accepted_native_refused=0, both_refused=0)
    forms = collections.Counter(); mismatches = collections.Counter(); transport = collections.Counter()
    seen = set(); digest = hashlib.sha256(); corners = packets = meshes = 0
    for path in paths:
        if not outside_repositories(path):
            raise ValidationError('inputs_must_be_outside_repositories')
        if path.is_symlink():
            counts['changed_input'] += 1
            continue
        if path.stat().st_size > MAX_INPUT:
            counts['skipped_limit'] += 1
            continue
        data = bounded_read(path)
        if len(data) > MAX_INPUT:
            counts['skipped_limit'] += 1
            continue
        fingerprint = hashlib.sha256(data).digest()
        if fingerprint in seen:
            counts['duplicates'] += 1
            continue
        seen.add(fingerprint); counts['unique'] += 1
        expected = None
        try:
            expected = reference_document(reference, data)
            counts['reference_decoded'] += 1
        except (reference.FormatError, struct.error, IndexError, ValidationError):
            counts['reference_refused'] += 1
        try:
            actual = run_native(native, path, timeout)
        except ValidationError as exc:
            counts['transport_errors'] += 1; transport[str(exc)] += 1
            continue
        if path.is_symlink() or hashlib.sha256(bounded_read(path)).digest() != fingerprint:
            counts['changed_input'] += 1
            continue
        digest.update(fingerprint)
        digest.update(json.dumps(actual,sort_keys=True,separators=(',',':')).encode('ascii'))
        if actual['status'] == 'refused':
            counts['refused'] += 1
            counts['reference_accepted_native_refused' if expected is not None else 'both_refused'] += 1
            continue
        counts['decoded'] += 1
        if expected is None:
            counts['unverified_native'] += 1
            continue
        difference = first_difference(expected,actual)
        if difference:
            counts['mismatched'] += 1; mismatches[difference] += 1
            continue
        counts['matched'] += 1
        for label, bit in [('reflection',0x10),('shell',0x20),('facing',0x40),('variants',2),('attach',4)]:
            if expected['flags'] & bit: forms[label] += 1
        for label in ('effect_segment_raw','sample_points'):
            if expected[label]:forms[label] += 1
        meshes += len(expected['meshes'])
        packets += sum(len(m['packets']) for m in expected['meshes'])
        corners += sum(len(m['corners']) for m in expected['meshes'])
    return dict(schema=1,status='compared' if counts['matched'] else 'no_matches',
                reference_commit=COMMIT,reference_sha256=REFERENCE_SHA256,
                counts=dict(counts),matched_forms=dict(forms),matched_meshes=meshes,
                matched_packets=packets,matched_corners=corners,comparison_sha256=digest.hexdigest(),
                mismatch_fields=dict(mismatches),transport_failures=dict(transport),
                scope='RTA rest geometry and preserved metadata only. No rendering, playback, model pairing, console or Norrath proof. Facing metadata uses documented raw layout; no real-disc facing example is established.')

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native',type=pathlib.Path,required=True)
    parser.add_argument('--research-dir',type=pathlib.Path,required=True)
    parser.add_argument('--input-dir',type=pathlib.Path,required=True)
    parser.add_argument('--report',type=pathlib.Path)
    parser.add_argument('--timeout',type=float,default=30)
    args=parser.parse_args(argv)
    if not 0 < args.timeout <= 300:
        parser.error('timeout must be in (0,300] seconds')
    try:
        if not args.native.is_file():
            raise ValidationError('native_unavailable')
        script_hash=hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest()
        binary_hash=hashlib.sha256(args.native.read_bytes()).hexdigest()
        report=validate(args.native.resolve(),args.input_dir,load_reference(args.research_dir),args.timeout)
        if hashlib.sha256(args.native.read_bytes()).hexdigest()!=binary_hash:
            raise ValidationError('native_changed_during_validation')
        report['native_transport_sha256']=binary_hash
        if hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest()!=script_hash:
            raise ValidationError('validator_changed_during_validation')
        report['validator_sha256']=script_hash
    except (ValidationError,OSError) as exc:
        code=str(exc) if isinstance(exc,ValidationError) else 'input_io'
        print(json.dumps(dict(schema=1,status='failed',reason=code)))
        return 2
    output=json.dumps(report,indent=2)+'\n'
    if args.report:
        args.report.write_text(output,encoding='utf-8')
    print(output,end='')
    return comparison_exit(report)

if __name__=='__main__':
    raise SystemExit(main())
