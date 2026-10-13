import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

from fill_texture_upscale_cache import fill, png_size


def png(width, height, color=b'\xff\x00\x00\xff'):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    pixels = (b'\x00' + color * width) * height
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b'')


class CacheTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.cache = Path(self.temporary.name)
        source = png(1, 1)
        self.key = hashlib.sha256(b'oc-upscale-v2\n1\n1\nexternal-rgba\n2\nrealesrgan-x4plus\n' + b'\xff\x00\x00\xff').hexdigest()
        self.manifest = self.cache / (self.key + '.json')
        self.source = self.cache / (self.key + '.input.png')
        self.source.write_bytes(source)
        self.manifest.write_text(json.dumps(dict(key=self.key, width=1, height=1, factor=2, model='realesrgan-x4plus', alpha_mode='decoded-rgba', fidelity_policy='exact-box-palette-alpha-v1', input_sha256=hashlib.sha256(source).hexdigest())))

    def test_cli_atomic_handoff(self):
        def fake_cli(args, **kwargs):
            if '--headless' in args:
                self.assertEqual(args[-1], '2')
                self.assertEqual(kwargs['timeout'], 120)
                return
            self.assertTrue(kwargs['check'])
            self.assertEqual(kwargs['timeout'], 600)
            self.assertEqual(kwargs['cwd'], Path('/installed/tool').resolve().parent)
            self.assertEqual(args[-6:], ['-n', 'realesrgan-x4plus', '-s', '2', '-f', 'png'])
            self.assertEqual(Path(args[args.index('-i') + 1]).read_bytes(), png(1, 1))
            Path(args[args.index('-o') + 1]).write_bytes(png(2, 2))
        with patch('fill_texture_upscale_cache.subprocess.run', side_effect=fake_cli):
            result = fill(self.manifest, '/installed/tool', '/installed/godot')
        self.assertEqual(png_size(result), (2, 2))
        self.assertEqual(self.source.read_bytes(), png(1, 1))
        self.assertFalse(any(self.cache.glob('upscale-*')))

    @unittest.skipUnless(os.environ.get('GODOT_FIDELITY_RUNTIME'), 'Optional real Godot binding check')
    def test_real_native_publication_gate(self):
        import subprocess
        tool = self.cache / 'synthetic-cli'
        tool.write_text('#!/usr/bin/env python3\nimport sys\nfrom pathlib import Path\nPath(sys.argv[sys.argv.index("-o")+1]).write_bytes(' + repr(png(2, 2)) + ')\n')
        tool.chmod(0o755)
        result = fill(self.manifest, tool, os.environ['GODOT_FIDELITY_RUNTIME'])
        self.assertEqual(png_size(result), (2, 2))
        # Same dimensions, wrong colors: native gate must refuse publication.
        invalid = png(2, 2, b'\x00\xff\x00\xff')
        tool.write_text('#!/usr/bin/env python3\nimport sys\nfrom pathlib import Path\nPath(sys.argv[sys.argv.index("-o")+1]).write_bytes(' + repr(invalid) + ')\n')
        with self.assertRaises(ValueError):
            fill(self.manifest, tool, os.environ['GODOT_FIDELITY_RUNTIME'])
        self.assertEqual(result.read_bytes(), png(2, 2))

    def test_bad_output_preserves_existing(self):
        destination = self.cache / (self.key + '.png')
        destination.write_bytes(png(2, 2))
        def fake_cli(args, **kwargs):
            Path(args[args.index('-o') + 1]).write_bytes(png(3, 3))
        with patch('fill_texture_upscale_cache.subprocess.run', side_effect=fake_cli), self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool', '/installed/godot')
        self.assertEqual(destination.read_bytes(), png(2, 2))

    def test_failed_fidelity_preserves_existing(self):
        import subprocess
        destination = self.cache / (self.key + '.png')
        destination.write_bytes(png(2, 2))
        def fake_cli(args, **kwargs):
            if '--headless' in args:
                raise subprocess.CalledProcessError(1, args)
            Path(args[args.index('-o') + 1]).write_bytes(png(2, 2))
        with patch('fill_texture_upscale_cache.subprocess.run', side_effect=fake_cli), self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool', '/installed/godot')
        self.assertEqual(destination.read_bytes(), png(2, 2))

    def test_tamper_refused_before_cli(self):
        self.source.write_bytes(png(1, 2))
        with patch('fill_texture_upscale_cache.subprocess.run') as runner, self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool', '/installed/godot')
        runner.assert_not_called()

    def test_repo_refused(self):
        (self.cache / '.git').write_text('synthetic worktree marker')
        with self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool', '/installed/godot')

    def test_settings_refused(self):
        data = json.loads(self.manifest.read_text())
        data['factor'] = 100000000
        self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool', '/installed/godot')

    def test_symlink_refused(self):
        target = self.cache / 'original.png'
        self.source.rename(target)
        try:
            self.source.symlink_to(target)
        except OSError:
            self.skipTest("This host does not permit symlink creation")
        with self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool', '/installed/godot')

if __name__ == '__main__':
    unittest.main()
