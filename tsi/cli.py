#!/usr/bin/env python3
"""Command-line interface for TSI package manager."""

import argparse
import sys
from pathlib import Path

from tsi.manager import PackageManager
from tsi.config import Config


def main():
    """Main CLI entry point."""
    parser = argparse.ArgumentParser(
        description="TSI - TheSourceInstaller: Distribution-independent source-based package manager"
    )
    subparsers = parser.add_subparsers(dest="command", help="Available commands")

    # Install command
    install_parser = subparsers.add_parser("install", help="Install a package from source")
    install_parser.add_argument("package", help="Package name or path to package manifest")
    install_parser.add_argument("--prefix", help="Installation prefix (default: ~/.tsi)")
    install_parser.add_argument("--force", action="store_true", help="Force reinstall")

    # Remove command
    remove_parser = subparsers.add_parser("remove", help="Remove an installed package")
    remove_parser.add_argument("package", help="Package name to remove")

    # List command
    list_parser = subparsers.add_parser("list", help="List installed packages")
    list_parser.add_argument("--all", action="store_true", help="Show all packages including dependencies")

    # Search command
    search_parser = subparsers.add_parser("search", help="Search for packages")
    search_parser.add_argument("query", help="Search query")

    # Update command
    update_parser = subparsers.add_parser("update", help="Update a package")
    update_parser.add_argument("package", nargs="?", help="Package name (omit to update all)")

    # Info command
    info_parser = subparsers.add_parser("info", help="Show package information")
    info_parser.add_argument("package", help="Package name")

    # Build command
    build_parser = subparsers.add_parser("build", help="Build a package without installing")
    build_parser.add_argument("package", help="Package name or path to package manifest")
    build_parser.add_argument("--prefix", help="Build prefix (default: ~/.tsi)")

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    config = Config()
    if args.prefix:
        config.prefix = Path(args.prefix).expanduser()

    manager = PackageManager(config)

    try:
        if args.command == "install":
            manager.install(args.package, force=args.force)
        elif args.command == "remove":
            manager.remove(args.package)
        elif args.command == "list":
            manager.list_installed(show_all=args.all)
        elif args.command == "search":
            manager.search(args.query)
        elif args.command == "update":
            manager.update(args.package if args.package else None)
        elif args.command == "info":
            manager.show_info(args.package)
        elif args.command == "build":
            manager.build(args.package)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

