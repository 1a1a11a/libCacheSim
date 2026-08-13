"""Sphinx configuration for the libCacheSim documentation.

The docs are the Markdown files in this directory, rendered with MyST so the
same sources stay readable on GitHub. Build locally with:

    pip install -r doc/requirements.txt
    sphinx-build -b html doc doc/_build/html
"""

import os

# -- Project information -----------------------------------------------------

project = "libCacheSim"
author = "Juncheng Yang"
copyright = "2024, libCacheSim authors"  # noqa: A001

_version_file = os.path.join(os.path.dirname(__file__), os.pardir, "version.txt")
with open(_version_file, encoding="utf-8") as f:
    release = f.read().strip()
version = release

# -- General configuration ---------------------------------------------------

extensions = ["myst_parser", "sphinxcontrib.mermaid"]

source_suffix = {".md": "markdown", ".rst": "restructuredtext"}

exclude_patterns = [
    "_build",
    # Index for browsing the docs on GitHub; index.md is the Sphinx entry point.
    "README.md",
    # Not documentation.
    "plot",
    "assets",
]

# Generate anchors for headings so cross-file "#section" links resolve.
myst_heading_anchors = 3

myst_enable_extensions = [
    "colon_fence",
    "deflist",
]

# Render ```mermaid fences as diagrams rather than trying to syntax-highlight
# them, which GitHub does natively.
myst_fence_as_directive = ["mermaid"]

# -- HTML output -------------------------------------------------------------

html_theme = "sphinx_rtd_theme"
html_title = f"libCacheSim {release}"
html_static_path = []

# -- Link rewriting ----------------------------------------------------------
#
# The Markdown sources are written to be read on GitHub, so links into the
# repository are root-absolute ("/libCacheSim/cache/eviction/LRU.c") or relative
# to the repository root ("../README.md"). Those resolve on github.com but not
# in a rendered docs site, so point them back at the repository. This runs on
# `source-read`, before MyST resolves links, otherwise MyST reports each one as
# a missing cross-reference.

import re  # noqa: E402

_REPO_BLOB_URL = "https://github.com/1a1a11a/libCacheSim/blob/develop"

# Markdown inline links whose target leaves this directory.
_LINK_RE = re.compile(r"\]\((/[^)\s]*|\.\./[^)\s]*)\)")


def _rewrite_target(match):
    target = match.group(1)

    # Pages in this build: keep them as local cross-references so the sidebar,
    # search, and PDF output link them properly.
    if target.startswith("/doc/"):
        return "](%s)" % target[len("/doc/") :]

    if target.startswith("../"):
        target = "/" + target[len("../") :]

    return "](%s%s)" % (_REPO_BLOB_URL, target)


def _rewrite_repo_links(app, docname, source):
    source[0] = _LINK_RE.sub(_rewrite_target, source[0])


def setup(app):
    app.connect("source-read", _rewrite_repo_links)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
