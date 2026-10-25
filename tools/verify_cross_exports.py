#!/usr/bin/env python3
"""Inspect the Windows/Linux exports without executing target binaries."""

import configparser
from pathlib import Path

from windows_pack import embedded_resources, verify_windows_pack


ROOT = Path(__file__).resolve().parent.parent
FORBIDDEN_GAME_SUFFIXES = {
    '.gob', '.lmp', '.world', '.tex', '.etex', '.bnk', '.iso', '.cso', '.elf',
}
ALLOWED_BIN_RESOURCES = {'.godot/uid_cache.bin', 'tests/fixtures/level_texture_synthetic.bin'}


def check(platform, player, extension_suffix):
    folder = ROOT / 'build' / 'exports' / f'{platform}-x64'
    libraries = {
        f'champions_{name}.{platform}.template_release.x86_64.{extension_suffix}'
        for name in ('archive', 'level_texture')
    }
    expected_files = {player, *libraries}
    actual_files = {entry.name for entry in folder.iterdir()}
    if actual_files != expected_files or any(not entry.is_file() for entry in folder.iterdir()):
        raise ValueError(f'{platform}: expected only player and two release libraries: {actual_files}')

    for library in libraries:
        if (folder / library).read_bytes() != (ROOT / 'godot' / 'bin' / library).read_bytes():
            raise ValueError(f'{platform}: exported library differs from cross-build: {library}')

    resources = embedded_resources(folder / player)
    if not {'project.binary', 'champions_archive.gdextension',
            'champions_level_texture.gdextension'} <= resources.keys():
        raise ValueError(f'{platform}: missing project or extension descriptor')
    for name in ('archive', 'level_texture'):
        config = configparser.ConfigParser(interpolation=None)
        config.read_string(resources[f'champions_{name}.gdextension'].decode('utf-8'))
        expected = f'res://bin/champions_{name}.{platform}.template_release.x86_64.{extension_suffix}'
        actual = config['libraries'][f'{platform}.release.x86_64.single'].strip('"')
        if actual != expected:
            raise ValueError(f'{platform}: {name} descriptor selects {actual}, expected {expected}')
    if platform == 'windows':
        verify_windows_pack(folder / player,
                            folder / 'champions_archive.windows.template_release.x86_64.dll',
                            'x86_64')

    bad_names = [name for name in resources
                 if Path(name).suffix.lower() in FORBIDDEN_GAME_SUFFIXES
                 or (Path(name).suffix.lower() == '.bin' and name not in ALLOWED_BIN_RESOURCES)]
    if bad_names:
        raise ValueError(f'{platform}: game data in embedded pack: {len(bad_names)} entries')

    private_markers = (str(Path.home()).encode(), b'private-validation')
    for path in folder.iterdir():
        if any(marker in path.read_bytes() for marker in private_markers):
            raise ValueError(f'{platform}: private path in {path.name}')
    for name, data in resources.items():
        if any(marker in data for marker in private_markers):
            raise ValueError(f'{platform}: private path in embedded resource {name}')
    print(f'{platform}-x64: PASS; 3 files, {len(resources)} checksummed resources, '
          'matching release libraries, no game-data suffixes or checked private markers')


if __name__ == '__main__':
    check('windows', 'OpenChampions.exe', 'dll')
    check('linux', 'OpenChampions.x86_64', 'so')
