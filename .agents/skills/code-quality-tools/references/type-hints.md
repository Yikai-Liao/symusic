# Type Hints Patterns — Deep Reference

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Pattern 6: Gradual Type Hint Adoption | 13–80 | Incrementally adding type hints to scientific code |
| Pattern 7: NumPy Array Type Hints | 81–127 | Using numpy.typing for proper array annotations |
| Pattern 8: Handling Optional and Union Types | 128–170 | Typing functions with optional parameters and multiple types |

---

## Pattern 6: Gradual Type Hint Adoption

Add type hints incrementally to existing scientific code:

**Step 1: Start with function signatures**
```python
import numpy as np
from numpy.typing import NDArray

def calculate_statistics(data: NDArray) -> dict:
    """Calculate basic statistics."""
    return {
        "mean": np.mean(data),
        "std": np.std(data),
        "min": np.min(data),
        "max": np.max(data),
    }
```

**Step 2: Add return type details**
```python
from typing import TypedDict
import numpy as np
from numpy.typing import NDArray

class Statistics(TypedDict):
    mean: float
    std: float
    min: float
    max: float

def calculate_statistics(data: NDArray) -> Statistics:
    """Calculate basic statistics."""
    return {
        "mean": float(np.mean(data)),
        "std": float(np.std(data)),
        "min": float(np.min(data)),
        "max": float(np.max(data)),
    }
```

**Step 3: Add internal variable types (optional)**
```python
from typing import TypedDict
import numpy as np
from numpy.typing import NDArray

class Statistics(TypedDict):
    mean: float
    std: float
    min: float
    max: float

def calculate_statistics(data: NDArray) -> Statistics:
    """Calculate basic statistics."""
    mean_val: float = float(np.mean(data))
    std_val: float = float(np.std(data))
    min_val: float = float(np.min(data))
    max_val: float = float(np.max(data))

    return {
        "mean": mean_val,
        "std": std_val,
        "min": min_val,
        "max": max_val,
    }
```

## Pattern 7: NumPy Array Type Hints

Use numpy.typing for proper array annotations in scientific code:

```python
import numpy as np
from numpy.typing import NDArray

# Generic array
def process_array(data: NDArray) -> NDArray:
    """Process numerical array."""
    return data * 2

# Specific dtype
def process_float_array(data: NDArray[np.float64]) -> NDArray[np.float64]:
    """Process float64 array."""
    return data * 2.0

# Multiple dimensions
Vector = NDArray[np.float64]  # 1D array
Matrix = NDArray[np.float64]  # 2D array

def matrix_multiply(a: Matrix, b: Matrix) -> Matrix:
    """Multiply two matrices."""
    return np.matmul(a, b)

# More specific shape hints
def normalize_vector(v: NDArray[np.float64]) -> NDArray[np.float64]:
    """
    Normalize a vector to unit length.

    Parameters
    ----------
    v : NDArray[np.float64]
        Input vector of shape (n,).

    Returns
    -------
    NDArray[np.float64]
        Normalized vector of shape (n,).
    """
    norm = np.linalg.norm(v)
    if norm == 0:
        return v
    return v / norm
```

## Pattern 8: Handling Optional and Union Types

Properly type functions with optional parameters and multiple accepted types:

```python
import numpy as np
from typing import Optional, Union
from pathlib import Path
from numpy.typing import NDArray

def load_data(
    filepath: Union[str, Path],
    delimiter: str = ",",
    skip_rows: Optional[int] = None
) -> NDArray:
    """
    Load data from file.

    Parameters
    ----------
    filepath : str or Path
        Path to data file.
    delimiter : str, default ","
        Column delimiter.
    skip_rows : int, optional
        Number of rows to skip at start.

    Returns
    -------
    NDArray
        Loaded data array.
    """
    # Convert to Path if string
    path = Path(filepath) if isinstance(filepath, str) else filepath

    # Load with optional skip_rows
    kwargs = {"delimiter": delimiter}
    if skip_rows is not None:
        kwargs["skiprows"] = skip_rows

    return np.loadtxt(path, **kwargs)
```
