"""Reject presentation, external and floating-point simulation dependencies."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from check_simulation_boundary import check, simulation_paths


class SimulationBoundary(unittest.TestCase):
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

    def test_host_double(self):
        self.source.write_text('double value = 0;\n')
        with self.assertRaisesRegex(ValueError, "floating-point"):
            check(self.root)


if __name__ == "__main__":
    unittest.main()
