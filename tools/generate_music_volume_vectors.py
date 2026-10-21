#!/usr/bin/env python3
"""Exact g_musicVolume 0x4f8944 fixtures from research commit 22408a1, never from native output.

The pinned reference tools/ref_music_volume.py (docs/spec/music-volume.md, executed by
tools/difftest_music_volume.py against the original with 0 mismatches) runs on authored
synthetic float patterns. Only the music word of the options image g_savedConfig
(+0x08) and the live global are compared; the other 16 option scalars, the button rows
and image +0x00 are outside this fixture. The reference is imported from immutable git
blobs copied into a temporary directory, so the research checkout is never written.

Records (all words hex, one record per line):
  CONST  elf_live elf_image default gate step timer_step lower_mask raise_mask
  DEFAULTS which live image -> live image                 config_setDefaults(which)
  CORE   image duck status -> live sent channel           game_addCoreTasks + applyVolume
  BOOT   mode image file or_bits -> image word0           memcard_browser step 29
  OPTS   state live image buffer -> live image            memcard_optionsScreen
  SLIDE  value held -> value                              one active slider step
  CALL   value held timer -> value timer active           one menuList_handleInput call
  WALK   start held n v1 .. vn                            held until the clamp
  COMMIT live image query nosave force -> count image save  config_commitOptions (music)
  GATE   volume -> passes
  APPLY  volume duck status -> sent channel               music_applyVolume
  CHAIN  file|none -> elf defaults card session request passes
"""
import gzip
import hashlib
import importlib
import json
import random
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
COMMIT = '22408a1d21aa073e7b2e088892c8742b098d83fc'
RESEARCH = Path.home() / 'Projects/champions-re-wt/research-combat-ctors'
PINS = {
    'docs/spec/music-volume.md': 'bd0340fe6bc8b2e5968deeb61a7876794ab0ad56cafc21f49b2c32b3210eee84',
    'tools/ref_music_volume.py': '3e657b3ba9bc59ce50cbd98343efe0754d82ad6a406ad18189be2b24dc7d20e9',
    'tools/test_ref_music_volume.py': '32beeec17a04e82ea4fa95d3fe7a43bdeb10a97fc8fb9475256523c93853f62f',
    'tools/difftest_music_volume.py': '5ef0390e13c06dc7aa79c3dec108c8d150b022308afc7af12c9e4bce66d2b905',
    'tools/ref_core.py': '0c12eeb51d12e8cdd8487ea1392693d27933b531a1bf69d15dbfa12d57726123',
}
SEED = 0x4F8944          # authored seed for the random patterns of this fixture
M32 = 0xFFFFFFFF


def blobs():
    out = {}
    for path, want in PINS.items():
        blob = subprocess.check_output(['git', '-C', str(RESEARCH), 'show', f'{COMMIT}:{path}'])
        got = hashlib.sha256(blob).hexdigest()
        if got != want:
            raise SystemExit(f'{path} at {COMMIT} hashed {got}, pin is {want}')
        out[path] = blob
    return out


def word(value):
    return f'{value & M32:08x}'


def words(values):
    return ' '.join(word(v) for v in values)


def f32(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


FIXED = (0, 0x80000000, 0x3D4CCCCC, 0x3D4CCCCB, 0x3D4CCCCD, 0x3D4CCC08, 0x3F800000,
         0x3F000000, 0x3F7FFFFF, 0x3F800001, 0x3FC00000, 0xBF800000, 0xBE800000,
         0x7FFFFFFF, 0xFFFFFFFF, 0x7F800000, 0x00000001, 0x3C088888, 0x3C088887,
         0x3F7DDDDE, 0x3E800000)


def pattern(rng):
    pick = rng.random()
    if pick < 0.2:
        return rng.choice(FIXED)
    if pick < 0.7:
        return f32(rng.uniform(0.0, 1.0))
    if pick < 0.85:
        return f32(rng.uniform(-2.0, 4.0))
    return rng.getrandbits(32)


def status(rng):
    return rng.choice((0, 0x20, 0x21, 0x1, 0xFFFFFFFF, rng.getrandbits(32)))


def anchors(M, R):
    """The executed facts of docs/spec/music-volume.md, checked on the pinned reference."""
    down = M.slider_walk(M.DEFAULT_MUSIC, 0x400000)
    up = M.slider_walk(M.DEFAULT_MUSIC, 0x800000)
    below = next(i + 1 for i, v in enumerate(down) if not M.music_gate_passes(v))
    facts = (M.STATIC_MUSIC == 0x3F800000, M.DEFAULT_MUSIC == 0x3F000000,
             M.boot_music_volume(None)[-1][1] == 0x3F000000,
             M.boot_music_volume(0x3FC00000)[-1][1] == 0x3FC00000,
             down[-1] == 0 and len(down) - 1 == 60, below == 54 and down[53] == 0x3D4CCC08,
             up[-1] == R.ONE and len(up) - 1 == 61, M.music_gate_passes(M.MUSIC_GATE),
             not M.music_gate_passes(M.MUSIC_GATE - 1))
    if not all(facts):
        raise SystemExit(f'pinned reference disagrees with the documented anchors: {facts}')


def generate(M, R):
    R.set_fpu('ps2')
    anchors(M, R)
    rng = random.Random(SEED)
    lines = ['# g_musicVolume 0x4f8944, research ' + COMMIT[:7]]
    lines.append('CONST ' + words([M.STATIC_MUSIC, M.STATIC_IMAGE_MUSIC, M.DEFAULT_MUSIC,
                                   M.MUSIC_GATE, M.SLIDER_STEP, M.TIMER_STEP,
                                   M.LEFT_MASK, M.RIGHT_MASK]))
    # config_setDefaults(which): music words only, from the reference's write set.
    for which in (-1, 0, 1, 2, 3, 4, 7, -2):
        for live, image in ((M.STATIC_MUSIC, M.STATIC_IMAGE_MUSIC), (0x3E800000, 0x3FC00000)):
            writes, _calls = M.config_set_defaults(which, 1)
            new_live = writes.get(M.G_MUSIC_VOLUME, live)
            new_image = writes.get(M.G_SAVED_CONFIG + 8, image)
            lines.append('DEFAULTS ' + words([which, live, image, new_live, new_image]))
    # game_addCoreTasks: image +0x08 -> 0x4f8944 verbatim, then music_applyVolume.
    core = [0, 0x80000000, 0xBF800000, 0x3FC00000, 0x7FFFFFFF, M.MUSIC_GATE,
            M.MUSIC_GATE - 1, M.DEFAULT_MUSIC, R.ONE]
    core += [pattern(rng) for _ in range(40)]
    for index, image in enumerate(core):
        duck = R.ONE if index < 9 else pattern(rng)
        st = 0x20 if index < 9 else status(rng)
        writes, calls = M.game_add_core_tasks({0x08: image}, duck, st)
        sent = len(calls) > 1
        channel = calls[1][2] if sent else 0
        lines.append('CORE ' + words([image, duck, st, writes[M.G_MUSIC_VOLUME], int(sent), channel]))
    # memcard_browser step 29: mode 1 copies the file over the image, mode 0 does not.
    for mode in (0, 1, 2, 0xFFFFFFFF):
        for or_bits in (-1, 0, 0x10):
            for file_word in (0, 0x3FC00000, 0xBF800000, 0x7FFFFFFF, pattern(rng)):
                image_word = rng.choice((M.DEFAULT_MUSIC, pattern(rng)))
                image = bytearray(M.CONFIG_SIZE)
                image[8:12] = struct.pack('<I', image_word)
                buffer = bytearray(M.CONFIG_SIZE)
                buffer[8:12] = struct.pack('<I', file_word)
                out = M.browser_config_copy(mode, bytes(image), bytes(buffer), or_bits)
                lines.append('BOOT ' + words([mode, image_word, file_word, or_bits,
                                              struct.unpack_from('<I', out, 8)[0],
                                              struct.unpack_from('<I', out, 0)[0]]))
    # memcard_optionsScreen: only state 2 writes the music words.
    for state in (-1, 0, 1, 2, 3, 4):
        for buffer in (0, 0x3E800000, 0x3FC00000, 0xBF800000, pattern(rng)):
            live, image = pattern(rng), pattern(rng)
            writes, _calls = M.options_screen(state, {0x08: buffer}, 2, 1, R.ONE, 0)
            lines.append('OPTS ' + words([state, live, image, buffer,
                                          writes.get(M.G_MUSIC_VOLUME, live),
                                          writes.get(M.G_SAVED_CONFIG + 8, image)]))
    # One active slider step.
    masks = (0, 0x400000, 0x8000, 0x800000, 0x2000, 0x408000, 0x802000, 0x402000,
             0x808000, 0xC0A000, 0xFF7F5FFF)
    for value in FIXED:
        for held in masks:
            lines.append('SLIDE ' + words([value, held, M.slider_step(value, held)]))
    for _ in range(200):
        value, held = pattern(rng), rng.choice(masks) | (rng.getrandbits(32) if rng.random() < 0.2 else 0)
        lines.append('SLIDE ' + words([value, held, M.slider_step(value, held)]))
    # Whole menuList_handleInput calls with the repeat timer.
    timers = (0, M.TIMER_STEP, 0x3F000000, 0x3F4CCCCD, 0x3F59999A, R.ONE, 0x3F400000, 0x3F7FFFFF)
    for timer in timers:
        for held in (0x400000, 0x800000, 0):
            for value in (M.DEFAULT_MUSIC, 0, R.ONE, 0x3FC00000):
                v, t, active = M.menu_slider_call(value, held, timer)
                lines.append('CALL ' + words([value, held, timer, v, t, int(active)]))
    value, timer = M.DEFAULT_MUSIC, 0
    for _ in range(12):   # the cursor lands with the timer at 0: four idle calls, then active
        v, t, active = M.menu_slider_call(value, 0x400000, timer)
        lines.append('CALL ' + words([value, 0x400000, timer, v, t, int(active)]))
        value, timer = v, t
    # Whole walks from the default and from the clamps and out-of-range file values.
    for start in (M.DEFAULT_MUSIC, 0, R.ONE, 0x3FC00000, 0xBE800000, 0x3D4CCCCC):
        for held in (0x400000, 0x800000, 0x8000, 0x2000):
            walk = M.slider_walk(start, held)
            lines.append('WALK ' + words([start, held, len(walk)] + walk))
    # config_commitOptions, music part (every other option equal to its image word).
    pairs = [(M.DEFAULT_MUSIC, M.DEFAULT_MUSIC), (0x3E800000, M.DEFAULT_MUSIC),
             (0, 0x80000000), (0x3D4CCC08, M.DEFAULT_MUSIC)]
    pairs += [(pattern(rng), pattern(rng)) for _ in range(12)]
    for live, image in pairs:
        for query in (0, 1):
            for nosave in (0, 1):
                for force in (0, 1):
                    count, new_image, save = M.config_commit_music(live, image, query, nosave, force)
                    lines.append('COMMIT ' + words([live, image, query, nosave, force,
                                                    count, new_image, int(save)]))
    # The gate.
    gate = list(FIXED) + [pattern(rng) for _ in range(100)]
    for volume in gate:
        lines.append('GATE ' + words([volume, int(M.music_gate_passes(volume))]))
    # music_applyVolume.
    for _ in range(60):
        volume, duck, st = pattern(rng), pattern(rng), status(rng)
        calls = M.music_apply_volume(volume, duck, st)
        sent = len(calls) > 1
        lines.append('APPLY ' + words([volume, duck, st, int(sent), calls[1][2] if sent else 0]))
    # The boot chain: ELF image, save_init defaults, card copy, session start, first request.
    files = [None, 0, M.MUSIC_GATE, M.MUSIC_GATE - 1, R.ONE, f32(1.5), 0xBF800000,
             f32(0.25), M.DEFAULT_MUSIC, 0x7FFFFFFF]
    files += [pattern(rng) for _ in range(10)]
    for music in files:
        steps = [value for _name, value in M.boot_music_volume(music)]
        passes = M.music_gate_passes(steps[-1])
        head = 'none' if music is None else word(music)
        lines.append('CHAIN ' + head + ' ' + words(steps + [int(passes)]))
    return ('\n'.join(lines) + '\n').encode()


def load_reference(tmp, data):
    for path, blob in data.items():
        if path.startswith('tools/'):
            (Path(tmp) / Path(path).name).write_bytes(blob)
    sys.path.insert(0, tmp)
    try:
        R = importlib.import_module('ref_core')
        M = importlib.import_module('ref_music_volume')
    finally:
        sys.path.remove(tmp)
    return M, R


def main():
    data = blobs()
    with tempfile.TemporaryDirectory(prefix='music-volume-ref-') as tmp:
        M, R = load_reference(tmp, data)
        text = generate(M, R)
    out = Path(__file__).resolve().parents[1] / 'native/tests/vectors'
    out.mkdir(parents=True, exist_ok=True)
    blob = gzip.compress(text, mtime=0)
    (out / 'music_volume.txt.gz').write_bytes(blob)
    records = sum(1 for line in text.splitlines() if line and not line.startswith(b'#'))
    meta = {
        'schema': 'music-volume-v1',
        'research_commit': COMMIT,
        'references': PINS,
        'seed': SEED,
        'vector_sha256': hashlib.sha256(blob).hexdigest(),
        'content_sha256': hashlib.sha256(text).hexdigest(),
        'records': records,
    }
    (out / 'music_volume.json').write_text(json.dumps(meta, indent=2) + '\n')
    print(f'{records} records, {len(text)} bytes')


if __name__ == '__main__':
    main()
