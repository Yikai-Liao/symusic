# NumPy-style Docstrings — Deep Reference

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Example Function Documentation | 13–56 | Complete NumPy-style docstring example with all sections |
| Common Scientific Dependencies | 57–70 | Standard dependency declarations for scientific packages |
| Reproducibility | 71–83 | Project URLs and citation metadata for reproducibility |

---

## Example Function Documentation

```python
def analyze_data(data, threshold=0.5, method="mean"):
    """
    Analyze scientific data above a threshold.

    Parameters
    ----------
    data : array_like
        Input data array to analyze.
    threshold : float, optional
        Minimum value for inclusion in analysis, by default 0.5.
    method : {"mean", "median", "std"}, optional
        Statistical method to apply, by default "mean".

    Returns
    -------
    result : float
        Computed statistical result.

    Raises
    ------
    ValueError
        If method is not recognized.

    Examples
    --------
    >>> import numpy as np
    >>> data = np.array([0.1, 0.6, 0.8, 0.3, 0.9])
    >>> analyze_data(data, threshold=0.5)
    0.7666666666666667

    Notes
    -----
    This function uses NumPy for efficient computation.

    References
    ----------
    .. [1] Harris et al., "Array programming with NumPy", Nature 585, 2020.
    """
    pass
```

## Common Scientific Dependencies

```toml
dependencies = [
    "numpy>=1.20",          # Arrays and numerical computing
    "scipy>=1.7",           # Scientific computing algorithms
    "pandas>=1.3",          # Data structures and analysis
    "matplotlib>=3.4",      # Plotting
    "xarray>=0.19",         # Labeled multi-dimensional arrays
    "scikit-learn>=1.0",    # Machine learning
    "astropy>=5.0",         # Astronomy (if applicable)
]
```

## Reproducibility

Include information for reproducibility:

```toml
[project.urls]
"Source Code" = "https://github.com/org/package"
"Documentation" = "https://package.readthedocs.io"
"Bug Reports" = "https://github.com/org/package/issues"
"Changelog" = "https://github.com/org/package/blob/main/CHANGELOG.md"
"Citation" = "https://doi.org/10.xxxx/xxxxx"  # DOI if available
```
