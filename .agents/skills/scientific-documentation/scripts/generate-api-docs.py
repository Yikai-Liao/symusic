#!/usr/bin/env python
"""
Generate API documentation files for Sphinx.

This script automatically generates reStructuredText files for API
documentation using Sphinx's autosummary. It creates a structured
API reference with one file per module.

Usage:
    python scripts/generate-api-docs.py
    python scripts/generate-api-docs.py --package mypackage --output docs/reference
"""

import argparse
import importlib
import inspect
import sys
from pathlib import Path
from typing import List, Set


def get_public_members(module) -> tuple[List[str], List[str], List[str]]:
    """
    Extract public functions, classes, and submodules from a module.

    Parameters
    ----------
    module : module
        Python module to inspect.

    Returns
    -------
    functions : list of str
        Public function names.
    classes : list of str
        Public class names.
    submodules : list of str
        Public submodule names.
    """
    functions = []
    classes = []
    submodules = []

    for name in dir(module):
        if name.startswith("_"):
            continue

        try:
            obj = getattr(module, name)
        except (AttributeError, ImportError):
            continue

        # Check if defined in this module (not imported)
        if hasattr(obj, "__module__") and not obj.__module__.startswith(
            module.__name__
        ):
            continue

        if inspect.isfunction(obj):
            functions.append(name)
        elif inspect.isclass(obj):
            classes.append(name)
        elif inspect.ismodule(obj) and obj.__name__.startswith(module.__name__):
            submodules.append(name)

    return sorted(functions), sorted(classes), sorted(submodules)


def generate_module_doc(
    module_name: str, output_dir: Path, package_name: str
) -> None:
    """
    Generate reStructuredText documentation for a single module.

    Parameters
    ----------
    module_name : str
        Full module name (e.g., 'mypackage.submodule').
    output_dir : Path
        Directory to write documentation files.
    package_name : str
        Top-level package name.
    """
    try:
        module = importlib.import_module(module_name)
    except ImportError as e:
        print(f"Warning: Could not import {module_name}: {e}")
        return

    functions, classes, submodules = get_public_members(module)

    if not (functions or classes or submodules):
        print(f"Skipping {module_name} (no public members)")
        return

    # Create output file
    filename = output_dir / f"{module_name}.rst"
    filename.parent.mkdir(parents=True, exist_ok=True)

    with open(filename, "w") as f:
        # Title
        title = module_name
        f.write(f"{title}\n")
        f.write("=" * len(title) + "\n\n")

        # Module docstring
        f.write(f".. automodule:: {module_name}\n\n")

        # Functions section
        if functions:
            f.write("Functions\n")
            f.write("-" * 9 + "\n\n")
            f.write(".. autosummary::\n")
            f.write("   :toctree: generated/\n\n")
            for func in functions:
                f.write(f"   {func}\n")
            f.write("\n")

        # Classes section
        if classes:
            f.write("Classes\n")
            f.write("-" * 7 + "\n\n")
            f.write(".. autosummary::\n")
            f.write("   :toctree: generated/\n")
            f.write("   :template: class.rst\n\n")
            for cls in classes:
                f.write(f"   {cls}\n")
            f.write("\n")

        # Submodules section
        if submodules:
            f.write("Submodules\n")
            f.write("-" * 10 + "\n\n")
            f.write(".. autosummary::\n")
            f.write("   :toctree: generated/\n")
            f.write("   :recursive:\n\n")
            for submod in submodules:
                f.write(f"   {submod}\n")
            f.write("\n")

    print(f"Generated: {filename}")


def discover_modules(package_name: str) -> Set[str]:
    """
    Discover all modules in a package.

    Parameters
    ----------
    package_name : str
        Name of the package to discover.

    Returns
    -------
    set of str
        Set of module names.
    """
    try:
        package = importlib.import_module(package_name)
    except ImportError as e:
        print(f"Error: Could not import package {package_name}: {e}")
        sys.exit(1)

    modules = {package_name}

    # Get package path
    if hasattr(package, "__path__"):
        package_path = Path(package.__path__[0])

        # Find all Python files
        for py_file in package_path.rglob("*.py"):
            if py_file.name.startswith("_") and py_file.name != "__init__.py":
                continue

            # Convert file path to module name
            relative_path = py_file.relative_to(package_path.parent)
            module_parts = list(relative_path.parts[:-1])

            if py_file.name != "__init__.py":
                module_parts.append(py_file.stem)

            module_name = ".".join(module_parts)

            if module_name:
                modules.add(module_name)

    return modules


def generate_api_index(
    package_name: str, modules: Set[str], output_dir: Path
) -> None:
    """
    Generate API reference index page.

    Parameters
    ----------
    package_name : str
        Name of the package.
    modules : set of str
        Set of module names.
    output_dir : Path
        Directory containing API documentation.
    """
    filename = output_dir / "api.rst"

    with open(filename, "w") as f:
        title = "API Reference"
        f.write(f"{title}\n")
        f.write("=" * len(title) + "\n\n")

        f.write(
            f"This page contains auto-generated API documentation for {package_name}.\n\n"
        )

        f.write(".. toctree::\n")
        f.write("   :maxdepth: 2\n\n")

        for module in sorted(modules):
            f.write(f"   {module}\n")

    print(f"Generated index: {filename}")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Generate API documentation for Sphinx"
    )
    parser.add_argument(
        "--package",
        "-p",
        required=True,
        help="Package name to document",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=Path("docs/reference"),
        help="Output directory for documentation (default: docs/reference)",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove existing documentation files before generating",
    )

    args = parser.parse_args()

    # Clean output directory if requested
    if args.clean and args.output.exists():
        import shutil

        for item in args.output.glob("*.rst"):
            if item.name != "index.rst":  # Keep index
                item.unlink()
                print(f"Removed: {item}")

        generated_dir = args.output / "generated"
        if generated_dir.exists():
            shutil.rmtree(generated_dir)
            print(f"Removed: {generated_dir}")

    # Create output directory
    args.output.mkdir(parents=True, exist_ok=True)

    print(f"Discovering modules in {args.package}...")
    modules = discover_modules(args.package)
    print(f"Found {len(modules)} modules")

    print("\nGenerating documentation files...")
    for module in sorted(modules):
        generate_module_doc(module, args.output, args.package)

    print("\nGenerating API index...")
    generate_api_index(args.package, modules, args.output)

    print("\nDone! API documentation generated successfully.")
    print(f"Documentation files: {args.output}")


if __name__ == "__main__":
    main()
