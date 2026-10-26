"""Temporary Godot projects and per-case user data; never mutate source settings."""

from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import uuid


def data_home():
    # Godot 4.5 OS implementations: macOS Application Support, Windows APPDATA,
    # Unix XDG_DATA_HOME (or ~/.local/share). Custom user dirs are relative here.
    if sys.platform == "darwin":
        return Path.home() / "Library/Application Support"
    if sys.platform == "win32":
        return Path(os.environ["APPDATA"])
    return Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share")


@contextmanager
def isolated_project(source=None):
    """Copy resources/cache once per case and remove user data even on failure.

    A nested driver receives this project's path and configures its generated
    project there. Every context has a fresh name, independent of --out.
    """
    name = "open-champions-matrix-" + uuid.uuid4().hex
    user = data_home() / name
    with tempfile.TemporaryDirectory(prefix="godot-matrix-") as temporary:
        project = Path(temporary) / "project"
        try:
            if source is not None:
                shutil.copytree(source, project)
            else:
                project.mkdir()
            yield project, name, user
        finally:
            shutil.rmtree(user, ignore_errors=False) if user.exists() else None


def configure_user_dir(project, name):
    # override.cfg is Godot's supported project-settings override. The staged
    # project owns this file; the source project and its caches stay untouched.
    override = project / "override.cfg"
    previous = override.read_text(encoding="utf-8") if override.exists() else ""
    override.write_text(previous + '\n[application]\nconfig/use_custom_user_dir=true\n'
                        'config/custom_user_dir_name=' + json.dumps(name) + '\n', encoding="utf-8")
