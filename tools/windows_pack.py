"""Inspect the pinned Godot 4.5.2 unencrypted embedded PCK without executing it.

Authored from core/io/file_access_pack.cpp at Godot's 4.5.2-stable tag.
Only the V3, relative-filebase layout produced by our presets is supported.
"""
import configparser
import hashlib
from pathlib import Path, PurePosixPath
import struct


def embedded_resources(executable):
    data = Path(executable).read_bytes()
    def read(offset, length):
        if offset < 0 or length < 0 or offset + length > len(data):
            raise ValueError('embedded pack range exceeds executable')
        return data[offset:offset + length]
    size, magic = struct.unpack('<Q4s', read(len(data) - 12, 12))
    if magic != b'GDPC':
        raise ValueError('missing embedded pack footer')
    start = len(data) - 12 - size
    header = struct.unpack('<4s5I2Q', read(start, 40))
    if header[:6] != (b'GDPC', 3, 4, 5, 2, 2):
        raise ValueError('expected pinned unencrypted Godot V3 embedded pack')
    base, directory = start + header[6], start + header[7]
    end = len(data) - 12
    if not start + 40 <= base <= directory < end:
        raise ValueError('invalid embedded pack directory')
    count = struct.unpack('<I', read(directory, 4))[0]
    if not 0 < count <= 10000:
        raise ValueError('invalid embedded resource count')
    cursor, resources = directory + 4, {}
    for _ in range(count):
        length = struct.unpack('<I', read(cursor, 4))[0]
        cursor += 4
        if not 0 < length <= 4096:
            raise ValueError('invalid embedded resource name length')
        name = read(cursor, length).rstrip(b'\0').decode('utf-8')
        cursor += length
        offset, size, digest, flags = struct.unpack('<QQ16sI', read(cursor, 36))
        cursor += 36
        path = PurePosixPath(name)
        if (path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name
                or name in resources or flags != 0 or cursor > end
                or base + offset + size > directory):
            raise ValueError('unsafe embedded resource entry')
        content = read(base + offset, size)
        if hashlib.md5(content).digest() != digest:
            raise ValueError('embedded resource checksum mismatch: ' + name)
        resources[name] = content
    return resources


def verify_windows_pack(executable, extension, arch):
    resources = embedded_resources(executable)
    required = ('project.binary', 'champions_archive.gdextension',
                '.godot/extension_list.cfg', 'scenes/session_sandbox.tscn.remap',
                'ui/archive_browser.tscn.remap')
    if not all(name in resources for name in required):
        raise ValueError('embedded pack is missing client resources')
    config = configparser.ConfigParser(interpolation=None)
    config.read_string(resources['champions_archive.gdextension'].decode('utf-8'))
    selected = config['libraries'][f'windows.release.{arch}.single'].strip('"')
    expected = f'champions_archive.windows.template_release.{arch}.dll'
    # OS_Windows::open_dynamic_library falls back to the executable's directory
    # for the basename. The official exporter copies the DLL there, not bin/.
    if selected != 'res://bin/' + expected or Path(extension) != Path(executable).parent / expected:
        raise ValueError('Windows DLL does not match descriptor and loader path')
    forbidden = {'.gob', '.lmp', '.world', '.iso', '.cso', '.elf', '.exe', '.dll'}
    if any(PurePosixPath(name).suffix.lower() in forbidden for name in resources):
        raise ValueError('unexpected game input or executable inside embedded pack')
    return {'format': 'embedded Godot V3', 'resource_count': len(resources),
            'descriptor_library': selected, 'loader_library': expected}
