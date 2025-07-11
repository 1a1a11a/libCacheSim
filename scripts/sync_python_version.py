#!/usr/bin/env python3
"""
Script to synchronize version between libCacheSim main project and Python bindings.

This script reads the version from version.txt and updates the pyproject.toml
in libCacheSim-python to match.
"""

import json
import os
import sys
import re
from pathlib import Path


def get_project_root():
    """Get the project root directory."""
    script_dir = Path(__file__).parent
    return script_dir.parent


def read_main_version():
    """Read version from version.txt."""
    project_root = get_project_root()
    version_file = project_root / "version.txt"

    if not version_file.exists():
        print(f"Error: {version_file} not found", file=sys.stderr)
        sys.exit(1)

    with open(version_file, 'r') as f:
        version = f.read().strip()

    if not version:
        print("Error: version.txt is empty", file=sys.stderr)
        sys.exit(1)

    return version

def update_pyproject_toml(version):
    """Update pyproject.toml with the new version."""
    project_root = get_project_root()
    pyproject_toml_path = project_root / "libCacheSim-python" / "pyproject.toml"

    if not pyproject_toml_path.exists():
        print(f"Error: {pyproject_toml_path} not found", file=sys.stderr)
        return False

    # Read current pyproject.toml
    with open(pyproject_toml_path, 'r') as f:
        pyproject_data = f.read()

    # Update the version line in pyproject.toml, make it can match any version in version.txt, like "0.3.1" or "dev"
    match = re.search(r"version = \"(dev|[0-9]+\.[0-9]+\.[0-9]+)\"", pyproject_data)
    if not match:
        print("Error: Could not find a valid version line in pyproject.toml", file=sys.stderr)
        return False
    current_version = match.group(1)
    if current_version == version:
        print(f"Python binding version already up to date: {version}")
        return False
    # replace the version line with the new version
    pyproject_data = re.sub(r"version = \"(dev|[0-9]+\.[0-9]+\.[0-9]+)\"", f"version = \"{version}\"", pyproject_data)

    # Write back to file with proper formatting
    with open(pyproject_toml_path, 'w') as f:
        f.write(pyproject_data)

    print(f"Updated Python version: {current_version} → {version}")
    return True


def main():
    """Main function."""
    try:
        # Read main project version
        main_version = read_main_version()
        print(f"Main project version: {main_version}")

        # Update Python binding version
        updated = update_pyproject_toml(main_version)

        if updated:
            print("Python binding version synchronized successfully")
        else:
            print("No changes needed")
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
