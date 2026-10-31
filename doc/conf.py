# Copyright (c) 2026 Romain Pelletant
# SPDX-License-Identifier: Apache-2.0

"""Sphinx configuration adapted from the Zephyr example application."""

from pathlib import Path
import re
import subprocess
import sys

from sphinx.errors import ExtensionError

DOC_DIR = Path(__file__).resolve().parent
ROOT = DOC_DIR.parent
sys.path.insert(0, str(DOC_DIR / "_extensions"))

project = "zndef"
copyright = "2026, Romain Pelletant"
author = "Romain Pelletant"
release = re.search(
    r"project\(zndef VERSION ([0-9.]+)",
    (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"),
).group(1)
version = release

extensions = ["breathe", "ndef_api", "sphinx.ext.intersphinx"]
exclude_patterns = ["README.rst", "_build_sphinx", "_build", "build*", "Thumbs.db", ".DS_Store"]
root_doc = "index"
primary_domain = "c"
highlight_language = "c"
language = "en"
nitpicky = True

# Keep the theme from the Zephyr example application template.
html_theme = "sphinx_rtd_theme"
html_title = f"zndef {release}"
intersphinx_mapping = {"zephyr": ("https://docs.zephyrproject.org/latest/", None)}

breathe_default_project = "zndef"
breathe_projects = {"zndef": str(ROOT / "build-doc" / "doxygen" / "xml")}
breathe_domain_by_extension = {"h": "c"}
# C standard types are provided by the toolchain, outside this reference.
nitpick_ignore = [("c:identifier", name) for name in (
    "bool", "size_t", "uint8_t", "uint16_t", "uint32_t",
)]
# Doxygen's synthetic type names for anonymous item-table unions/structures.
nitpick_ignore_regex = [("c:identifier", r"ndef_item\.@.*")]


def generate_doxygen(app):
    """Generate current XML before Breathe reads the API."""
    output = Path(app.outdir).resolve().parent / "doxygen"
    config = (DOC_DIR / "Doxyfile").read_text(encoding="utf-8")
    config += (
        f'\nOUTPUT_DIRECTORY = "{output}"\n'
        f'PROJECT_NUMBER = "{release}"\n'
        "GENERATE_HTML = NO\n"
    )
    try:
        subprocess.run(["doxygen", "-"], input=config, text=True, cwd=ROOT, check=True)
    except (OSError, subprocess.CalledProcessError) as exc:
        raise ExtensionError(f"Doxygen generation failed: {exc}") from exc
    app.config.breathe_projects = {"zndef": str(output / "xml")}


def setup(app):
    app.connect("builder-inited", generate_doxygen)
