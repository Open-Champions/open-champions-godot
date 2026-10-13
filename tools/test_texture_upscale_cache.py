import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

from fill_texture_upscale_cache import fill, png_size


def png(width, height):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    pixels = (b'\x00' + b'\xff\x00\x00\xff' * width) * height
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b'')


class CacheTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.cache = Path(self.temporary.name)
        source = png(1, 1)
        self.key = hashlib.sha256(b'oc-upscale-v1\n1\n1\nexternal-opaque-rgb\n2\nrealesrgan-x4plus\n' + b'\xff\x00\x00\xff').hexdigest()
        self.manifest = self.cache / (self.key + '.json')
        self.source = self.cache / (self.key + '.input.png')
        self.source.write_bytes(source)
        self.manifest.write_text(json.dumps(dict(key=self.key, width=1, height=1, factor=2, model='realesrgan-x4plus', alpha_mode='opaque-rgb', input_sha256=hashlib.sha256(source).hexdigest())))

    def test_cli_atomic_handoff(self):
        def fake_cli(args, **kwargs):
            self.assertTrue(kwargs['check'])
            self.assertEqual(kwargs['timeout'], 600)
            self.assertEqual(kwargs['cwd'], Path('/installed/tool').resolve().parent)
            self.assertEqual(args[-6:], ['-n', 'realesrgan-x4plus', '-s', '2', '-f', 'png'])
            self.assertEqual(Path(args[args.index('-i') + 1]).read_bytes(), png(1, 1))
            Path(args[args.index('-o') + 1]).write_bytes(png(2, 2))
        with patch('fill_texture_upscale_cache.subprocess.run', side_effect=fake_cli):
            result = fill(self.manifest, '/installed/tool')
        self.assertEqual(png_size(result), (2, 2))
        self.assertEqual(self.source.read_bytes(), png(1, 1))
        self.assertFalse(any(self.cache.glob('upscale-*')))

    def test_bad_output_preserves_existing(self):
        destination = self.cache / (self.key + '.png')
        destination.write_bytes(png(2, 2))
        def fake_cli(args, **kwargs):
            Path(args[args.index('-o') + 1]).write_bytes(png(3, 3))
        with patch('fill_texture_upscale_cache.subprocess.run', side_effect=fake_cli), self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool')
        self.assertEqual(destination.read_bytes(), png(2, 2))

    def test_tamper_refused_before_cli(self):
        self.source.write_bytes(png(1, 2))
        with patch('fill_texture_upscale_cache.subprocess.run') as runner, self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool')
        runner.assert_not_called()

    def test_repo_refused(self):
        (self.cache / '.git').write_text('synthetic worktree marker')
        with self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool')

    def test_settings_refused(self):
        data = json.loads(self.manifest.read_text())
        data['factor'] = 100000000
        self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool')

    def test_symlink_refused(self):
        target = self.cache / 'original.png'
        self.source.rename(target)
        try:
            self.source.symlink_to(target)
        except OSError:
            self.skipTest("This host does not permit symlink creation")
        with self.assertRaises(ValueError):
            fill(self.manifest, '/installed/tool')

if __name__ == '__main__':
    unittest.main()
