"""Authored embedded-pack fixtures; no Godot or game files required."""
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from windows_pack import embedded_resources, verify_windows_pack


def pack_bytes(resources):
    content = bytearray(112)
    entries = []
    for name, value in resources.items():
        offset = len(content) - 112
        content.extend(value)
        encoded = name.encode() + b'\0'
        entries.append(struct.pack('<I', len(encoded)) + encoded +
                       struct.pack('<QQ16sI', offset, len(value), hashlib.md5(value).digest(), 0))
    directory = len(content)
    struct.pack_into('<4s5I2Q', content, 0, b'GDPC', 3, 4, 5, 2, 2, 112, directory)
    content.extend(struct.pack('<I', len(entries)) + b''.join(entries))
    return b'authored executable prefix' + content + struct.pack('<Q4s', len(content), b'GDPC')


class WindowsPackTests(unittest.TestCase):
    def test_valid_pack_and_exact_loader_path(self):
        filename = 'champions_archive.windows.template_release.x86_64.dll'
        resources = {name: b'authored' for name in (
            'project.binary', '.godot/extension_list.cfg',
            'scenes/session_sandbox.tscn.remap', 'ui/archive_browser.tscn.remap')}
        resources['champions_archive.gdextension'] = (
            '[libraries]\nwindows.release.x86_64.single="res://bin/' + filename + '"\n').encode()
        with tempfile.TemporaryDirectory() as temp:
            exe = Path(temp) / 'OpenChampions.exe'
            exe.write_bytes(pack_bytes(resources))
            self.assertEqual(embedded_resources(exe), resources)
            self.assertEqual(verify_windows_pack(exe, exe.parent / filename, 'x86_64')['resource_count'], 5)
            with self.assertRaisesRegex(ValueError, 'loader path'):
                verify_windows_pack(exe, exe.parent / 'unrelated' / filename, 'x86_64')
            resources['private.gob'] = b'authored forbidden-file fixture'
            exe.write_bytes(pack_bytes(resources))
            with self.assertRaisesRegex(ValueError, 'game input'):
                verify_windows_pack(exe, exe.parent / filename, 'x86_64')

    def test_truncated_corrupt_and_unsafe_packs_fail(self):
        valid = pack_bytes({'authored.txt': b'fixture'})
        corrupt = valid.replace(b'fixture', b'changed')
        for data in (valid[:-8], corrupt, pack_bytes({'../escape': b'fixture'}),
                     pack_bytes({'unsafe.exe': b'fixture'})[:40]):
            with self.subTest(data=data[:20]), tempfile.TemporaryDirectory() as temp:
                exe = Path(temp) / 'OpenChampions.exe'
                exe.write_bytes(data)
                with self.assertRaises(ValueError):
                    embedded_resources(exe)


if __name__ == '__main__':
    unittest.main()
