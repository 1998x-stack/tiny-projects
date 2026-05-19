import random
import pytest


@pytest.fixture(autouse=True)
def set_seed():
    """Ensure reproducibility across test runs."""
    random.seed(42)


@pytest.fixture
def torch_available():
    """Skip PyTorch-dependent tests if torch not installed."""
    try:
        import torch
        return torch
    except ImportError:
        pytest.skip("PyTorch not installed")
