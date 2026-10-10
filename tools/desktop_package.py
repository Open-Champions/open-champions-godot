#!/usr/bin/env python3
"""Export, execute and package the archive browser on its target desktop OS.

No game inputs are used. Downloads require --download-templates; every template
archive is SHA-512 checked against the official 4.5.2 release before installation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import plistlib
import posixpath
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent.parent
VERSION = '4.5.2.stable'
TEMPLATE_SHA512 = '003aa33743f58fb657717f090fc872ed3975e48d08a6012201a2259970d458a63d4d8a83090585307c23455ebfa4e6e0050e1057761c34863536095e3fcfab6c'
TEMPLATE_URL = 'https://github.com/godotengine/godot/releases/download/4.5.2-stable/Godot_v4.5.2-stable_export_templates.tpz'
PASS_MARKER = 'Open Champions browser smoke: PASS'
TARGETS = {
    'macos-universal': ('Darwin', 'macOS universal', 'OpenChampions.zip'),
    'windows-x64': ('Windows', 'Windows x64', 'OpenChampions.exe'),
    'windows-arm64': ('Windows', 'Windows ARM64', 'OpenChampions.exe'),
    'linux-x64': ('Linux', 'Linux x64', 'OpenChampions.x86_64'),
}
NOTICES = ('open-champions.txt', 'godot.txt', 'godot-cpp.txt', 'godot-third-party.txt')
SCOPE = 'Native archive browser with limited RTA texture and static rest-pose model/texture-color previews; no gameplay or Norrath compatibility claim.'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def file_hash(path, algorithm='sha256'):
    digest = hashlib.new(algorithm)
    with Path(path).open('rb') as source:
        for chunk in iter(lambda: source.read(4 * 1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def template_names(target):
    require(target in TARGETS, 'unknown desktop platform')
    if target == 'macos-universal':
        return ('version.txt', 'macos.zip')
    if target == 'linux-x64':
        return ('version.txt', 'linux_debug.x86_64', 'linux_release.x86_64')
    arch = 'arm64' if target == 'windows-arm64' else 'x86_64'
    return ('version.txt',) + tuple(
        f'windows_{kind}_{arch}{suffix}.exe'
        for kind in ('debug', 'release') for suffix in ('', '_console'))


def template_directory(system=None, environ=None, home=None):
    system = system or platform.system()
    environ = os.environ if environ is None else environ
    home = Path.home() if home is None else Path(home)
    if system == 'Darwin':
        base = home / 'Library/Application Support/Godot'
    elif system == 'Windows':
        require(bool(environ.get('APPDATA')), 'APPDATA is required to locate Godot templates')
        base = Path(environ['APPDATA']) / 'Godot'
    elif system == 'Linux':
        base = Path(environ.get('XDG_DATA_HOME') or home / '.local/share') / 'godot'
    else:
        raise ValueError('unsupported host operating system')
    return base / 'export_templates' / VERSION


def ensure_template_archive(path, allow_download=False):
    path = Path(path)
    if not path.exists():
        require(allow_download, 'template archive is absent; provide it locally or explicitly add --download-templates')
        path.parent.mkdir(parents=True, exist_ok=True)
        descriptor, temporary = tempfile.mkstemp(prefix='.godot-templates-', dir=path.parent)
        os.close(descriptor)
        try:
            print('Downloading official Godot 4.5.2 export templates...', flush=True)
            with urllib.request.urlopen(TEMPLATE_URL, timeout=60) as response, open(temporary, 'wb') as output:
                shutil.copyfileobj(response, output, 4 * 1024 * 1024)
            require(file_hash(temporary, 'sha512') == TEMPLATE_SHA512, 'downloaded template SHA-512 mismatch')
            os.replace(temporary, path)
        finally:
            Path(temporary).unlink(missing_ok=True)
    require(path.is_file() and file_hash(path, 'sha512') == TEMPLATE_SHA512,
            'template SHA-512 mismatch; no templates installed')
    return path


def install_templates(archive, target, destination=None):
    """Caller must supply the archive returned by ensure_template_archive."""
    destination = template_directory() if destination is None else Path(destination)
    names = template_names(target)
    with zipfile.ZipFile(archive) as bundle:
        require(bundle.read('templates/version.txt').decode('ascii').strip() == VERSION,
                'template version does not match pinned Godot')
        infos = [bundle.getinfo('templates/' + name) for name in names]
        require(all(not info.is_dir() and not stat.S_ISLNK(info.external_attr >> 16) for info in infos),
                'expected regular template files')
        destination.mkdir(parents=True, exist_ok=True)
        for name, info in zip(names, infos):
            output = destination / name
            descriptor, temporary = tempfile.mkstemp(prefix='.install-', dir=destination)
            os.close(descriptor)
            try:
                with bundle.open(info) as source, open(temporary, 'wb') as sink:
                    shutil.copyfileobj(source, sink, 4 * 1024 * 1024)
                if output.exists():
                    require(output.is_file() and not output.is_symlink() and file_hash(output) == file_hash(temporary),
                            f'existing template differs: {output}; preserve it before installing the pinned version')
                else:
                    os.chmod(temporary, 0o644 if name.endswith(('.txt', '.zip')) else 0o755)
                    os.replace(temporary, output)
            finally:
                Path(temporary).unlink(missing_ok=True)
    return destination


def run_logged(command, log, timeout, cwd=None):
    """No shell; nonzero exit and timeout are both hard failures."""
    try:
        result = subprocess.run([str(value) for value in command], cwd=cwd,
                                capture_output=True, text=True, errors='replace', timeout=timeout, check=False)
    except subprocess.TimeoutExpired as error:
        parts = [error.stdout or '', error.stderr or '']
        text = ''.join(value.decode('utf-8', errors='replace') if isinstance(value, bytes) else value for value in parts)
        Path(log).write_text(text + '\nTIMEOUT\n', encoding='utf-8')
        raise ValueError(f'command timed out after {timeout}s; see {log}') from error
    output = result.stdout + result.stderr
    Path(log).write_text(output, encoding='utf-8')
    if result.returncode != 0:
        print(output[-16000:], file=sys.stderr)
    require(result.returncode == 0, f'command exited {result.returncode}; see {log}')
    return output


def validate_macos_zip(archive):
    """Allow one app bundle and only symlinks resolving inside that bundle."""
    with zipfile.ZipFile(archive) as bundle:
        roots = set()
        for info in bundle.infolist():
            name = PurePosixPath(info.filename)
            require(not name.is_absolute() and '..' not in name.parts and '\\' not in info.filename,
                    'unsafe path in macOS export ZIP')
            require(bool(name.parts) and name.parts[0].endswith('.app'), 'unexpected file outside exported app')
            roots.add(name.parts[0])
            if stat.S_ISLNK(info.external_attr >> 16):
                require(info.file_size <= 4096, 'oversized app symlink')
                target = bundle.read(info).decode('utf-8')
                require(not target.startswith('/') and '\\' not in target, 'unsafe app symlink')
                resolved = posixpath.normpath(posixpath.join(str(name.parent), target))
                require(resolved == name.parts[0] or resolved.startswith(name.parts[0] + '/'),
                        'app symlink escapes bundle')
        require(len(roots) == 1, 'expected exactly one exported macOS app')
        return next(iter(roots))


def executable_architectures(path):
    """Read CPU identities without executing or loading an unverified file."""
    with Path(path).open('rb') as source:
        header = source.read(4096)
        if header.startswith(b'MZ'):
            require(len(header) >= 64, 'truncated PE header')
            offset = struct.unpack_from('<I', header, 60)[0]
            source.seek(offset)
            pe = source.read(6)
            require(len(pe) == 6 and pe[:4] == b'PE\0\0', 'invalid PE signature')
            return {{0x8664: 'x86_64', 0xaa64: 'arm64'}.get(struct.unpack_from('<H', pe, 4)[0], 'unknown')}
        if header.startswith(b'\x7fELF'):
            require(len(header) >= 20 and header[4:6] == b'\x02\x01', 'expected ELF64 little endian')
            return {{62: 'x86_64', 183: 'arm64'}.get(struct.unpack_from('<H', header, 18)[0], 'unknown')}
        if header[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
            require(len(header) >= 8, 'truncated universal Mach-O header')
            count = struct.unpack_from('>I', header, 4)[0]
            width = 32 if header[:4] == b'\xca\xfe\xba\xbf' else 20
            require(0 < count <= 16 and len(header) >= 8 + count * width, 'invalid universal Mach-O architecture table')
            return {{0x01000007: 'x86_64', 0x0100000c: 'arm64'}.get(
                struct.unpack_from('>I', header, 8 + i * width)[0], 'unknown') for i in range(count)}
        if header[:4] == b'\xcf\xfa\xed\xfe':
            require(len(header) >= 8, 'truncated Mach-O header')
            return {{0x01000007: 'x86_64', 0x0100000c: 'arm64'}.get(struct.unpack_from('<I', header, 4)[0], 'unknown')}
    raise ValueError(f'unsupported executable format: {path.name}')



def pe_imports(path):
    """Bounded PE32+ import-name inspection for the selected extension DLL."""
    data = Path(path).read_bytes()
    def read(offset, size):
        require(0 <= offset <= len(data) and size <= len(data) - offset, 'PE import metadata exceeds file')
        return data[offset:offset + size]
    def u16(offset):
        return struct.unpack('<H', read(offset, 2))[0]
    def u32(offset):
        return struct.unpack('<I', read(offset, 4))[0]
    require(read(0, 2) == b'MZ', 'expected PE extension')
    pe = u32(60)
    require(read(pe, 4) == b'PE\0\0', 'invalid PE extension signature')
    count, optional_size = u16(pe + 6), u16(pe + 20)
    optional = pe + 24
    require(0 < count <= 96 and optional_size >= 128 and u16(optional) == 0x20b,
            'expected bounded PE32+ optional header')
    read(optional, optional_size)
    require(u32(optional + 108) >= 2, 'PE import directory missing')
    import_rva, import_size = u32(optional + 120), u32(optional + 124)
    if import_rva == 0:
        require(import_size == 0, 'inconsistent PE import directory')
        return []
    sections = []
    for index in range(count):
        offset = optional + optional_size + index * 40
        read(offset, 40)
        sections.append((u32(offset + 12), u32(offset + 20), u32(offset + 16)))
    def rva_offset(rva, size):
        candidates = [raw + rva - base for base, raw, length in sections
                      if base <= rva and rva - base <= length and size <= length - (rva - base)]
        require(len(candidates) == 1, 'PE import RVA is missing or ambiguous')
        read(candidates[0], size)
        return candidates[0]
    require(20 <= import_size <= 1024 * 1024, 'invalid PE import directory size')
    table = rva_offset(import_rva, import_size)
    names = []
    for index in range(min(import_size // 20, 256)):
        descriptor = read(table + index * 20, 20)
        if not any(descriptor):
            return sorted(set(names))
        name_rva = struct.unpack_from('<I', descriptor, 12)[0]
        name = bytearray()
        for delta in range(256):
            value = data[rva_offset(name_rva + delta, 1)]
            if value == 0:
                break
            name.append(value)
        else:
            raise ValueError('PE import name exceeds limit')
        text = name.decode('ascii')
        require(text and '/' not in text and '\\' not in text, 'invalid PE import name')
        names.append(text)
    raise ValueError('PE import directory lacks bounded terminator')


def audit_windows_runtime(extension):
    imports = pe_imports(extension)
    # This project's release contract is /MT. Do not credit a DLL merely because
    # an identically named file is somewhere in the payload; that does not prove
    # loader visibility, architecture or the runtime's own dependencies.
    dynamic_runtime = [name for name in imports if name.lower().startswith(('vcruntime', 'msvcp'))]
    require(not dynamic_runtime, 'static Visual C++ runtime required; extension imports: ' + ', '.join(dynamic_runtime))
    return imports


def locate_binaries(payload, target, app_name=None):
    payload = Path(payload)
    if target == 'macos-universal':
        app = payload / app_name
        with (app / 'Contents/Info.plist').open('rb') as source:
            name = plistlib.load(source)['CFBundleExecutable']
        require(isinstance(name, str) and name not in ('.', '..') and '/' not in name and '\\' not in name,
                'invalid macOS bundle executable name')
        executable = app / 'Contents/MacOS' / name
        extension = app / 'Contents/Frameworks/champions_archive.macos.template_release.dylib'
        wanted = {'arm64', 'x86_64'}
    else:
        executable = payload / TARGETS[target][2]
        arch = 'arm64' if target == 'windows-arm64' else 'x86_64'
        filename = f'champions_archive.windows.template_release.{arch}.dll' if target.startswith('windows') else 'champions_archive.linux.template_release.x86_64.so'
        matches = list(payload.rglob(filename))
        require(len(matches) == 1, f'expected one exported native extension: {filename}')
        extension, wanted = matches[0], {arch}
    require(executable.is_file() and extension.is_file(), 'export is missing its executable or native extension')
    for binary in (executable, extension):
        require(executable_architectures(binary) == wanted, f'exported CPU architecture mismatch: {binary.name}')
    if os.name != 'nt':
        require(os.access(executable, os.X_OK), 'exported executable has no executable permission')
    return executable, extension, sorted(wanted)


def verify_export(executable, output_dir):
    output = run_logged([Path(executable).resolve(), '--headless', '--quit-after', '120', '--', '--smoke-test'],
                        Path(output_dir) / 'exported-smoke.log', 180, cwd=Path(executable).parent)
    require(PASS_MARKER in output.splitlines(), 'exported app did not report smoke PASS')
    require(re.search(r'(?m)^Archive browser checks: [1-9][0-9]*; failures: 0\r?$', output),
            'exported app did not report completed zero-failure checks')
    return output


def inventory(root):
    rows = []
    for path in sorted(Path(root).rglob('*')):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            require(path.resolve().is_relative_to(Path(root).resolve()), 'package symlink escapes payload')
            rows.append({'path': relative, 'symlink': os.readlink(path)})
        elif path.is_file():
            rows.append({'path': relative, 'size': path.stat().st_size, 'sha256': file_hash(path)})
    return rows


def package_payload(payload, archive, target, log):
    payload, archive = Path(payload), Path(archive)
    temporary = archive.with_suffix('.zip.partial')
    try:
        if target == 'macos-universal':
            run_logged(['/usr/bin/ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', payload, temporary], log, 180)
        else:
            with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
                for path in sorted(payload.rglob('*')):
                    require(not path.is_symlink(), 'unexpected symlink in non-macOS export')
                    bundle.write(path, path.relative_to(payload.parent))
        os.replace(temporary, archive)
    finally:
        temporary.unlink(missing_ok=True)
    checksum = file_hash(archive)
    archive.with_suffix('.zip.sha256').write_text(f'{checksum}  {archive.name}\n', encoding='ascii')
    return checksum


def build_package(args, root=ROOT):
    root = Path(root).resolve()
    require(platform.system() == TARGETS[args.platform][0], 'execute packaging on the selected target operating system')
    require(re.fullmatch('[0-9a-fA-F]{40}', args.source_commit), 'source commit must be a full 40-character Git revision')
    executable = Path(args.godot).resolve()
    require(executable.is_file(), 'Godot executable does not exist')
    output = Path(args.output_dir).resolve()
    require(not output.exists() or (output.is_dir() and not any(output.iterdir())),
            'output directory must be new or empty; old exports are never reused')
    output.mkdir(parents=True, exist_ok=True)
    version = run_logged([executable, '--version'], output / 'godot-version.log', 30).strip()
    require(version.startswith(VERSION + '.'), 'expected the pinned Godot 4.5.2 stable executable')
    archive = ensure_template_archive(args.templates, args.download_templates)
    installed = install_templates(archive, args.platform)
    print(f'Pinned templates ready for {args.platform}: {installed}', flush=True)
    payload = output / ('OpenChampions-' + args.platform)
    payload.mkdir()
    destination = output / 'export.zip' if args.platform == 'macos-universal' else payload / TARGETS[args.platform][2]
    run_logged([executable, '--headless', '--path', root / 'godot', '--export-release',
                TARGETS[args.platform][1], destination], output / 'export.log', 300, cwd=root)
    app_name = None
    if args.platform == 'macos-universal':
        app_name = validate_macos_zip(destination)
        run_logged(['/usr/bin/ditto', '-xk', destination, payload], output / 'extract.log', 120)
    binary, extension, arches = locate_binaries(payload, args.platform, app_name)
    windows_imports = audit_windows_runtime(extension) if args.platform.startswith('windows') else None
    verify_export(binary, output)
    shutil.copy2(root / 'LICENSE', payload / 'LICENSE')
    notices = payload / 'licenses'
    notices.mkdir(exist_ok=True)
    for name in NOTICES:
        shutil.copy2(root / 'godot/licenses' / name, notices / name)
    (payload / 'README.txt').write_text(
        'OPEN CHAMPIONS — ARCHIVE BROWSER\n\n' + SCOPE + '\n\n'
        'Open the application, choose a game label, then select your own GOB or LMP archive.\n'
        'Select a supported RTA .tex/.etex entry to preview its stored RGB or alpha channel.\n'
        'No game files are included. Inputs stay on your device; nothing is extracted or uploaded.\n'
        'Both game labels are available. Only Return to Arms archive comparisons are established.\n'
        'This development build is unsigned and is not notarized. No clean-machine or visual proof\n'
        'is implied by its automated headless smoke test. See licenses/ for bundled notices.\n'
        'Project: https://github.com/Open-Champions/open-champions-godot\n', encoding='utf-8')
    metadata = {'schema_version': 1, 'source_commit': args.source_commit.lower(), 'platform': args.platform,
                'host': {'system': platform.system(), 'machine': platform.machine()},
                'architectures': arches, 'scope': SCOPE, 'godot_version': version,
                'godot_executable_sha256': file_hash(executable), 'template_sha512': TEMPLATE_SHA512,
                'exported_executable': binary.relative_to(payload).as_posix(),
                'extension': extension.relative_to(payload).as_posix(), 'windows_extension_imports': windows_imports,
                'smoke': PASS_MARKER,
                'smoke_log_sha256': file_hash(output / 'exported-smoke.log'),
                'files': inventory(payload)}
    (payload / 'build-info.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    package = output / (payload.name + '.zip')
    checksum = package_payload(payload, package, args.platform, output / 'package.log')
    print(f'Exported desktop smoke: PASS\nPackage: {package}\nSHA256: {checksum}')
    return package


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', required=True, choices=TARGETS)
    parser.add_argument('--godot', required=True, type=Path)
    parser.add_argument('--templates', required=True, type=Path)
    parser.add_argument('--download-templates', action='store_true', help='explicitly download pinned official templates if absent')
    parser.add_argument('--output-dir', required=True, type=Path)
    parser.add_argument('--source-commit', required=True)
    args = parser.parse_args(argv)
    try:
        build_package(args)
        return 0
    except (OSError, ValueError, KeyError, zipfile.BadZipFile, subprocess.SubprocessError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
