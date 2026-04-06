"""Nox sessions for building and testing documentation.

This file provides automated workflows for documentation building,
link checking, spell checking, and live preview.

Usage:
    nox -s docs          # Build HTML documentation
    nox -s docs_live     # Live preview with auto-reload
    nox -s docs_linkcheck  # Check external links
    nox -s docs_spelling   # Check spelling
    nox -s docs_clean    # Clean build artifacts
"""

import nox

# Default sessions to run when just typing 'nox'
nox.options.sessions = ["docs"]

# Python version for documentation builds
PYTHON_VERSION = "3.12"

# Documentation source and build directories
DOCS_DIR = "docs"
BUILD_DIR = f"{DOCS_DIR}/_build"


@nox.session(python=PYTHON_VERSION)
def docs(session):
    """Build the documentation with Sphinx.

    This session:
    1. Installs package with docs dependencies
    2. Runs sphinx-build to generate HTML documentation
    3. Treats warnings as errors (fail fast)
    """
    session.install(".[docs]")

    session.run(
        "sphinx-build",
        "-W",  # Treat warnings as errors
        "--keep-going",  # Continue on errors to see all issues
        "-b", "html",  # Build HTML
        DOCS_DIR,
        f"{BUILD_DIR}/html",
        *session.posargs,  # Allow passing extra args
    )

    # Print location of built docs
    index_path = f"{BUILD_DIR}/html/index.html"
    session.log(f"Documentation built successfully!")
    session.log(f"Open {index_path} in your browser to view")


@nox.session(python=PYTHON_VERSION)
def docs_live(session):
    """Build and serve documentation with live reload.

    This session:
    1. Installs sphinx-autobuild
    2. Serves documentation on localhost:8000
    3. Automatically rebuilds on file changes
    4. Opens browser automatically
    """
    session.install(".[docs]", "sphinx-autobuild")

    session.run(
        "sphinx-autobuild",
        "-W",  # Treat warnings as errors
        "--open-browser",  # Open browser automatically
        "--port", "8000",
        "--watch", "src",  # Watch source code for changes
        "--ignore", "*.pyc",
        "--ignore", "*~",
        "--ignore", ".git/*",
        DOCS_DIR,
        f"{BUILD_DIR}/html",
        *session.posargs,
    )


@nox.session(python=PYTHON_VERSION)
def docs_linkcheck(session):
    """Check documentation for broken links.

    This session:
    1. Installs package with docs dependencies
    2. Runs linkcheck builder
    3. Reports broken external links
    """
    session.install(".[docs]")

    session.run(
        "sphinx-build",
        "-W",
        "--keep-going",
        "-b", "linkcheck",
        DOCS_DIR,
        f"{BUILD_DIR}/linkcheck",
        *session.posargs,
    )

    # Print results
    session.log("Link check complete!")
    session.log(f"See {BUILD_DIR}/linkcheck/output.txt for results")


@nox.session(python=PYTHON_VERSION)
def docs_spelling(session):
    """Check documentation for spelling errors.

    This session requires:
    - sphinxcontrib-spelling
    - pyenchant (system dependency)

    Install system dependency:
        # macOS
        brew install enchant

        # Ubuntu/Debian
        sudo apt-get install enchant-2

        # Windows
        # Install from https://github.com/AbiWord/enchant
    """
    session.install(".[docs]", "sphinxcontrib-spelling")

    session.run(
        "sphinx-build",
        "-W",
        "--keep-going",
        "-b", "spelling",
        DOCS_DIR,
        f"{BUILD_DIR}/spelling",
        *session.posargs,
    )

    session.log("Spell check complete!")
    session.log(f"See {BUILD_DIR}/spelling/output.txt for results")


@nox.session(python=PYTHON_VERSION)
def docs_coverage(session):
    """Check documentation coverage.

    This session:
    1. Builds documentation with coverage builder
    2. Reports undocumented objects
    """
    session.install(".[docs]")

    session.run(
        "sphinx-build",
        "-b", "coverage",
        DOCS_DIR,
        f"{BUILD_DIR}/coverage",
        *session.posargs,
    )

    session.log("Documentation coverage check complete!")
    session.log(f"See {BUILD_DIR}/coverage/python.txt for results")


@nox.session(python=PYTHON_VERSION)
def docs_doctest(session):
    """Run doctests in documentation.

    This session:
    1. Executes code examples in docstrings
    2. Verifies outputs match expected results
    """
    session.install(".[docs]")

    session.run(
        "sphinx-build",
        "-W",
        "-b", "doctest",
        DOCS_DIR,
        f"{BUILD_DIR}/doctest",
        *session.posargs,
    )

    session.log("Doctest complete!")


@nox.session(python=PYTHON_VERSION)
def docs_pdf(session):
    """Build PDF documentation via LaTeX.

    This session requires:
    - LaTeX distribution (texlive, mactex, etc.)

    Install LaTeX:
        # macOS
        brew install --cask mactex

        # Ubuntu/Debian
        sudo apt-get install texlive-full

        # Windows
        # Install MiKTeX from https://miktex.org/
    """
    session.install(".[docs]")

    # Build LaTeX
    session.run(
        "sphinx-build",
        "-W",
        "-b", "latex",
        DOCS_DIR,
        f"{BUILD_DIR}/latex",
        *session.posargs,
    )

    # Compile PDF
    session.cd(f"{BUILD_DIR}/latex")
    session.run("make", external=True)

    session.log("PDF documentation built successfully!")
    session.log(f"See {BUILD_DIR}/latex/*.pdf")


@nox.session
def docs_clean(session):
    """Clean documentation build artifacts.

    This session:
    1. Removes _build directory
    2. Removes auto-generated files
    """
    import shutil
    from pathlib import Path

    # Remove build directory
    build_path = Path(BUILD_DIR)
    if build_path.exists():
        shutil.rmtree(build_path)
        session.log(f"Removed {BUILD_DIR}")

    # Remove autosummary generated files
    generated_path = Path(DOCS_DIR) / "generated"
    if generated_path.exists():
        shutil.rmtree(generated_path)
        session.log(f"Removed {generated_path}")

    # Remove auto-generated API docs
    api_path = Path(DOCS_DIR) / "api"
    if api_path.exists():
        for f in api_path.glob("*.rst"):
            if f.name != "index.rst":  # Keep index
                f.unlink()
                session.log(f"Removed {f}")

    session.log("Documentation cleaned successfully!")


@nox.session(python=PYTHON_VERSION)
def docs_all(session):
    """Build documentation in all formats.

    This session:
    1. Builds HTML documentation
    2. Checks links
    3. Runs doctests
    4. Checks coverage
    """
    session.install(".[docs]")

    # HTML
    session.log("Building HTML documentation...")
    session.run(
        "sphinx-build",
        "-W", "--keep-going",
        "-b", "html",
        DOCS_DIR,
        f"{BUILD_DIR}/html",
    )

    # Link check
    session.log("Checking links...")
    session.run(
        "sphinx-build",
        "-W", "--keep-going",
        "-b", "linkcheck",
        DOCS_DIR,
        f"{BUILD_DIR}/linkcheck",
    )

    # Doctest
    session.log("Running doctests...")
    session.run(
        "sphinx-build",
        "-W",
        "-b", "doctest",
        DOCS_DIR,
        f"{BUILD_DIR}/doctest",
    )

    # Coverage
    session.log("Checking coverage...")
    session.run(
        "sphinx-build",
        "-b", "coverage",
        DOCS_DIR,
        f"{BUILD_DIR}/coverage",
    )

    session.log("All documentation builds complete!")


# MkDocs sessions (alternative to Sphinx)

@nox.session(python=PYTHON_VERSION)
def docs_mkdocs(session):
    """Build documentation with MkDocs.

    Alternative to Sphinx for projects using MkDocs.
    """
    session.install(".[docs]")

    session.run(
        "mkdocs",
        "build",
        "--strict",  # Treat warnings as errors
        *session.posargs,
    )

    session.log("MkDocs documentation built successfully!")
    session.log("Open site/index.html in your browser to view")


@nox.session(python=PYTHON_VERSION)
def docs_mkdocs_serve(session):
    """Serve MkDocs documentation with live reload."""
    session.install(".[docs]")

    session.run(
        "mkdocs",
        "serve",
        "--dev-addr", "localhost:8000",
        *session.posargs,
    )
