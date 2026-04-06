# NumPy-style Docstring Examples — Deep Reference

Comprehensive examples of NumPy-style docstrings for scientific Python code.

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Module Docstrings | 24–99 | Simple and complete module-level docstring examples |
| Function Docstrings | 100–261 | Simple and fully-documented function docstrings |
| Class Docstrings | 262–412 | Simple and comprehensive class docstring patterns |
| Method Docstrings | 413–488 | Simple and complete method docstring examples |
| Property Docstrings | 489–524 | Property docstrings with return types and errors |
| Generator Docstrings | 525–557 | Generator function docstrings with Yields section |
| Special Sections | 558–666 | See Also, Notes, Examples, Warnings, and Deprecated |
| Type Hints Integration | 667–705 | Combining type hints with docstrings |
| Common Patterns | 706–777 | Validator functions and decorator docstrings |
| Best Practices | 778–788 | Eight guidelines for effective docstrings |
| Tools for Validation | 789–810 | numpydoc, pydocstyle, and darglint validation |
| Resources | 811–815 | Links to NumPy style guide, PEP 257, and numpydoc docs |

---

## Module Docstrings

### Simple Module

```python
"""Statistical analysis utilities for scientific data.

This module provides functions for computing descriptive statistics,
hypothesis testing, and exploratory data analysis.
"""
```

### Complete Module

```python
"""
Statistical Analysis Tools
===========================

This module provides comprehensive statistical analysis tools for
scientific research, including descriptive statistics, hypothesis
testing, and data visualization.

The module is organized into several submodules:

- `descriptive`: Descriptive statistics (mean, median, std, etc.)
- `inference`: Hypothesis testing and confidence intervals
- `visualization`: Statistical plots and diagnostics

Main Classes
------------
StatisticalTest
    Framework for statistical hypothesis tests
BootstrapEstimator
    Bootstrap-based estimation and confidence intervals

Main Functions
--------------
compute_statistic
    Compute various statistical measures
test_hypothesis
    Perform statistical hypothesis tests
bootstrap_ci
    Bootstrap confidence intervals

See Also
--------
scipy.stats : Comprehensive statistical functions
statsmodels : Statistical modeling
pingouin : Statistical analysis for research

Examples
--------
Perform a basic t-test:

>>> from mypackage import stats
>>> import numpy as np
>>> sample1 = np.array([1.2, 1.5, 1.3, 1.4, 1.6])
>>> sample2 = np.array([1.8, 2.0, 1.9, 2.1, 1.7])
>>> result = stats.test_hypothesis(sample1, sample2, test='t-test')
>>> print(f"p-value: {result.pvalue:.4f}")

Notes
-----
This module assumes data follows standard conventions:
- Missing values are represented as NaN
- Arrays are NumPy arrays or array-like
- Axis convention follows NumPy standards

References
----------
.. [1] Wilcox, R. R. (2010). Fundamentals of Modern Statistical
       Methods. Springer.
"""
```

## Function Docstrings

### Simple Function

```python
def mean(data):
    """
    Compute the arithmetic mean.

    Parameters
    ----------
    data : array_like
        Input data.

    Returns
    -------
    float
        The arithmetic mean.
    """
```

### Complete Function with All Sections

```python
def compute_statistic(data, method='mean', axis=0, weights=None,
                      ddof=0, keepdims=False, nan_policy='propagate'):
    """
    Compute a statistical measure along the specified axis.

    This function calculates various statistical measures for the input
    data, with support for weighted calculations, missing data handling,
    and multi-dimensional arrays.

    Parameters
    ----------
    data : array_like
        Input data array. Can be any shape. For complex data, operates
        on the absolute values.
    method : {'mean', 'median', 'std', 'var', 'mad'}, default='mean'
        Statistical method to compute:

        - 'mean' : Arithmetic mean
        - 'median' : Median value
        - 'std' : Standard deviation
        - 'var' : Variance
        - 'mad' : Median absolute deviation

    axis : int, tuple of int, or None, default=0
        Axis or axes along which to compute the statistic. If None,
        compute over the flattened array.
    weights : array_like, optional
        Weights for each value in `data`. Must be broadcastable to
        the same shape as `data` along the specified axis. If None,
        all values are weighted equally.
    ddof : int, default=0
        Delta degrees of freedom for variance and standard deviation.
        The divisor used is N - ddof, where N is the number of elements.
    keepdims : bool, default=False
        If True, the reduced axes are left in the result as dimensions
        with size one. With this option, the result will broadcast
        correctly against the input array.
    nan_policy : {'propagate', 'raise', 'omit'}, default='propagate'
        Defines how to handle NaN values:

        - 'propagate': Return NaN
        - 'raise': Raise a ValueError
        - 'omit': Ignore NaN values

    Returns
    -------
    result : ndarray or scalar
        Computed statistic. The shape depends on `axis` and `keepdims`.
        If `axis=None`, returns a scalar.

    Raises
    ------
    ValueError
        If `method` is not one of the supported methods.
    ValueError
        If `weights` shape is incompatible with `data` shape.
    ValueError
        If `nan_policy='raise'` and data contains NaN.
    ZeroDivisionError
        If all weights sum to zero.

    See Also
    --------
    numpy.mean : Compute the arithmetic mean.
    numpy.median : Compute the median.
    numpy.std : Compute the standard deviation.
    scipy.stats.describe : Compute several descriptive statistics.

    Notes
    -----
    The weighted mean is computed as:

    .. math::
        \\bar{x} = \\frac{\\sum_{i=1}^{n} w_i x_i}{\\sum_{i=1}^{n} w_i}

    For the weighted variance, we use the reliability weights formula
    (also called frequency weights) [1]_:

    .. math::
        s^2 = \\frac{\\sum_{i=1}^{n} w_i (x_i - \\bar{x})^2}
                   {\\sum_{i=1}^{n} w_i - \\text{ddof}}

    The median absolute deviation (MAD) is defined as:

    .. math::
        \\text{MAD} = \\text{median}(|X_i - \\text{median}(X)|)

    Performance: This function is optimized for large arrays using
    NumPy's compiled routines. For very large arrays (>1GB), consider
    using chunked processing.

    References
    ----------
    .. [1] Bevington, P. R., & Robinson, D. K. (2003). Data reduction
           and error analysis for the physical sciences (3rd ed.).
           McGraw-Hill.
    .. [2] Rousseeuw, P. J., & Croux, C. (1993). Alternatives to the
           median absolute deviation. Journal of the American Statistical
           Association, 88(424), 1273-1283.

    Examples
    --------
    Basic usage with 1D array:

    >>> import numpy as np
    >>> from mypackage import compute_statistic
    >>> data = np.array([1, 2, 3, 4, 5])
    >>> compute_statistic(data, method='mean')
    3.0

    Compute along specific axis for 2D array:

    >>> data_2d = np.array([[1, 2, 3], [4, 5, 6]])
    >>> compute_statistic(data_2d, method='mean', axis=0)
    array([2.5, 3.5, 4.5])

    >>> compute_statistic(data_2d, method='mean', axis=1)
    array([2., 5.])

    Using weights:

    >>> weights = np.array([1, 1, 1, 1, 10])
    >>> compute_statistic(data, method='mean', weights=weights)
    4.285714285714286

    Handle missing data:

    >>> data_with_nan = np.array([1, 2, np.nan, 4, 5])
    >>> compute_statistic(data_with_nan, nan_policy='omit')
    3.0

    Compute standard deviation with degrees of freedom:

    >>> compute_statistic(data, method='std', ddof=1)
    1.5811388300841898
    """
```

## Class Docstrings

### Simple Class

```python
class DataAnalyzer:
    """
    Analyze scientific data with statistical methods.

    Parameters
    ----------
    method : str, default='robust'
        Analysis method to use.

    Attributes
    ----------
    results_ : dict
        Analysis results after calling `fit()`.
    """
```

### Complete Class

```python
class DataAnalyzer:
    """
    Comprehensive statistical data analysis framework.

    This class provides a unified interface for performing statistical
    analysis on scientific datasets, with support for missing data,
    outlier detection, and multiple analysis methods.

    Parameters
    ----------
    method : {'standard', 'robust', 'bayesian'}, default='robust'
        Analysis method to use:

        - 'standard': Classical statistical methods (mean, std)
        - 'robust': Robust methods resistant to outliers (median, MAD)
        - 'bayesian': Bayesian statistical inference

    missing_data : {'remove', 'impute', 'propagate'}, default='remove'
        How to handle missing data:

        - 'remove': Remove samples with missing values
        - 'impute': Impute missing values using median
        - 'propagate': Propagate NaN through calculations

    significance_level : float, default=0.05
        Significance level for statistical tests (alpha).
        Must be between 0 and 1.
    outlier_threshold : float, default=3.0
        Threshold for outlier detection in standard deviations
        (for standard method) or MAD units (for robust method).
    random_state : int or None, default=None
        Random state for reproducibility. Only used for Bayesian method.

    Attributes
    ----------
    results_ : dict
        Dictionary of analysis results, populated after calling `fit()`.
        Contains keys: 'mean', 'std', 'median', 'mad', 'outliers'.
    n_samples_ : int
        Number of samples processed (after handling missing data).
    n_features_ : int
        Number of features in the dataset.
    outlier_mask_ : ndarray of bool
        Boolean mask indicating outliers (True = outlier).
    is_fitted_ : bool
        Whether the analyzer has been fitted to data.

    Methods
    -------
    fit(X)
        Fit the analyzer to data and compute statistics.
    transform(X)
        Transform data by removing outliers and handling missing values.
    fit_transform(X)
        Fit to data and return transformed data.
    get_summary()
        Get a summary of analysis results.

    Examples
    --------
    Basic usage:

    >>> import numpy as np
    >>> from mypackage import DataAnalyzer
    >>> data = np.random.randn(100, 5)
    >>> analyzer = DataAnalyzer(method='robust')
    >>> analyzer.fit(data)
    DataAnalyzer(method='robust')
    >>> print(analyzer.results_['median'])

    Handle missing data:

    >>> data_with_nan = data.copy()
    >>> data_with_nan[0, 0] = np.nan
    >>> analyzer = DataAnalyzer(missing_data='impute')
    >>> analyzer.fit(data_with_nan)
    DataAnalyzer(missing_data='impute')

    Detect and remove outliers:

    >>> analyzer = DataAnalyzer(outlier_threshold=2.5)
    >>> analyzer.fit(data)
    >>> clean_data = analyzer.transform(data)
    >>> print(f"Removed {analyzer.outlier_mask_.sum()} outliers")

    See Also
    --------
    scipy.stats : Statistical functions
    sklearn.preprocessing : Data preprocessing utilities

    Notes
    -----
    The robust method uses median absolute deviation (MAD) for scale
    estimation:

    .. math::
        \\text{MAD} = \\text{median}(|X_i - \\text{median}(X)|)

    Outliers are detected as values beyond:

    .. math::
        \\text{median}(X) \\pm \\text{threshold} \\times \\text{MAD}

    The Bayesian method uses MCMC sampling to estimate posterior
    distributions. This can be computationally expensive for large
    datasets.

    References
    ----------
    .. [1] Rousseeuw, P.J. and Croux, C. (1993) "Alternatives to the
           Median Absolute Deviation", Journal of the American Statistical
           Association, Vol. 88, pp. 1273-1283.
    .. [2] Gelman, A., et al. (2013). Bayesian Data Analysis, Third
           Edition. Chapman and Hall/CRC.
    """

    def __init__(self, method='robust', missing_data='remove',
                 significance_level=0.05, outlier_threshold=3.0,
                 random_state=None):
        self.method = method
        self.missing_data = missing_data
        self.significance_level = significance_level
        self.outlier_threshold = outlier_threshold
        self.random_state = random_state
        self.is_fitted_ = False
```

## Method Docstrings

### Simple Method

```python
def fit(self, X):
    """
    Fit the analyzer to data.

    Parameters
    ----------
    X : array_like, shape (n_samples, n_features)
        Training data.

    Returns
    -------
    self : object
        Returns the instance itself.
    """
```

### Complete Method

```python
def fit(self, X, y=None, sample_weight=None):
    """
    Fit the analyzer to data and compute statistics.

    This method computes summary statistics, detects outliers, and
    prepares the analyzer for transforming new data.

    Parameters
    ----------
    X : array_like, shape (n_samples, n_features)
        Training data. Each row is a sample, each column is a feature.
    y : array_like, shape (n_samples,), optional
        Target values. Not used, present for API consistency.
    sample_weight : array_like, shape (n_samples,), optional
        Individual weights for each sample. If None, all samples are
        weighted equally.

    Returns
    -------
    self : DataAnalyzer
        Returns the fitted instance.

    Raises
    ------
    ValueError
        If X contains non-finite values and missing_data='propagate'.
    ValueError
        If sample_weight contains negative values.

    Notes
    -----
    After fitting, the following attributes are set:
    - `results_`: Dictionary of computed statistics
    - `n_samples_`: Number of samples (after handling missing data)
    - `n_features_`: Number of features
    - `outlier_mask_`: Boolean mask for outliers
    - `is_fitted_`: Set to True

    Examples
    --------
    >>> import numpy as np
    >>> from mypackage import DataAnalyzer
    >>> X = np.random.randn(100, 3)
    >>> analyzer = DataAnalyzer()
    >>> analyzer.fit(X)
    DataAnalyzer()
    >>> print(analyzer.n_samples_)
    100
    >>> print(analyzer.results_['mean'])
    """
```

## Property Docstrings

```python
@property
def summary_statistics(self):
    """
    Get summary statistics for the fitted data.

    Returns
    -------
    dict
        Dictionary containing:

        - 'mean': Mean values for each feature
        - 'std': Standard deviations
        - 'median': Median values
        - 'mad': Median absolute deviations
        - 'min': Minimum values
        - 'max': Maximum values
        - 'q25': 25th percentile
        - 'q75': 75th percentile

    Raises
    ------
    RuntimeError
        If the analyzer has not been fitted yet.

    Examples
    --------
    >>> analyzer = DataAnalyzer()
    >>> analyzer.fit(data)
    >>> stats = analyzer.summary_statistics
    >>> print(f"Mean: {stats['mean']}")
    """
```

## Generator Docstrings

```python
def iter_chunks(data, chunk_size=1000):
    """
    Iterate over data in chunks.

    This generator yields successive chunks of the input data, useful
    for processing large datasets that don't fit in memory.

    Parameters
    ----------
    data : array_like, shape (n_samples, n_features)
        Input data to chunk.
    chunk_size : int, default=1000
        Number of samples per chunk.

    Yields
    ------
    chunk : ndarray, shape (chunk_size, n_features)
        Successive chunks of data. The last chunk may be smaller
        if n_samples is not divisible by chunk_size.

    Examples
    --------
    >>> import numpy as np
    >>> data = np.random.randn(10000, 5)
    >>> for chunk in iter_chunks(data, chunk_size=1000):
    ...     result = process(chunk)
    ...     print(f"Processed {len(chunk)} samples")
    """
```

## Special Sections

### See Also Section

```python
"""
See Also
--------
numpy.mean : Compute the arithmetic mean.
numpy.median : Compute the median along the specified axis.
scipy.stats.describe : Compute several descriptive statistics.
pandas.DataFrame.describe : Generate descriptive statistics.
compute_weighted_mean : Compute weighted arithmetic mean (related function).
"""
```

### Notes Section

```python
"""
Notes
-----
This function uses a numerically stable algorithm for computing
variance, based on Welford's online algorithm [1]_.

The algorithm has O(n) time complexity and O(1) space complexity,
making it suitable for large datasets.

For floating-point data, the result may not be exact due to
rounding errors. The relative error is typically on the order
of machine epsilon (2.22e-16 for float64).

.. math::
    \\sigma^2 = \\frac{1}{n-1} \\sum_{i=1}^{n} (x_i - \\bar{x})^2

where :math:`\\bar{x}` is the sample mean.

References
----------
.. [1] Welford, B. P. (1962). "Note on a method for calculating
       corrected sums of squares and products". Technometrics.
       4 (3): 419-420. doi:10.2307/1266577.
"""
```

### Examples Section

```python
"""
Examples
--------
Basic usage:

>>> import numpy as np
>>> x = np.array([1, 2, 3, 4, 5])
>>> result = function(x)
>>> print(result)
3.0

Using multiple features:

>>> x_2d = np.array([[1, 2], [3, 4], [5, 6]])
>>> function(x_2d, axis=0)
array([3., 4.])

Handling edge cases:

>>> function(np.array([]))  # Empty array
nan

>>> function(np.array([1]))  # Single element
1.0

With optional parameters:

>>> function(x, weights=[1, 1, 1, 1, 10])
4.285714285714286

Plotting results:

>>> import matplotlib.pyplot as plt
>>> x = np.linspace(0, 10, 100)
>>> y = function(x)
>>> plt.plot(x, y)  # doctest: +SKIP
>>> plt.show()  # doctest: +SKIP
"""
```

### Warnings and Deprecated

```python
"""
Warnings
--------
This function may produce unexpected results if the input data
contains NaN or Inf values. Use nan_policy='omit' to handle
missing data.

.. deprecated:: 0.2.0
   `old_param` will be removed in version 1.0.0. Use `new_param`
   instead.

.. versionadded:: 0.1.0

.. versionchanged:: 0.2.0
   Added support for weighted statistics.
"""
```

## Type Hints Integration

```python
from typing import Optional, Union, Literal
import numpy as np
import numpy.typing as npt

def compute_statistic(
    data: npt.ArrayLike,
    method: Literal['mean', 'median', 'std'] = 'mean',
    axis: Optional[Union[int, tuple[int, ...]]] = 0,
    weights: Optional[npt.ArrayLike] = None,
) -> Union[np.ndarray, float]:
    """
    Compute a statistical measure along the specified axis.

    Parameters
    ----------
    data
        Input data array.
    method
        Statistical method to compute.
    axis
        Axis along which to compute the statistic.
    weights
        Weights for each value in `data`.

    Returns
    -------
    result
        Computed statistic.

    Notes
    -----
    Type hints are automatically integrated into the documentation
    when using `sphinx-autodoc-typehints` extension.
    """
```

## Common Patterns

### Validator Function

```python
def validate_input(data, shape=None, dtype=None):
    """
    Validate input data array.

    Parameters
    ----------
    data : array_like
        Input data to validate.
    shape : tuple of int, optional
        Expected shape. None to skip shape validation.
    dtype : dtype, optional
        Expected data type. None to skip dtype validation.

    Returns
    -------
    ndarray
        Validated and converted NumPy array.

    Raises
    ------
    ValueError
        If shape doesn't match expected shape.
    TypeError
        If dtype doesn't match expected dtype.

    Examples
    --------
    >>> validate_input([1, 2, 3], shape=(3,))
    array([1, 2, 3])

    >>> validate_input([[1, 2], [3, 4]], dtype=float)
    array([[1., 2.],
           [3., 4.]])
    """
```

### Decorator

```python
def validate_fitted(method):
    """
    Decorator to validate that estimator is fitted before use.

    Parameters
    ----------
    method : callable
        Method to decorate.

    Returns
    -------
    callable
        Decorated method.

    Raises
    ------
    RuntimeError
        If estimator is not fitted.

    Examples
    --------
    >>> class Estimator:
    ...     @validate_fitted
    ...     def predict(self, X):
    ...         return self.transform(X)
    """
```

## Best Practices

1. **Be Concise**: First line should be a one-line summary
2. **Be Complete**: Document all parameters, returns, and exceptions
3. **Use Math**: Include mathematical formulas for algorithms
4. **Give Examples**: Provide runnable examples
5. **Cross-Reference**: Link to related functions
6. **Cite Sources**: Reference papers for algorithms
7. **Test Examples**: Use doctest to ensure examples work
8. **Update Together**: Update docstrings when changing code

## Tools for Validation

### numpydoc validation

```bash
python -m numpydoc --validate mymodule.myfunction
```

### pydocstyle

```bash
pip install pydocstyle
pydocstyle mypackage/
```

### darglint

```bash
pip install darglint
darglint -v 2 mypackage/
```

## Resources

- [NumPy Documentation Style Guide](https://numpydoc.readthedocs.io/en/latest/format.html)
- [PEP 257 – Docstring Conventions](https://www.python.org/dev/peps/pep-0257/)
- [numpydoc Documentation](https://numpydoc.readthedocs.io/)
