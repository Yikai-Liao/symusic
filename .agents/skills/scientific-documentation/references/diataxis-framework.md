# The Diátaxis Framework for Scientific Python Documentation — Deep Reference

The [Diátaxis framework](https://diataxis.fr/) provides a systematic approach to organizing technical documentation into four distinct categories based on user needs and contexts.

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Framework Overview | 20–46 | Two-dimensional model of documentation with four quadrants |
| 1. Tutorials (Learning-oriented) | 47–172 | Guide newcomers through hands-on learning experiences |
| 2. How-to Guides (Task-oriented) | 173–294 | Practical guidance for solving specific problems |
| 3. Reference (Information-oriented) | 295–431 | Complete, accurate technical specifications |
| 4. Explanation (Understanding-oriented) | 432–610 | Conceptual background and design rationale |
| Applying Diátaxis to Your Project | 611–667 | Auditing, separating, and organizing existing docs |
| Common Questions | 668–681 | FAQs about applying the framework |
| Resources | 682–686 | Official Diataxis site, talks, and guides |

---

## Framework Overview

Diátaxis organizes documentation along two dimensions:

1. **Acquisition vs Application** - Is the user learning or doing?
2. **Practical vs Theoretical** - Are they working with code or concepts?

This creates four quadrants:

```
                  PRACTICAL STEPS
                        │
    ┌───────────────────┼───────────────────┐
    │                   │                   │
    │    TUTORIALS      │    HOW-TO GUIDES  │
    │   (Learning)      │     (Problem)     │
    │                   │                   │
ACQUISITION ───────────────────────────── APPLICATION
    │                   │                   │
    │   EXPLANATION     │    REFERENCE      │
    │  (Understanding)  │  (Information)    │
    │                   │                   │
    └───────────────────┼───────────────────┘
                        │
                 THEORETICAL KNOWLEDGE
```

## 1. Tutorials (Learning-oriented)

### Purpose
Guide newcomers through a learning experience to acquire basic competence.

### Characteristics
- **Goal**: Help users **learn** by **doing**
- **Audience**: Beginners, newcomers
- **Tone**: Encouraging, supportive
- **Approach**: Step-by-step, hands-on
- **Focus**: Success and confidence building

### What to Include
- Clear learning objectives
- Prerequisites and setup
- Sequential, logical steps
- Complete working examples
- Expected results at each stage
- Minimal explanation (link to Explanation instead)
- Safe, controlled environment

### What to Avoid
- Production-level complexity
- Deep theoretical explanations
- Optional steps or variations
- Error handling edge cases
- "What if" scenarios

### Scientific Python Example: NumPy Tutorial

**Good Tutorial:**
```markdown
# Tutorial: Your First NumPy Array Analysis

## What You'll Learn
In this tutorial, you'll learn to:
- Create NumPy arrays from data
- Compute basic statistics
- Visualize results

## Prerequisites
- Python 3.9 or later installed
- Basic Python knowledge (lists, functions)

## Step 1: Install NumPy

Install NumPy using pip:

\`\`\`bash
pip install numpy matplotlib
\`\`\`

## Step 2: Create Your First Array

Create a new file `analysis.py` and add:

\`\`\`python
import numpy as np

# Create an array of temperatures in Celsius
temperatures = np.array([20, 22, 19, 23, 21, 20, 22])
print(f"Temperatures: {temperatures}")
\`\`\`

Run it:
\`\`\`bash
python analysis.py
\`\`\`

You should see:
\`\`\`
Temperatures: [20 22 19 23 21 20 22]
\`\`\`

## Step 3: Calculate Statistics

Add these lines to compute basic statistics:

\`\`\`python
mean_temp = temperatures.mean()
std_temp = temperatures.std()

print(f"Mean temperature: {mean_temp:.1f}°C")
print(f"Standard deviation: {std_temp:.1f}°C")
\`\`\`

You should see:
\`\`\`
Mean temperature: 21.0°C
Standard deviation: 1.2°C
\`\`\`

## Step 4: Visualize the Data

Add visualization:

\`\`\`python
import matplotlib.pyplot as plt

days = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']
plt.plot(days, temperatures, marker='o')
plt.axhline(mean_temp, color='r', linestyle='--', label='Mean')
plt.ylabel('Temperature (°C)')
plt.title('Weekly Temperature')
plt.legend()
plt.savefig('temperatures.png')
print("Saved plot to temperatures.png")
\`\`\`

## What You've Learned
- Creating NumPy arrays from lists
- Computing mean and standard deviation
- Creating simple line plots

## Next Steps
- [How to: Handle missing data](../guides/missing-data.md)
- [Explanation: NumPy array concepts](../explanation/arrays.md)
```

**Why This Works:**
- Clear, achievable learning objectives
- Explicit prerequisites
- Every step builds on the last
- Shows expected output
- Encourages without overwhelming

## 2. How-to Guides (Task-oriented)

### Purpose
Provide practical guidance to accomplish specific tasks.

### Characteristics
- **Goal**: Help users **solve** a **problem**
- **Audience**: Users with basic competence
- **Tone**: Direct, practical
- **Approach**: Recipe-based, goal-oriented
- **Focus**: Solving real-world problems

### What to Include
- Clear problem statement
- Assumption of basic knowledge
- Step-by-step solution
- Alternative approaches
- When to use this approach
- Links to related guides

### What to Avoid
- Teaching basics (link to Tutorials)
- Complete explanations (link to Explanation)
- Every possible variation
- Theoretical background

### Scientific Python Example: Handling Large Datasets

**Good How-to Guide:**
```markdown
# How to: Process Datasets Larger Than Memory

## Problem
You need to analyze a 10GB dataset, but your computer only has 8GB of RAM.

## Prerequisites
- Familiarity with NumPy and Pandas
- Dataset in CSV or HDF5 format

## Solution 1: Chunked Processing (CSV)

For CSV files, process in chunks:

\`\`\`python
import pandas as pd

chunk_size = 100000  # Rows per chunk
results = []

for chunk in pd.read_csv('large_file.csv', chunksize=chunk_size):
    # Process each chunk
    result = chunk.groupby('category')['value'].mean()
    results.append(result)

# Combine results
final_result = pd.concat(results).groupby(level=0).mean()
\`\`\`

**When to use:** Text-based formats (CSV, JSON) with simple operations.

## Solution 2: Memory Mapping (Binary)

For NumPy arrays, use memory mapping:

\`\`\`python
import numpy as np

# Create memory-mapped array (doesn't load into RAM)
data = np.memmap('large_array.dat', dtype='float32',
                  mode='r', shape=(1000000, 100))

# Operate on slices
mean_values = data[:, 0].mean()  # Only loads one column
\`\`\`

**When to use:** Binary formats, need random access, simple computations.

## Solution 3: Dask (Parallel Processing)

For complex operations, use Dask:

\`\`\`python
import dask.dataframe as dd

# Read with Dask (lazy loading)
df = dd.read_csv('large_file.csv')

# Operations are lazy
result = df.groupby('category')['value'].mean()

# Compute when needed
result.compute()
\`\`\`

**When to use:** Complex operations, want parallel processing, multiple cores.

## Solution Comparison

| Method | Best For | Pros | Cons |
|--------|----------|------|------|
| Chunking | Simple aggregations | No dependencies, straightforward | Sequential processing |
| Memory mapping | Random access | Fast, low memory | Binary format only |
| Dask | Complex workflows | Parallel, familiar API | Additional dependency |

## Common Pitfalls
- Chunk size too large → Out of memory
- Chunk size too small → Slow processing
- Forgetting to combine chunk results

## See Also
- [Tutorial: Getting started with Dask](../tutorials/dask-intro.md)
- [Explanation: Memory management](../explanation/memory.md)
- [Reference: Dask API](../reference/dask.md)
```

**Why This Works:**
- Starts with clear problem
- Multiple solutions for different contexts
- Practical code examples
- Comparison table for decision-making
- Links to deeper resources

## 3. Reference (Information-oriented)

### Purpose
Provide accurate, complete technical specification.

### Characteristics
- **Goal**: Provide **information**
- **Audience**: Users who know what they're looking for
- **Tone**: Neutral, factual
- **Approach**: Systematic, comprehensive
- **Focus**: Accuracy and completeness

### What to Include
- Complete parameter descriptions
- Return value specifications
- Type information
- Possible exceptions
- Working examples
- Cross-references
- Version information

### What to Avoid
- Tutorials or how-tos
- Conceptual explanations
- Incomplete information
- Opinions or recommendations

### Scientific Python Example: API Reference

**Good API Reference (Auto-generated from Docstring):**

```python
def compute_statistic(data, method='mean', axis=0, weights=None,
                      ddof=0, keepdims=False):
    """
    Compute a statistical measure along the specified axis.

    Parameters
    ----------
    data : array_like
        Input data array. Can be any shape.
    method : {'mean', 'median', 'std', 'var'}, default='mean'
        Statistical method to compute:

        - 'mean' : Arithmetic mean
        - 'median' : Median value
        - 'std' : Standard deviation
        - 'var' : Variance

    axis : int or None, default=0
        Axis along which to compute the statistic. If None, compute
        over the flattened array.
    weights : array_like, optional
        Weights for each value in `data`. Must be broadcastable to
        the same shape as `data` along the specified axis. If None,
        all values are weighted equally.
    ddof : int, default=0
        Delta degrees of freedom for variance and standard deviation.
        The divisor used is N - ddof.
    keepdims : bool, default=False
        If True, the reduced axes are left in the result as dimensions
        with size one.

    Returns
    -------
    result : ndarray
        Computed statistic. The shape depends on `axis` and `keepdims`.

    Raises
    ------
    ValueError
        If `method` is not one of the supported methods.
    ValueError
        If `weights` shape is incompatible with `data` shape.
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
    For weighted statistics, the weighted mean is computed as:

    .. math::
        \\bar{x} = \\frac{\\sum_{i=1}^{n} w_i x_i}{\\sum_{i=1}^{n} w_i}

    The weighted variance uses the reliability weights formula [1]_:

    .. math::
        s^2 = \\frac{\\sum_{i=1}^{n} w_i (x_i - \\bar{x})^2}{\\sum_{i=1}^{n} w_i - \\text{ddof}}

    References
    ----------
    .. [1] Bevington, P. R., & Robinson, D. K. (2003). Data reduction
           and error analysis for the physical sciences (3rd ed.).
           McGraw-Hill.

    Examples
    --------
    Basic usage:

    >>> import numpy as np
    >>> from mypackage import compute_statistic
    >>> data = np.array([1, 2, 3, 4, 5])
    >>> compute_statistic(data, method='mean')
    3.0

    With weights:

    >>> weights = np.array([1, 1, 1, 1, 10])
    >>> compute_statistic(data, method='mean', weights=weights)
    4.285714285714286

    Multi-dimensional:

    >>> data_2d = np.array([[1, 2, 3], [4, 5, 6]])
    >>> compute_statistic(data_2d, method='mean', axis=0)
    array([2.5, 3.5, 4.5])

    >>> compute_statistic(data_2d, method='mean', axis=1)
    array([2., 5.])
    """
```

**Why This Works:**
- Complete parameter documentation
- All variants and options covered
- Mathematical formulas when relevant
- References to scientific literature
- Runnable examples
- Clear type information

## 4. Explanation (Understanding-oriented)

### Purpose
Illuminate concepts, provide context, and deepen understanding.

### Characteristics
- **Goal**: Help users **understand**
- **Audience**: Users seeking conceptual knowledge
- **Tone**: Discursive, exploratory
- **Approach**: Conceptual, contextual
- **Focus**: Understanding and insight

### What to Include
- Conceptual background
- Design decisions and rationale
- Alternative approaches and trade-offs
- Historical context
- Connections to broader concepts
- Diagrams and visualizations

### What to Avoid
- Step-by-step instructions
- Complete code examples (snippets OK)
- API details (link to Reference)
- Solving specific problems

### Scientific Python Example: Architecture Explanation

**Good Explanation:**
```markdown
# Understanding NumPy's Array Architecture

## Why NumPy Arrays Exist

Python lists are flexible and easy to use, but they have fundamental
limitations for numerical computing:

1. **Memory overhead**: Each Python object carries metadata
2. **Type heterogeneity**: Lists can hold mixed types, requiring type checks
3. **Lack of vectorization**: Operations require explicit loops

NumPy arrays address these limitations through a different architecture.

## Core Design: Contiguous Memory

NumPy arrays store data in contiguous memory blocks:

\`\`\`
Python List:              NumPy Array:
┌─────┐                  ┌─┬─┬─┬─┬─┐
│ ptr │─→ [obj]          │1│2│3│4│5│  (contiguous memory)
└─────┘                  └─┴─┴─┴─┴─┘
┌─────┐                       ↑
│ ptr │─→ [obj]               │
└─────┘                  Single pointer
┌─────┐
│ ptr │─→ [obj]
└─────┘
```

This design enables:
- **Cache efficiency**: Data is localized
- **Vectorization**: SIMD instructions can process multiple elements
- **Memory efficiency**: No per-element overhead

## The ndarray Structure

A NumPy array has metadata separate from data:

```
ndarray object
├── shape: (3, 4)          # Dimensions
├── dtype: float64          # Element type
├── strides: (32, 8)       # Memory layout
├── data: ptr              # Points to actual data
└── flags: C_CONTIGUOUS    # Memory layout info
```

### Strides: The Key to Efficiency

Strides determine how to navigate the array in memory:

\`\`\`python
arr = np.array([[1, 2, 3], [4, 5, 6]])  # shape: (2, 3)
# Memory: [1, 2, 3, 4, 5, 6]
# strides: (24, 8)  # bytes to next row, bytes to next column
\`\`\`

This enables **views without copying**:

\`\`\`python
arr[0]       # View of first row - no copy!
arr[:, 0]    # View of first column - no copy!
arr.T        # Transposed view - just changes strides!
\`\`\`

## Broadcasting: Conceptual Foundation

Broadcasting allows operations on different shapes:

```
    (3, 1)       (3, 4)       (3, 4)
    ┌─┐          ┌─┬─┬─┬─┐   ┌─┬─┬─┬─┐
    │1│    +     │ │ │ │ │ = │ │ │ │ │
    ├─┤          └─┴─┴─┴─┘   └─┴─┴─┴─┘
    │2│
    ├─┤
    │3│
    └─┘

    Broadcast  → Same shape → Result
```

Broadcasting rules:
1. Align dimensions from right to left
2. Dimensions must be equal or one of them is 1
3. Missing dimensions are treated as 1

**Why this design?** Memory efficiency - no data copying.

## Universal Functions (ufuncs)

Ufuncs operate element-wise with optimization:

\`\`\`python
np.sqrt(arr)  # Implemented in C, uses SIMD
\`\`\`

Instead of Python:
\`\`\`python
[math.sqrt(x) for x in arr]  # Slow Python loop
\`\`\`

The ufunc:
1. Operates on native types (no Python overhead)
2. Uses vectorized CPU instructions
3. Handles broadcasting automatically
4. Can be parallelized

## Design Trade-offs

| Choice | Benefit | Cost |
|--------|---------|------|
| Fixed type | Speed, memory | Less flexible |
| Contiguous memory | Cache efficiency | Copy on resize |
| View semantics | Memory efficient | Can confuse beginners |
| Broadcasting | Concise code | Implicit behavior |

## Historical Context

NumPy built on Numeric (1995) and Numarray (2001), learning from both:
- **From Numeric**: Simplicity and performance
- **From Numarray**: Large array support and type system
- **NumPy's synthesis** (2006): Best of both worlds

## Implications for Users

Understanding this architecture explains why:
- Slicing is fast (views)
- Modifying a slice modifies the original (view semantics)
- `.copy()` is sometimes necessary
- Some operations are much faster than others
- Memory layout matters for performance

## Further Reading

- [NumPy Paper (2011)](https://doi.org/10.1109/MCSE.2011.37)
- [Guide to NumPy](https://web.mit.edu/dvp/Public/numpybook.pdf)
- [Memory layout tutorial](../tutorials/memory-layout.md)
```

**Why This Works:**
- Explains the "why" not just "how"
- Provides conceptual models (diagrams)
- Discusses design decisions and trade-offs
- Gives historical context
- Connects to practical implications
- Links to related resources

## Applying Diátaxis to Your Project

### 1. Audit Your Existing Documentation

For each piece of content, ask:
- What need does this serve?
- What quadrant does it belong to?
- Is it trying to do too many things?

### 2. Separate Mixed Content

Split pages that mix categories:

**Before (Mixed):**
```markdown
# Data Loading

Our package loads data efficiently [EXPLANATION].
Here's how to load a file [HOW-TO]:
...
For beginners, follow this tutorial [TUTORIAL]:
...
API reference [REFERENCE]:
...
```

**After (Separated):**
- Tutorial: "Your first data load"
- How-to: "Load large files efficiently"
- Reference: "load_data() API"
- Explanation: "Data loading architecture"

### 3. Fill Gaps

Create missing content in underserved quadrants.

### 4. Organize Navigation

Structure your documentation to reflect Diátaxis:

```
docs/
├── index.md
├── tutorials/
│   ├── index.md
│   └── ...
├── guides/          # How-tos
│   ├── index.md
│   └── ...
├── reference/
│   ├── index.md
│   └── ...
└── explanation/
    ├── index.md
    └── ...
```

## Common Questions

**Q: Can one page span multiple categories?**
A: No. Keep them separate and cross-link.

**Q: Where do FAQs go?**
A: Usually how-to guides (problem-solving).

**Q: Where does changelog go?**
A: Reference (factual information).

**Q: What about installation instructions?**
A: Tutorial for detailed walkthrough, reference for quick commands.

## Resources

- [Diátaxis Official Site](https://diataxis.fr/)
- [Diátaxis in Practice (PyCon Talk)](https://www.youtube.com/watch?v=t4vKPhjcMZg)
- [Scientific Python Documentation Guide](https://learn.scientific-python.org/development/guides/docs/)
