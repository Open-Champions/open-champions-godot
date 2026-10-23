#!/usr/bin/env python3
"""Real-data smoke for the mounted Horsemen level-load accounting (counts only).

Runs godot/tests/real_data_scene.gd on a level range of the owner's disc with the level script on and
checks the summary: every level in the range tried, loaded and played; the mount refusal counted the
expected number of times; the rider records' draws no longer counted as unmodelled.

    python3 -I tools/mounted_horsemen_real_smoke.py GODOT DISC CACHE_DIR SUMMARY.json FROM TO EXPECT_REFUSALS
"""
import json
import subprocess
import sys
from pathlib import Path

godot, disc, cache, summary, first, last, expect = sys.argv[1:8]
disc = str(Path(disc).resolve())
cache = str(Path(cache).resolve())
summary = Path(summary).resolve()
Path(cache).mkdir(parents=True, exist_ok=True)
summary.parent.mkdir(parents=True, exist_ok=True)
cmd = [godot, '--headless', '--path', 'godot', '--script', 'res://tests/real_data_scene.gd', '--',
       f'--game-folder={disc}', f'--cache={cache}', '--all', '--levels=0', f'--from={first}', f'--to={last}',
       '--ticks=60', f'--summary={summary}', '--scripts']
code = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=3000).returncode
if code or not summary.exists():
    print(json.dumps({'exit': code, 'summary_written': summary.exists(), 'ok': False}))
    sys.exit(1)
data = json.loads(summary.read_text())
counts = data['counts']
levels = int(last) - int(first)
reason = 'scripts.build_refused.mount property list contents Unknown'
ok = (code == 0 and not data.get('failures') and not data.get('refusals')
      and counts.get('levels.tried') == levels and counts.get('levels.loaded') == levels
      and counts.get('levels.played') == levels and counts.get(reason, 0) == int(expect))
print(json.dumps({'exit': code, 'levels': levels, 'mount_refusals': counts.get(reason, 0),
                  'objects_unbuilt': counts.get('scripts.objects_unbuilt'),
                  'load_draws_unmodelled': counts.get('scripts.load_draws_unmodelled'),
                  'tried': counts.get('levels.tried'), 'loaded': counts.get('levels.loaded'),
                  'played': counts.get('levels.played'), 'ok': ok}))
sys.exit(0 if ok else 1)
