#!/usr/bin/env python3
"""Run an owner-installed realesrgan-ncnn-vulkan compatible CLI; never download."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

LIMIT = 64 * 1024 * 1024
MODEL = 'realesrgan-x4plus'


def png_size(path):
    if path.stat().st_size > LIMIT:
        raise ValueError('PNG exceeds 64 MiB')
    with path.open('rb') as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b'\x89PNG\r\n\x1a\n' or header[12:16] != b'IHDR':
        raise ValueError('Invalid PNG header')
    return struct.unpack('>II', header[16:24])


def fill(manifest_path, executable, godot):
    executable = Path(executable).resolve()
    manifest_path = Path(manifest_path).resolve()
    cache = manifest_path.parent
    # Reject every source checkout, even a different worktree. Cache belongs in user data.
    if any((parent / '.git').exists() for parent in [cache, *cache.parents]):
        raise ValueError('Cache must be outside a repository')
    if manifest_path.stat().st_size > 4096:
        raise ValueError('Manifest exceeds 4 KiB')
    data = json.loads(manifest_path.read_text())
    key = data['key']
    if not isinstance(key, str) or not re.fullmatch(r'[0-9a-f]{64}', key) or manifest_path.name != key + '.json':
        raise ValueError('Invalid cache identity')
    width, height, factor = (data[k] for k in ('width', 'height', 'factor'))
    if any(type(v) is not int for v in (width, height, factor)) or not (1 <= width <= 1024 and 1 <= height <= 1024 and factor in (2, 3, 4)) or data['model'] != MODEL or data.get('alpha_mode') != 'decoded-rgba' or data.get('fidelity_policy') != 'exact-box-palette-alpha-v1':
        raise ValueError('Unsupported cache settings')
    source = cache / (key + '.input.png')
    destination = cache / (key + '.png')
    if source.is_symlink() or destination.is_symlink():
        raise ValueError('Cache images must not be symlinks')
    if png_size(source) != (width, height):
        raise ValueError('Cache input dimensions differ from exported manifest')
    with source.open('rb') as stream:
        encoded = stream.read(LIMIT + 1)
    if len(encoded) > LIMIT or hashlib.sha256(encoded).hexdigest() != data['input_sha256']:
        raise ValueError('Cache input differs from exported manifest')
    # Temporary output and atomic publication stay in the user cache folder.
    with tempfile.TemporaryDirectory(prefix='upscale-', dir=cache) as temporary:
        input_snapshot = Path(temporary) / 'input.png'
        input_snapshot.write_bytes(encoded)
        output = Path(temporary) / 'result.png'
        subprocess.run([str(executable), '-i', str(input_snapshot), '-o', str(output), '-n', MODEL, '-s', str(factor), '-f', 'png'], check=True, timeout=600, cwd=executable.parent)
        if png_size(output) != (width * factor, height * factor):
            raise ValueError('CLI produced unexpected dimensions')
        # The same portable C++ gate used by the browser, via the real binding.
        # The helper never publishes an unchecked AI image. No tolerance override.
        project = Path(__file__).resolve().parents[1] / 'godot'
        try:
            subprocess.run([str(Path(godot).resolve()), '--headless', '--path', str(project),
                            '--script', 'res://tools/cache_fidelity.gd', '--',
                            str(input_snapshot), str(output), str(factor)],
                           check=True, timeout=120, capture_output=True)
        except subprocess.CalledProcessError as error:
            report = (error.stdout or b'').decode('utf-8', errors='replace').strip()
            raise ValueError('AI fidelity gate refused publication: ' + (report or 'checker failed')) from None
        os.replace(output, destination)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path, help='User-cache <key>.json exported by the browser')
    parser.add_argument('--executable', required=True, type=Path, help='Installed compatible upscaler binary')
    parser.add_argument('--godot', required=True, type=Path, help='Installed pinned Godot with built project extension; runs the native fidelity gate')
    args = parser.parse_args()
    print(fill(args.manifest, args.executable.resolve(), args.godot.resolve()))


if __name__ == '__main__':
    main()
