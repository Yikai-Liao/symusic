# My Scientific Package

<!-- Replace with your package description -->
A powerful scientific Python package for [describe main purpose].

```{include} ../README.md
:start-after: <!-- start-intro -->
:end-before: <!-- end-intro -->
```

## Quick Start

Install the package:

```bash
pip install mypackage
```

Basic usage:

```python
import mypackage as mpkg
import numpy as np

# Create sample data
data = np.random.randn(100, 5)

# Analyze data
analyzer = mpkg.DataAnalyzer(method='robust')
analyzer.fit(data)

# View results
print(analyzer.results_)
```

## Features

- **Fast and Efficient**: Optimized NumPy operations for large datasets
- **Flexible**: Support for multiple analysis methods and data formats
- **Well-Tested**: Comprehensive test suite with >95% coverage
- **Well-Documented**: Complete API documentation and tutorials

## Getting Started

```{toctree}
:maxdepth: 2

getting-started/installation
getting-started/quickstart
getting-started/configuration
```

## Documentation Structure

This documentation is organized following the [Diátaxis framework](https://diataxis.fr/):

### 📚 Tutorials

**Learning-oriented**: Step-by-step guides to help you learn the basics.

Perfect for newcomers who want to get started quickly.

```{toctree}
:maxdepth: 1
:caption: Tutorials

tutorials/index
tutorials/first-analysis
tutorials/data-loading
tutorials/visualization
```

### 📖 How-to Guides

**Task-oriented**: Practical guides for accomplishing specific goals.

For users who know what they want to do and need specific guidance.

```{toctree}
:maxdepth: 1
:caption: How-to Guides

guides/index
guides/large-datasets
guides/performance
guides/integration
guides/extending
```

### 📄 Reference

**Information-oriented**: Technical descriptions of the API, configuration, and data formats.

For users who need detailed specifications and complete API documentation.

```{toctree}
:maxdepth: 2
:caption: Reference

reference/api
reference/cli
reference/configuration
reference/data-formats
```

### 💡 Explanation

**Understanding-oriented**: Clarification and discussion of key topics.

For users who want to understand the design, architecture, and underlying concepts.

```{toctree}
:maxdepth: 1
:caption: Explanation

explanation/index
explanation/architecture
explanation/design
explanation/algorithms
explanation/performance
```

## Community

```{toctree}
:maxdepth: 1
:caption: Community

contributing
code-of-conduct
citation
changelog
license
```

## Quick Links

- [Installation Guide](getting-started/installation)
- [Quick Start Tutorial](getting-started/quickstart)
- [API Reference](reference/api)
- [GitHub Repository](https://github.com/org/package)
- [Issue Tracker](https://github.com/org/package/issues)

## Citation

If you use this package in your research, please cite:

```bibtex
@software{mypackage2024,
  author = {Your Name},
  title = {My Scientific Package},
  year = {2024},
  version = {0.1.0},
  doi = {10.5281/zenodo.XXXXXX},
  url = {https://github.com/org/package}
}
```

See the [citation guide](citation) for more details.

## Indices and Tables

- {ref}`genindex`
- {ref}`modindex`
- {ref}`search`
