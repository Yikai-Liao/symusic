# tests/conftest.py
import pytest
import numpy as np
from pathlib import Path

@pytest.fixture(scope="session")
def test_data_dir():
    """Provide path to test data directory."""
    return Path(__file__).parent / "data"

@pytest.fixture
def sample_array():
    """Provide sample NumPy array."""
    np.random.seed(42)
    return np.random.randn(100)

@pytest.fixture
def temp_output_dir(tmp_path):
    """Provide temporary directory for test outputs."""
    output_dir = tmp_path / "output"
    output_dir.mkdir()
    return output_dir

@pytest.fixture(autouse=True)
def reset_random_state():
    """Reset random state before each test."""
    np.random.seed(42)

@pytest.fixture(scope="session")
def large_dataset():
    """Load large dataset once per test session."""
    return load_reference_data()

# Platform-specific fixtures
@pytest.fixture(params=["Linux", "Darwin", "Windows"])
def platform_name(request, monkeypatch):
    """Parametrize tests across platforms."""
    monkeypatch.setattr("platform.system", lambda: request.param)
    return request.param
