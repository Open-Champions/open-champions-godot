"""Reject presentation, external and floating-point simulation dependencies."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from check_simulation_boundary import check, simulation_paths


class SimulationBoundary(unittest.TestCase):
    def test_animation_decoder_rejects_viewer_and_host_arithmetic(self):
        for name in ("native/include/champions/animation_decoder.hpp",
                     "native/src/animation_decoder.cpp"):
            for text, error in (("double time = 0;\n", "floating-point"),
                                ('#include "champions/animation.hpp"\n', "dependency"),
                                ('#include "champions/skinning.hpp"\n', "dependency"),
                                ('#include "godot_cpp/classes/node.hpp"\n', "dependency")):
                with self.subTest(path=name, text=text):
                    path = self.root / name
                    path.write_text(text)
                    with self.assertRaisesRegex(ValueError, error):
                        check(self.root)
                    path.write_text("")

    def test_animation_decoder_paths_are_part_of_simulation_census(self):
        paths = {p.relative_to(self.root).as_posix() for p in simulation_paths(self.root)}
        self.assertEqual(len(paths), 56)
        self.assertTrue({"native/include/champions/animation_decoder.hpp",
                         "native/src/animation_decoder.cpp"} <= paths)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        repository = Path(__file__).resolve().parent.parent
        for actual in simulation_paths(repository):
            path = self.root / actual.relative_to(repository)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("")
        self.source = self.root / "native/src/combat_common.cpp"

    def test_uncompiled_backup_preserved(self):
        backup = self.root / "native/src/core_arithmetic 2.cpp"
        backup.write_text("double backup = 0;\n")
        with contextlib.redirect_stdout(io.StringIO()):
            check(self.root)
        self.assertEqual(backup.read_text(), "double backup = 0;\n")

    def test_creature_movement_cannot_import_presentation(self):
        for name in ("native/include/champions/creature_movement.hpp",
                     "native/src/creature_movement.cpp"):
            for text, error in (("float velocity = 0;\n", "floating-point"),
                                ('#include "godot_cpp/classes/node.hpp"\n', "dependency"),
                                ('#include "champions/animation.hpp"\n', "dependency")):
                with self.subTest(path=name, text=text):
                    path = self.root / name
                    path.write_text(text)
                    with self.assertRaisesRegex(ValueError, error):
                        check(self.root)
                    path.write_text("")

    def test_session_creature_rules_cannot_import_presentation(self):
        for name in ("native/include/champions/session.hpp", "native/src/session.cpp"):
            for text, error in (("double velocity = 0;\n", "floating-point"),
                                ('#include "godot_cpp/classes/node.hpp"\n', "dependency"),
                                ('#include "champions/animation.hpp"\n', "dependency")):
                with self.subTest(path=name, text=text):
                    path = self.root / name
                    path.write_text(text)
                    with self.assertRaisesRegex(ValueError, error):
                        check(self.root)
                    path.write_text("")

    def test_session_animation_integer_clock_dependency_is_allowed(self):
        header = self.root / "native/include/champions/session.hpp"
        header.write_text('#include "champions/animation_clock.hpp"\n'
                          '#include <cstdint>\nstd::uint32_t flags = 0;\n')
        source = self.root / "native/src/session.cpp"
        source.write_text('#include "champions/session.hpp"\n'
                          '#include "champions/animation_clock.hpp"\n')
        with contextlib.redirect_stdout(io.StringIO()):
            check(self.root)

    def test_session_animation_cannot_import_pose_skinning_or_asset_models(self):
        for name in ("native/include/champions/session.hpp", "native/src/session.cpp"):
            for dependency in ("champions/animation.hpp", "champions/skinning.hpp",
                               "champions/model.hpp", "godot_cpp/classes/skeleton3d.hpp"):
                with self.subTest(path=name, dependency=dependency):
                    path = self.root / name
                    path.write_text(f'#include "{dependency}"\n')
                    with self.assertRaisesRegex(ValueError, "dependency"):
                        check(self.root)
                    path.write_text("")

    def test_session_animation_paths_remain_in_complete_simulation_boundary(self):
        paths = {p.relative_to(self.root).as_posix() for p in simulation_paths(self.root)}
        self.assertEqual(len(paths), 56)
        self.assertTrue({"native/include/champions/session.hpp", "native/src/session.cpp",
                         "native/include/champions/animation_clock.hpp",
                         "native/src/animation_clock.cpp"} <= paths)

    def test_animation_clock_cannot_import_presentation_or_host_arithmetic(self):
        for name in ("native/include/champions/animation_clock.hpp",
                     "native/src/animation_clock.cpp"):
            for text, error in (("double clock = 0;\n", "floating-point"),
                                ('#include "champions/animation.hpp"\n', "dependency"),
                                ('#include "godot_cpp/classes/node.hpp"\n', "dependency")):
                with self.subTest(path=name, text=text):
                    path = self.root / name
                    path.write_text(text)
                    with self.assertRaisesRegex(ValueError, error):
                        check(self.root)
                    path.write_text("")

    def test_control_host_float_is_rejected_in_header_and_body(self):
        for name in ("native/include/champions/session_control.hpp",
                     "native/src/session_control.cpp"):
            with self.subTest(path=name):
                path = self.root / name
                path.write_text("double elapsed = 0;\n")
                with self.assertRaisesRegex(ValueError, "floating-point"):
                    check(self.root)
                path.write_text("")

    def test_control_cannot_import_godot_or_asset_geometry(self):
        for name in ("native/include/champions/session_control.hpp",
                     "native/src/session_control.cpp"):
            for include in ("godot_cpp/classes/node.hpp", "champions/model.hpp"):
                with self.subTest(path=name, include=include):
                    path = self.root / name
                    path.write_text(f'#include "{include}"\n')
                    with self.assertRaisesRegex(ValueError, "dependency"):
                        check(self.root)
                    path.write_text("")

    def test_control_integer_session_boundary_is_allowed(self):
        header = self.root / "native/include/champions/session_control.hpp"
        header.write_text('#include "champions/session.hpp"\n#include <cstdint>\n')
        source = self.root / "native/src/session_control.cpp"
        source.write_text('#include "champions/session_control.hpp"\nstd::uint32_t commands = 0;\n')
        with contextlib.redirect_stdout(io.StringIO()):
            check(self.root)

    def test_terrain_file_host_double(self):
        self.source = self.root / "native/src/terrain_file.cpp"
        self.source.write_text("double value = 0;\n")
        with self.assertRaisesRegex(ValueError, "floating-point"):
            check(self.root)

    def test_terrain_file_viewer_dependency(self):
        self.source = self.root / "native/include/champions/terrain_file.hpp"
        self.source.write_text('#include "champions/model.hpp"\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_terrain_host_double(self):
        self.source = self.root / "native/src/world_terrain.cpp"
        self.source.write_text("double value = 0;\n")
        with self.assertRaisesRegex(ValueError, "floating-point"):
            check(self.root)

    def test_terrain_viewer_dependency(self):
        self.source = self.root / "native/src/world_movement.cpp"
        self.source.write_text('#include "champions/model.hpp"\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_standard_integer_dependency(self):
        self.source.write_text('#include <cstdint>\nstd::uint32_t value = 0;\n')
        with contextlib.redirect_stdout(io.StringIO()):
            check(self.root)

    def test_godot_dependency(self):
        self.source.write_text('#include <godot_cpp/classes/node.hpp>\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_external_dependency(self):
        self.source.write_text('#include <boost/multiprecision/cpp_dec_float.hpp>\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_presentation_dependency(self):
        self.source.write_text('#include "champions/skinning.hpp"\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_math_dependency(self):
        self.source.write_text('#include <cmath>\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_session_host_double(self):
        self.source = self.root / "native/src/session.cpp"
        self.source.write_text("double value = 0;\n")
        with self.assertRaisesRegex(ValueError, "floating-point"):
            check(self.root)

    def test_session_viewer_dependency(self):
        self.source = self.root / "native/src/session.cpp"
        self.source.write_text('#include "champions/model.hpp"\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_object_host_double(self):
        self.source = self.root / "native/src/objects_queries.cpp"
        self.source.write_text("double value = 0;\n")
        with self.assertRaisesRegex(ValueError, "floating-point"):
            check(self.root)

    def test_object_viewer_dependency(self):
        self.source = self.root / "native/src/objects_scheduler.cpp"
        self.source.write_text('#include "champions/skinning.hpp"\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_creature_host_double(self):
        self.source = self.root / "native/src/creature_hp.cpp"
        self.source.write_text("double value = 0;\n")
        with self.assertRaisesRegex(ValueError, "floating-point"):
            check(self.root)

    def test_creature_viewer_dependency(self):
        self.source = self.root / "native/include/champions/creature_hp.hpp"
        self.source.write_text('#include "champions/model.hpp"\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_combat_creature_host_double(self):
        self.source = self.root / "native/src/combat_creature.cpp"
        self.source.write_text("double value = 0;\n")
        with self.assertRaisesRegex(ValueError, "floating-point"):
            check(self.root)

    def test_combat_creature_viewer_dependency(self):
        self.source = self.root / "native/include/champions/combat_creature.hpp"
        self.source.write_text('#include "champions/model.hpp"\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_raw_handle_host_double(self):
        self.source = self.root / "native/src/raw_handles.cpp"
        self.source.write_text("double value = 0;\n")
        with self.assertRaisesRegex(ValueError, "floating-point"):
            check(self.root)

    def test_raw_handle_viewer_dependency(self):
        self.source = self.root / "native/include/champions/raw_handles.hpp"
        self.source.write_text('#include "champions/model.hpp"\n')
        with self.assertRaisesRegex(ValueError, "dependency"):
            check(self.root)

    def test_host_double(self):
        self.source.write_text('double value = 0;\n')
        with self.assertRaisesRegex(ValueError, "floating-point"):
            check(self.root)


if __name__ == "__main__":
    unittest.main()
