#!/bin/bash

# Activate virtual environment (.venv only)
if [ -d ".venv" ]; then
    echo "Activating virtual environment at ./.venv ..."
    source .venv/bin/activate
fi

# Clean previous build to ensure proper Pagefind integration
echo "Cleaning previous build..."
rm -rf docs_rtd/_build

# Build Sphinx documentation
echo "Building Sphinx documentation..."
sphinx-build -b html docs_rtd docs_rtd/_build/html

# Run Pagefind indexing
echo "Indexing with Pagefind..."
npx pagefind --site docs_rtd/_build/html --output-subdir _pagefind

echo ""
echo "✓ Done! Documentation built with Pagefind search."
echo ""
echo "To serve locally, run:"
echo "  npx pagefind --serve --site docs_rtd/_build/html"
echo ""
echo "Then visit: http://localhost:1414/search.html"
