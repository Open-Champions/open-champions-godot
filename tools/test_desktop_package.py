"""Synthetic package tests; no engine downloads, retail inputs or real exports."""
import argparse
import hashlib
import importlib.util
import os
import plistlib
from pathlib import Path
import stat
import struct
import subprocess
import tempfile
import unittest
from unittest import mock
import zipfile

SPEC = importlib.util.spec_from_file_location('desktop_package', Path(__file__).with_name('desktop_package.py'))
package = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(package)


def pe(machine):
    data = bytearray(70)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 64)
    data[64:68] = b'PE\0\0'
    struct.pack_into('<H', data, 68, machine)
    return bytes(data)


def elf(machine=62):
    data = bytearray(64)
    data[:6] = b'\x7fELF\x02\x01'
    struct.pack_into('<H', data, 18, machine)
    return bytes(data)


def pe_with_import(name):
    data = bytearray(1024)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 64)
    data[64:68] = b'PE\0\0'
    struct.pack_into('<HH', data, 68, 0x8664, 1)
    struct.pack_into('<H', data, 84, 240)
    optional = 88
    struct.pack_into('<H', data, optional, 0x20b)
    struct.pack_into('<I', data, optional + 108, 16)
    struct.pack_into('<II', data, optional + 120, 0x1000, 40)
    section = optional + 240
    struct.pack_into('<III', data, section + 12, 0x1000, 512, 512)
    struct.pack_into('<I', data, 512 + 12, 0x1040)
    encoded = name.encode('ascii') + b'\0'
    data[576:576 + len(encoded)] = encoded
    return bytes(data)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def template(self, version=package.VERSION):
        path = self.root / 'synthetic.tpz'
        with zipfile.ZipFile(path, 'w') as bundle:
            bundle.writestr('templates/version.txt', version)
            for target in package.TARGETS:
                for name in package.template_names(target)[1:]:
                    bundle.writestr('templates/' + name, name.encode())
            bundle.writestr('templates/android_release.apk', b'not selected')
        return path

    def test_host_template_locations(self):
        self.assertEqual(package.template_directory('Darwin', {}, self.root),
                         self.root / 'Library/Application Support/Godot/export_templates/4.5.2.stable')
        self.assertEqual(package.template_directory('Linux', {'XDG_DATA_HOME': str(self.root)}, self.root),
                         self.root / 'godot/export_templates/4.5.2.stable')
        self.assertEqual(package.template_directory('Windows', {'APPDATA': str(self.root)}, self.root),
                         self.root / 'Godot/export_templates/4.5.2.stable')
        with self.assertRaisesRegex(ValueError, 'APPDATA'):
            package.template_directory('Windows', {}, self.root)

    def test_install_selects_only_platform_files(self):
        for target in package.TARGETS:
            with self.subTest(target=target):
                output = self.root / target
                package.install_templates(self.template(), target, output)
                self.assertEqual({p.name for p in output.iterdir()}, set(package.template_names(target)))
                # A matching existing installation is reusable without clobbering it.
                package.install_templates(self.template(), target, output)

    def test_wrong_template_version_rejected_before_install(self):
        output = self.root / 'templates'
        with self.assertRaisesRegex(ValueError, 'version'):
            package.install_templates(self.template('4.4.stable'), 'linux-x64', output)
        self.assertFalse(output.exists())

    def test_existing_different_template_is_preserved(self):
        output = self.root / 'templates'
        output.mkdir()
        old = output / 'macos.zip'
        old.write_bytes(b'custom template')
        with self.assertRaisesRegex(ValueError, 'existing template differs'):
            package.install_templates(self.template(), 'macos-universal', output)
        self.assertEqual(old.read_bytes(), b'custom template')

    def test_missing_archive_never_downloads_implicitly(self):
        with mock.patch.object(package.urllib.request, 'urlopen') as network:
            with self.assertRaisesRegex(ValueError, 'explicitly'):
                package.ensure_template_archive(self.root / 'missing.tpz')
            network.assert_not_called()

    def test_checksum_rejection_and_verified_local_path(self):
        path = self.template()
        with self.assertRaisesRegex(ValueError, 'SHA-512 mismatch'):
            package.ensure_template_archive(path)
        expected = hashlib.sha512(path.read_bytes()).hexdigest()
        with mock.patch.object(package, 'TEMPLATE_SHA512', expected):
            self.assertEqual(package.ensure_template_archive(path), path)

    def test_failed_download_is_not_installed_or_left_partial(self):
        path = self.root / 'download.tpz'
        response = mock.MagicMock()
        response.__enter__.return_value.read.side_effect = [b'invalid', b'']
        with mock.patch.object(package.urllib.request, 'urlopen', return_value=response):
            with self.assertRaisesRegex(ValueError, 'SHA-512 mismatch'):
                package.ensure_template_archive(path, True)
        self.assertFalse(path.exists())
        self.assertEqual(list(self.root.iterdir()), [])

    def test_pe_and_elf_architecture_identity(self):
        self.assertEqual(package.executable_architectures(self.write('x64.exe', pe(0x8664))), {'x86_64'})
        self.assertEqual(package.executable_architectures(self.write('arm.dll', pe(0xaa64))), {'arm64'})
        self.assertEqual(package.executable_architectures(self.write('linux', elf())), {'x86_64'})
        with self.assertRaisesRegex(ValueError, 'invalid PE'):
            package.executable_architectures(self.write('bad.exe', b'MZ' + bytes(100)))

    def test_universal_mach_o_identity(self):
        data = bytearray(48)
        data[:4] = b'\xca\xfe\xba\xbe'
        struct.pack_into('>I', data, 4, 2)
        struct.pack_into('>I', data, 8, 0x01000007)
        struct.pack_into('>I', data, 28, 0x0100000c)
        self.assertEqual(package.executable_architectures(self.write('universal', data)), {'x86_64', 'arm64'})

    def test_export_requires_selected_extension_and_matching_cpu(self):
        payload = self.root / 'payload'
        self.write('payload/OpenChampions.exe', pe(0xaa64)).chmod(0o755)
        extension = self.write('payload/champions_archive.windows.template_release.arm64.dll', pe(0x8664))
        with self.assertRaisesRegex(ValueError, 'CPU architecture'):
            package.locate_binaries(payload, 'windows-arm64')
        extension.write_bytes(pe(0xaa64))
        executable, found, arches = package.locate_binaries(payload, 'windows-arm64')
        self.assertEqual(found, extension)
        self.assertEqual(arches, ['arm64'])
        self.assertEqual(executable.name, 'OpenChampions.exe')


    def test_debug_only_binding_is_rejected_for_every_release_export(self):
        for target in ['windows-x64', 'windows-arm64', 'linux-x64']:
            with self.subTest(target=target):
                payload = self.root / target
                windows = target.startswith('windows')
                arch = 'arm64' if target == 'windows-arm64' else 'x86_64'
                data = pe(0xaa64 if arch == 'arm64' else 0x8664) if windows else elf()
                executable = 'OpenChampions.exe' if windows else 'OpenChampions.x86_64'
                self.write(target + '/' + executable, data).chmod(0o755)
                debug_name = f'champions_archive.windows.template_debug.{arch}.dll' if windows else 'champions_archive.linux.template_debug.x86_64.so'
                self.write(target + '/' + debug_name, data)
                with self.assertRaisesRegex(ValueError, 'template_release'):
                    package.locate_binaries(payload, target)
        app_name = 'Browser.app'
        self.write('mac/' + app_name + '/Contents/Info.plist', plistlib.dumps({'CFBundleExecutable': 'Browser'}))
        self.write('mac/' + app_name + '/Contents/MacOS/Browser', b'not executed').chmod(0o755)
        self.write('mac/' + app_name + '/Contents/Frameworks/champions_archive.macos.template_debug.dylib', b'not selected')
        with self.assertRaisesRegex(ValueError, 'missing'):
            package.locate_binaries(self.root / 'mac', 'macos-universal', app_name)

    def test_linux_extension_name_matches_descriptor(self):
        payload = self.root / 'linux'
        self.write('linux/OpenChampions.x86_64', elf()).chmod(0o755)
        self.write('linux/champions_archive.linux.template_release.x86_64.so', elf())
        self.assertEqual(package.locate_binaries(payload, 'linux-x64')[2], ['x86_64'])

    def test_macos_zip_rejects_outside_paths_and_escaping_symlinks(self):
        for name, link in [('../escape', None), ('Extra.txt', None), ('App.app/link', '../../escape')]:
            with self.subTest(name=name):
                archive = self.root / 'mac.zip'
                with zipfile.ZipFile(archive, 'w') as bundle:
                    info = zipfile.ZipInfo(name)
                    if link:
                        info.create_system = 3
                        info.external_attr = (stat.S_IFLNK | 0o777) << 16
                    bundle.writestr(info, link or 'text')
                with self.assertRaises(ValueError):
                    package.validate_macos_zip(archive)

    def test_macos_zip_accepts_in_bundle_framework_symlink(self):
        archive = self.root / 'mac.zip'
        with zipfile.ZipFile(archive, 'w') as bundle:
            bundle.writestr('App.app/Contents/Frameworks/Versions/A/library', b'authored')
            link = zipfile.ZipInfo('App.app/Contents/Frameworks/Versions/Current')
            link.create_system = 3
            link.external_attr = (stat.S_IFLNK | 0o777) << 16
            bundle.writestr(link, 'A')
        self.assertEqual(package.validate_macos_zip(archive), 'App.app')

    def test_smoke_requires_exit_success_and_exact_pass(self):
        executable = self.root / 'OpenChampions.exe'
        good = 'Archive browser checks: 23; failures: 0\n' + package.PASS_MARKER + '\n'
        for code, output, success in [(0, good, True), (1, good, False), (0, '', False),
                                       (0, 'prefix ' + package.PASS_MARKER, False),
                                       (0, package.PASS_MARKER + '\n', False)]:
            with self.subTest(code=code, output=output):
                result = subprocess.CompletedProcess([], code, output, '')
                with mock.patch.object(package.subprocess, 'run', return_value=result) as run:
                    if success:
                        package.verify_export(executable, self.root)
                        args = run.call_args.args[0]
                        self.assertEqual(args[0], str(executable.resolve()))
                        self.assertNotIn('--path', args)
                        self.assertEqual(args[1:], ['--headless', '--quit-after', '120', '--', '--browser-smoke-test', '--smoke-test'])
                        self.assertEqual(run.call_args.kwargs['cwd'], executable.parent)
                    else:
                        with self.assertRaises(ValueError):
                            package.verify_export(executable, self.root)

    def test_sandbox_smoke_requires_completed_independent_checks(self):
        good = 'Open Champions session sandbox: 40 checks, 0 failures\nSESSION_SANDBOX_SMOKE_PASS\n'
        for code, output, success in ((0, good, True), (1, good, False),
                                      (0, 'SESSION_SANDBOX_SMOKE_PASS\n', False),
                                      (0, good.replace('0 failures', '1 failures'), False),
                                      (0, 'Archive browser checks: 23; failures: 0\n' + package.PASS_MARKER + '\n', False)):
            with self.subTest(code=code, output=output):
                result = subprocess.CompletedProcess([], code, output, '')
                with mock.patch.object(package.subprocess, 'run', return_value=result) as run:
                    if success:
                        package.verify_sandbox_export(self.root / 'program', self.root)
                        self.assertEqual(run.call_args.args[0][-1], '--sandbox-smoke-test')
                        self.assertNotIn('--path', run.call_args.args[0])
                    else:
                        with self.assertRaises(ValueError):
                            package.verify_sandbox_export(self.root / 'program', self.root)

    def test_timeout_records_log_and_fails(self):
        failure = subprocess.TimeoutExpired(['fake'], 180, output=b'partial output')
        with mock.patch.object(package.subprocess, 'run', side_effect=failure):
            with self.assertRaisesRegex(ValueError, 'timed out'):
                package.verify_export(self.root / 'program', self.root)
        self.assertIn('TIMEOUT', (self.root / 'exported-smoke.log').read_text())


    def test_windows_runtime_import_audit(self):
        dll = self.write('payload/champions_archive.windows.template_release.x86_64.dll', pe_with_import('VCRUNTIME140.dll'))
        self.assertEqual(package.pe_imports(dll), ['VCRUNTIME140.dll'])
        with self.assertRaisesRegex(ValueError, 'static Visual C'):
            package.audit_windows_runtime(dll)
        self.write('payload/vcruntime140.dll', b'authored fixture')
        with self.assertRaisesRegex(ValueError, 'static Visual C'):
            package.audit_windows_runtime(dll)
        dll.write_bytes(pe_with_import('KERNEL32.dll'))
        self.assertEqual(package.audit_windows_runtime(dll), ['KERNEL32.dll'])

    def test_invalid_import_rva_fails_closed(self):
        data = bytearray(pe_with_import('MSVCP140.dll'))
        struct.pack_into('<I', data, 512 + 12, 0xffffffff)
        with self.assertRaisesRegex(ValueError, 'RVA'):
            package.pe_imports(self.write('bad.dll', data))

    def test_whole_pipeline_uses_only_fresh_export_and_adds_evidence(self):
        godot = self.write('godot', b'fake engine')
        self.write('LICENSE', b'authored license')
        for name in package.NOTICES:
            self.write('godot-project/licenses/' + name, b'notice')
        # Engine path and project directory must be distinct in this synthetic tree.
        godot.rename(self.root / 'engine')
        godot = self.root / 'engine'
        (self.root / 'godot-project').rename(self.root / 'godot')
        templates = self.template()
        args = argparse.Namespace(platform='linux-x64', godot=godot, templates=templates,
                                  download_templates=False, output_dir=self.root / 'out', source_commit='a' * 40)
        original_install = package.install_templates
        def install(archive, target):
            return original_install(archive, target, self.root / 'installed')
        def command(argv, **kwargs):
            if '--version' in argv:
                return subprocess.CompletedProcess(argv, 0, package.VERSION + '.official.test', '')
            if '--export-release' in argv:
                output = Path(argv[-1])
                output.write_bytes(elf())
                output.chmod(0o755)
                (output.parent / 'champions_archive.linux.template_release.x86_64.so').write_bytes(elf())
                (output.parent / 'export-sidecar.txt').write_text('authored sidecar')
                return subprocess.CompletedProcess(argv, 0, 'exported', '')
            if argv[-1] == '--sandbox-smoke-test':
                return subprocess.CompletedProcess(argv, 0, 'Open Champions session sandbox: 40 checks, 0 failures\nSESSION_SANDBOX_SMOKE_PASS\n', '')
            self.assertEqual(argv[-1], '--smoke-test')
            self.assertNotIn('--path', argv)
            return subprocess.CompletedProcess(argv, 0, 'Archive browser checks: 23; failures: 0\n' + package.PASS_MARKER + '\n', '')
        with mock.patch.object(package.platform, 'system', return_value='Linux'), \
             mock.patch.object(package, 'TEMPLATE_SHA512', hashlib.sha512(templates.read_bytes()).hexdigest()), \
             mock.patch.object(package, 'install_templates', side_effect=install), \
             mock.patch.object(package.subprocess, 'run', side_effect=command):
            output = package.build_package(args, self.root)
        with zipfile.ZipFile(output) as bundle:
            names = bundle.namelist()
            prefix = 'OpenChampions-linux-x64/'
            for name in ['LICENSE', 'build-info.json', 'README.txt', 'export-sidecar.txt', 'licenses/godot-cpp.txt']:
                self.assertIn(prefix + name, names)
            self.assertNotIn(prefix + 'godot', names)

    def test_zip_includes_sidecars_not_unrelated_outputs(self):
        payload = self.root / 'OpenChampions-linux-x64'
        executable = self.write(payload.name + '/OpenChampions.x86_64', b'authored')
        executable.chmod(0o755)
        self.write(payload.name + '/champions_archive.linux.template_release.x86_64.so', b'extension')
        self.write(payload.name + '/nested/notice.txt', b'notice')
        self.write('other-export/extra.bin', b'not selected')
        archive = self.root / 'package.zip'
        checksum = package.package_payload(payload, archive, 'linux-x64', self.root / 'package.log')
        self.assertEqual(checksum, hashlib.sha256(archive.read_bytes()).hexdigest())
        self.assertIn(checksum, archive.with_suffix('.zip.sha256').read_text())
        with zipfile.ZipFile(archive) as bundle:
            names = bundle.namelist()
            self.assertTrue(all(name.startswith(payload.name + '/') for name in names))
            self.assertIn(payload.name + '/champions_archive.linux.template_release.x86_64.so', names)
            if os.name != 'nt':
                self.assertTrue((bundle.getinfo(payload.name + '/OpenChampions.x86_64').external_attr >> 16) & stat.S_IXUSR)

    def test_fresh_output_and_host_guards_precede_export(self):
        godot = self.write('godot', b'fake')
        output = self.root / 'used'
        output.mkdir()
        (output / 'old.dll').write_text('stale')
        args = argparse.Namespace(platform='linux-x64', godot=godot, templates=self.root / 'none',
                                  download_templates=False, output_dir=output, source_commit='a' * 40)
        with mock.patch.object(package.platform, 'system', return_value='Linux'):
            with mock.patch.object(package.subprocess, 'run') as run:
                with self.assertRaisesRegex(ValueError, 'new or empty'):
                    package.build_package(args, self.root)
                run.assert_not_called()
        with mock.patch.object(package.platform, 'system', return_value='Windows'):
            with self.assertRaisesRegex(ValueError, 'operating system'):
                package.build_package(args, self.root)


if __name__ == '__main__':
    unittest.main()
