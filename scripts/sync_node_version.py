#!/usr/bin/env python3
"""
Script to synchronize version between libCacheSim main project and Node.js binding.

This script reads the version from version.txt and updates the package.json
in libCacheSim-node to match.
"""

import json
import os
import sys
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


def update_package_json(version):
    """Update package.json with the new version."""
    project_root = get_project_root()
    package_json_path = project_root / "libCacheSim-node" / "package.json"

    if not package_json_path.exists():
        print(f"Error: {package_json_path} not found", file=sys.stderr)
        sys.exit(1)

    # Read current package.json
    with open(package_json_path, 'r') as f:
        package_data = json.load(f)

    current_version = package_data.get('version', 'unknown')

    if current_version == version:
        print(f"Version already up to date: {version}")
        return False

    # Update version
    package_data['version'] = version

    # Write back to file with proper formatting
    with open(package_json_path, 'w') as f:
        json.dump(package_data, f, indent=2)
        f.write('\n')  # Add trailing newline

    print(f"Updated Node.js binding version: {current_version} → {version}")
    return True


def main():
    """Main function."""
    try:
        # Read main project version
        main_version = read_main_version()
        print(f"Main project version: {main_version}")

        # Update Node.js binding version
        updated = update_package_json(main_version)

        if updated:
            print("✓ Node.js binding version synchronized successfully")
        else:
            print("✓ No changes needed")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
