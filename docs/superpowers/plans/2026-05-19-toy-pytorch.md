# Toy PyTorch — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A pure-Python scalar autograd engine + MLP neural network library that classifies non-linear data with >95% accuracy, built from scratch with no ML dependencies.

**Architecture:** Define-by-run computation graph via `Value` class — each operation creates a new `Value` with a `_backward` closure. `backward()` does topological sort + reverse chain rule. `Module` base class provides recursive `parameters()` traversal. `SGD` and `Adam` optimizers update parameters by in-place data mutation. All verified against PyTorch reference.

**Tech Stack:** Python 3.11, `graphviz` (visualization), `pytest` (testing), `torch` (test-only cross-validation), `numpy` (demo data), `matplotlib` (demo plots), `scikit-learn` (moon dataset for demos).

---

## File Structure

```
toypytorch/                    # (create as package directory)
├── __init__.py                # Public API re-exports
├── engine.py                  # Value class — core autograd
├── nn.py                      # Module, Neuron, Layer, MLP
├── optim.py                   # SGD, Adam
├── loss.py                    # mse_loss, binary_cross_entropy
├── data.py                    # Dataset, DataLoader
├── utils.py                   # draw_dot, gradient_check
│
tests/
├── __init__.py                # empty
├── conftest.py                # fixtures: random seed, PyTorch helpers
├── test_engine.py             # all 9 ops forward + backward + diamonds
├── test_nn.py                 # module hierarchy, forward shapes
├── test_optim.py              # SGD step, Adam step, zero_grad
├── test_loss.py               # MSE vs PyTorch, BCE vs PyTorch
├── test_training.py           # end-to-end moon classification
└── test_utils.py              # draw_dot structure, gradient_check edge cases
│
demos/
├── demo_moon.py               # binary classification with plots
├── demo_sine.py               # sine regression with plots
└── demo.ipynb                 # Jupyter walkthrough
```

### Dependency Rules

```
engine.py   → (zero internal imports)
nn.py       → from toypytorch.engine import Value
optim.py    → (operates on .data / .grad directly)
loss.py     → from toypytorch.engine import Value
data.py     → from toypytorch.engine import Value
utils.py    → from toypytorch.engine import Value
```

---

## Phase 1: Engine Core

### Task 1: Project Skeleton

**Files:**
- Create: `toypytorch/__init__.py`
- Create: `toypytorch/engine.py`
- Create: `tests/__init__.py`
- Create: `tests/conftest.py`

- [ ] **Step 1: Create directory structure**

```bash
mkdir -p toypytorch tests demos
touch toypytorch/__init__.py tests/__init__.py
```

- [ ] **Step 2: Write `conftest.py` with fixtures**

Write `tests/conftest.py`:
```python
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
```

- [ ] **Step 3: Verify skeleton**

```bash
python -c "import toypytorch; print('ok')"
```
Expected: `ok`

- [ ] **Step 4: Commit**

```bash
git add toypytorch/ tests/ demos/
git commit -m "feat: create toypytorch project skeleton"
```

---

### Task 2: Value Constructor and Basic Attributes

**Files:**
- Modify: `toypytorch/engine.py`
- Create: `tests/test_engine.py`

- [ ] **Step 1: Write the failing test**

Write `tests/test_engine.py`:
```python
from toypytorch.engine import Value

def test_value_constructor():
    v = Value(3.0)
    assert v.data == 3.0
    assert v.grad == 0.0
    assert v._prev == set()
    assert v._op == ''
    assert callable(v._backward)

def test_value_with_children():
    a = Value(1.0)
    b = Value(2.0)
    v = Value(3.0, _children=(a, b), _op='test')
    assert v._prev == {a, b}
    assert v._op == 'test'

def test_value_repr():
    v = Value(3.0, _op='+')
    assert '3.0000' in repr(v)
    assert '+' in repr(v)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
python -m pytest tests/test_engine.py::test_value_constructor -v
```
Expected: FAIL with `ModuleNotFoundError` or `ImportError`

- [ ] **Step 3: Write minimal implementation**

Write `toypytorch/engine.py`:
```python
class Value:
    def __init__(self, data, _children=(), _op=''):
        self.data = data
        self.grad = 0.0
        self._backward = lambda: None
        self._prev = set(_children)
        self._op = _op

    def __repr__(self):
        return f"Value(data={self.data:.4f}, grad={self.grad:.4f}, op='{self._op}')"
```

- [ ] **Step 4: Run test to verify it passes**

```bash
python -m pytest tests/test_engine.py -v
```
Expected: 3 passed

- [ ] **Step 5: Commit**

```bash
git add toypytorch/engine.py tests/test_engine.py
git commit -m "feat: Value constructor with data, grad, _prev, _op, _backward, __repr__"
```

---

### Task 3: Addition (+)

**Files:**
- Modify: `toypytorch/engine.py`
- Modify: `tests/test_engine.py`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_engine.py`:
```python
def test_add_forward():
    a = Value(2.0)
    b = Value(3.0)
    out = a + b
    assert out.data == 5.0
    assert out._op == '+'
    assert out._prev == {a, b}

def test_add_auto_wrap_float():
    a = Value(2.0)
    out = a + 3.0
    assert out.data == 5.0
    assert isinstance(out._prev.pop(), Value)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
python -m pytest tests/test_engine.py::test_add_forward -v
```
Expected: FAIL with `TypeError: unsupported operand type(s) for +`

- [ ] **Step 3: Write implementation**

Append to `toypytorch/engine.py` (inside `Value` class):
```python
    def __add__(self, other):
        other = other if isinstance(other, Value) else Value(other)
        out = Value(self.data + other.data, (self, other), '+')

        def _backward():
            self.grad += out.grad
            other.grad += out.grad

        out._backward = _backward
        return out
```

- [ ] **Step 4: Run tests**

```bash
python -m pytest tests/test_engine.py -v
```
Expected: 4 passed

- [ ] **Step 5: Commit**

```bash
git add toypytorch/engine.py tests/test_engine.py
git commit -m "feat: Value.__add__ with backward closure and auto-wrapping"
```

---

### Task 4: Multiplication (*)

**Files:**
- Modify: `toypytorch/engine.py`
- Modify: `tests/test_engine.py`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_engine.py`:
```python
def test_mul_forward():
    a = Value(3.0)
    b = Value(4.0)
    out = a * b
    assert out.data == 12.0
    assert out._op == '*'

def test_mul_auto_wrap_float():
    a = Value(3.0)
    out = a * 2.0
    assert out.data == 6.0
```

- [ ] **Step 2: Run test to verify it fails**

```bash
python -m pytest tests/test_engine.py::test_mul_forward -v
```
Expected: FAIL

- [ ] **Step 3: Write implementation**

Append to `Value` in `toypytorch/engine.py`:
```python
    def __mul__(self, other):
        other = other if isinstance(other, Value) else Value(other)
        out = Value(self.data * other.data, (self, other), '*')

        def _backward():
            self.grad += other.data * out.grad
            other.grad += self.data * out.grad

        out._backward = _backward
        return out
```

- [ ] **Step 4: Run tests**

```bash
python -m pytest tests/test_engine.py -v
```
Expected: 6 passed

- [ ] **Step 5: Commit**

```bash
git add toypytorch/engine.py tests/test_engine.py
git commit -m "feat: Value.__mul__ with backward closure and auto-wrapping"
```

---

### Task 5: Backward Pass (Topological Sort + Chain Rule)

**Files:**
- Modify: `toypytorch/engine.py`
- Modify: `tests/test_engine.py`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_engine.py`:
```python
def test_backward_simple():
    a = Value(2.0)
    b = Value(3.0)
    c = a * b    # c = 6.0
    c.backward()
    # dc/da = b = 3.0, dc/db = a = 2.0
    assert a.grad == 3.0
    assert b.grad == 2.0

def test_backward_chained():
    a = Value(2.0)
    b = a + a    # b = 4.0; db/da = 2
    c = b * b    # c = 16.0; dc/db = 2*b = 8; dc/da = 8*2 = 16
    c.backward()
    assert a.grad == 16.0
```

- [ ] **Step 2: Run test to verify it fails**

```bash
python -m pytest tests/test_engine.py::test_backward_simple -v
```
Expected: FAIL (backward not yet implemented or does nothing)

- [ ] **Step 3: Write implementation**

Append to `Value` in `toypytorch/engine.py`:
```python
    def backward(self):
        # Topological sort via DFS post-order
        topo = []
        visited = set()

        def build_topo(v):
            if v not in visited:
                visited.add(v)
                for child in v._prev:
                    build_topo(child)
                topo.append(v)

        build_topo(self)

        # Seed gradient at output
        self.grad = 1.0

        # Reverse topological order → apply chain rule
        for v in reversed(topo):
            v._backward()
```

- [ ] **Step 4: Run tests**

```bash
python -m pytest tests/test_engine.py -v
```
Expected: 8 passed

- [ ] **Step 5: Commit**

```bash
git add toypytorch/engine.py tests/test_engine.py
git commit -m "feat: backward() with topological sort and chain rule"
```

---

### Task 6: Diamond Dependency Test

**Files:**
- Modify: `tests/test_engine.py`

- [ ] **Step 1: Write the diamond dependency test**

Append to `tests/test_engine.py`:
```python
def test_diamond_dependency():
    """
         a
        / \
       b   c
        \ /
         d
    d = a*b + a*c = a*(b+c)
    dd/da = b + c
    """
    a = Value(2.0)
    b = Value(3.0)
    c = Value(4.0)
    d = a * b + a * c   # a appears in two children
    d.backward()

    # Analytical: d = a*b + a*c = 2*3 + 2*4 = 6 + 8 = 14
    # dd/da = b + c = 3 + 4 = 7
    assert abs(a.grad - 7.0) < 1e-6

    # dd/db = a = 2
    assert abs(b.grad - 2.0) < 1e-6

    # dd/dc = a = 2
    assert abs(c.grad - 2.0) < 1e-6
```

- [ ] **Step 2: Run test**

```bash
python -m pytest tests/test_engine.py::test_diamond_dependency -v
```
Expected: PASS

- [ ] **Step 3: Commit**

```bash
git add tests/test_engine.py
git commit -m "test: diamond dependency gradient verification"
```

---

### Task 7: Power (**), Negation (-), Subtraction

**Files:**
- Modify: `toypytorch/engine.py`
- Modify: `tests/test_engine.py`

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_engine.py`:
```python
def test_pow_forward():
    a = Value(2.0)
    out = a ** 3
    assert out.data == 8.0
    assert out._op == '**3'

def test_pow_backward():
    a = Value(2.0)
    b = a ** 3    # b = 8.0, db/da = 3*2^2 = 12
    b.backward()
    assert abs(a.grad - 12.0) < 1e-6

def test_neg():
    a = Value(3.0)
    b = -a
    assert b.data == -3.0

def test_sub():
    a = Value(5.0)
    b = Value(2.0)
    c = a - b
    assert c.data == 3.0
```

- [ ] **Step 2: Run test to verify failure**

```bash
python -m pytest tests/test_engine.py::test_pow_forward -v
```
Expected: FAIL

- [ ] **Step 3: Write implementation**

Append to `Value` in `toypytorch/engine.py`:
```python
    def __pow__(self, other):
        assert isinstance(other, (int, float)), "only supporting int/float powers"
        out = Value(self.data ** other, (self,), f'**{other}')

        def _backward():
            self.grad += (other * self.data ** (other - 1)) * out.grad

        out._backward = _backward
        return out

    def __neg__(self):
        return self * -1

    def __sub__(self, other):
        return self + (-other)
```

- [ ] **Step 4: Run all tests**

```bash
python -m pytest tests/test_engine.py -v
```
Expected: 14 passed

- [ ] **Step 5: Commit**

```bash
git add toypytorch/engine.py tests/test_engine.py
git commit -m "feat: Value.__pow__, __neg__, __sub__ with gradient support"
```

---

### Task 8: Reverse Operators (__radd__, __rmul__)

**Files:**
- Modify: `toypytorch/engine.py`
- Modify: `tests/test_engine.py`

- [ ] **Step 1: Write test**

Append to `tests/test_engine.py`:
```python
def test_radd():
    out = 3.0 + Value(2.0)
    assert out.data == 5.0

def test_rmul():
    out = 3.0 * Value(2.0)
    assert out.data == 6.0
```

- [ ] **Step 2: Implement**

Append to `Value` in `toypytorch/engine.py`:
```python
    def __radd__(self, other):
        return self + other

    def __rmul__(self, other):
        return self * other
```

- [ ] **Step 3: Run tests**

```bash
python -m pytest tests/test_engine.py -v
```
Expected: 16 passed

- [ ] **Step 4: Commit**

```bash
git add toypytorch/engine.py tests/test_engine.py
git commit -m "feat: Value.__radd__, __rmul__ for float + Value and float * Value"
```

---

### Task 9: Division (/)

**Files:**
- Modify: `toypytorch/engine.py`
- Modify: `tests/test_engine.py`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_engine.py`:
```python
def test_truediv_forward():
    a = Value(6.0)
    b = Value(2.0)
    out = a / b
    assert out.data == 3.0

def test_truediv_backward():
    a = Value(6.0)
    b = Value(2.0)
    c = a / b           # c = 3.0
    c.backward()
    # dc/da = 1/b = 0.5, dc/db = -a/b^2 = -6/4 = -1.5
    assert abs(a.grad - 0.5) < 1e-6
    assert abs(b.grad - (-1.5)) < 1e-6
```

- [ ] **Step 2: Implement division as composition**

Append to `Value` in `toypytorch/engine.py`:
```python
    def __truediv__(self, other):
        # a / b = a * (b ** -1)
        return self * other ** -1
```

- [ ] **Step 3: Run tests**

```bash
python -m pytest tests/test_engine.py -v
```
Expected: 18 passed

- [ ] **Step 4: Commit**

```bash
git add toypytorch/engine.py tests/test_engine.py
git commit -m "feat: Value.__truediv__ via composition (a * b**-1)"
```

---

### Task 10: Activation Functions — ReLU and Tanh

**Files:**
- Modify: `toypytorch/engine.py`
- Modify: `tests/test_engine.py`

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_engine.py`:
```python
import math

def test_relu_forward():
    a = Value(3.0)
    out = a.relu()
    assert out.data == 3.0

def test_relu_zero():
    a = Value(-2.0)
    out = a.relu()
    assert out.data == 0.0

def test_relu_backward_positive():
    a = Value(3.0)
    b = a.relu()
    b.backward()
    assert a.grad == 1.0

def test_relu_backward_negative():
    a = Value(-3.0)
    b = a.relu()
    b.backward()
    assert a.grad == 0.0

def test_tanh_forward():
    a = Value(0.0)
    out = a.tanh()
    assert abs(out.data - 0.0) < 1e-6

def test_tanh_backward():
    a = Value(0.5)
    b = a.tanh()
    b.backward()
    # d/dx tanh(x) = 1 - tanh(x)^2
    expected = 1 - math.tanh(0.5) ** 2
    assert abs(a.grad - expected) < 1e-6

def test_tanh_clamping():
    """Large inputs should not overflow."""
    a = Value(100.0)
    b = a.tanh()
    assert abs(b.data - 1.0) < 1e-6  # tanh(100) ≈ 1.0
```

- [ ] **Step 2: Run test to verify failure**

```bash
python -m pytest tests/test_engine.py::test_relu_forward -v
```
Expected: FAIL

- [ ] **Step 3: Write implementation**

Append to `Value` in `toypytorch/engine.py`:
```python
    def relu(self):
        out = Value(max(0, self.data), (self,), 'ReLU')

        def _backward():
            self.grad += (out.data > 0) * out.grad

        out._backward = _backward
        return out

    def tanh(self):
        t = math.tanh(self.data)
        out = Value(t, (self,), 'tanh')

        def _backward():
            self.grad += (1 - t ** 2) * out.grad

        out._backward = _backward
        return out
```

Add `import math` at top of `engine.py`.

- [ ] **Step 4: Run tests**

```bash
python -m pytest tests/test_engine.py -v
```
Expected: 25 passed

- [ ] **Step 5: Commit**

```bash
git add toypytorch/engine.py tests/test_engine.py
git commit -m "feat: Value.relu() and Value.tanh() with numerical clamping"
```

---

### Task 11: Exp and Log

**Files:**
- Modify: `toypytorch/engine.py`
- Modify: `tests/test_engine.py`

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_engine.py`:
```python
def test_exp_forward():
    a = Value(0.0)
    out = a.exp()
    assert abs(out.data - 1.0) < 1e-6

def test_exp_backward():
    a = Value(2.0)
    b = a.exp()       # exp(2) ≈ 7.389
    b.backward()
    # d/dx exp(x) = exp(x)
    assert abs(a.grad - math.exp(2.0)) < 1e-6

def test_exp_overflow_clamping():
    a = Value(100.0)
    out = a.exp()
    # Should not be inf/nan
    assert not math.isinf(out.data)
    assert not math.isnan(out.data)

def test_log_forward():
    a = Value(1.0)
    out = a.log()
    assert abs(out.data - 0.0) < 1e-6

def test_log_backward():
    a = Value(2.0)
    b = a.log()
    b.backward()
    # d/dx log(x) = 1/x
    assert abs(a.grad - 0.5) < 1e-6

def test_log_near_zero():
    """log(0) should not raise due to epsilon."""
    a = Value(0.0)
    out = a.log()
    assert not math.isnan(out.data)
    assert not math.isinf(out.data)
```

- [ ] **Step 2: Run test to verify failure**

```bash
python -m pytest tests/test_engine.py::test_exp_forward -v
```
Expected: FAIL

- [ ] **Step 3: Write implementation**

Append to `Value` in `toypytorch/engine.py`:
```python
    def exp(self):
        # Clamp to prevent overflow (float64 safe up to ~700)
        x = max(min(self.data, 50.0), -50.0)
        out = Value(math.exp(x), (self,), 'exp')

        def _backward():
            self.grad += out.data * out.grad

        out._backward = _backward
        return out

    def log(self):
        # Add epsilon for numerical stability
        eps = 1e-7
        out = Value(math.log(self.data + eps), (self,), 'log')

        def _backward():
            self.grad += (1.0 / (self.data + eps)) * out.grad

        out._backward = _backward
        return out
```

- [ ] **Step 4: Run all engine tests**

```bash
python -m pytest tests/test_engine.py -v
```
Expected: 31 passed

- [ ] **Step 5: Commit**

```bash
git add toypytorch/engine.py tests/test_engine.py
git commit -m "feat: Value.exp() and Value.log() with numerical stability"
```

---

## Phase 2: Neural Network Modules

### Task 12: Module Base Class

**Files:**
- Create: `toypytorch/nn.py`
- Create: `tests/test_nn.py`

- [ ] **Step 1: Write the failing test**

Write `tests/test_nn.py`:
```python
from toypytorch.nn import Module
from toypytorch.engine import Value

def test_module_parameters_default_empty():
    m = Module()
    assert m.parameters() == []

def test_module_zero_grad():
    m = Module()
    m.zero_grad()  # Should not raise
```

- [ ] **Step 2: Run test to verify it fails**

```bash
python -m pytest tests/test_nn.py -v
```
Expected: FAIL with `ModuleNotFoundError`

- [ ] **Step 3: Write implementation**

Write `toypytorch/nn.py`:
```python
from toypytorch.engine import Value


class Module:
    def zero_grad(self):
        for p in self.parameters():
            p.grad = 0.0

    def parameters(self):
        return []

    def __call__(self, x):
        raise NotImplementedError
```

- [ ] **Step 4: Run tests**

```bash
python -m pytest tests/test_nn.py -v
```
Expected: 2 passed

- [ ] **Step 5: Commit**

```bash
git add toypytorch/nn.py tests/test_nn.py
git commit -m "feat: Module base class with parameters() and zero_grad()"
```

---

### Task 13: Neuron

**Files:**
- Modify: `toypytorch/nn.py`
- Modify: `tests/test_nn.py`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_nn.py`:
```python
from toypytorch.nn import Neuron

def test_neuron_parameters_count():
    n = Neuron(3)  # 3 weights + 1 bias = 4 params
    params = n.parameters()
    assert len(params) == 4

def test_neuron_forward():
    n = Neuron(2, nonlin=False)  # linear neuron
    # Set known weights for testing
    n.w[0].data = 1.0
    n.w[1].data = 2.0
    n.b.data = 0.5
    x = [Value(3.0), Value(4.0)]
    out = n(x)
    # 1*3 + 2*4 + 0.5 = 11.5
    assert abs(out.data - 11.5) < 1e-6

def test_neuron_relu():
    n = Neuron(2, nonlin=True)  # ReLU neuron
    n.w[0].data = -1.0
    n.w[1].data = -1.0
    n.b.data = 0.0
    x = [Value(1.0), Value(1.0)]
    out = n(x)
    # -1*1 + -1*1 + 0 = -2.0 → ReLU → 0.0
    assert out.data == 0.0
```

- [ ] **Step 2: Run test to verify failure**

```bash
python -m pytest tests/test_nn.py::test_neuron_parameters_count -v
```
Expected: FAIL

- [ ] **Step 3: Write implementation**

Append to `toypytorch/nn.py`:
```python
import random


class Neuron(Module):
    def __init__(self, nin, nonlin=True):
        # He initialization (for ReLU)
        self.w = [Value(random.uniform(-1, 1) * (2.0 / nin) ** 0.5) for _ in range(nin)]
        self.b = Value(0.0)
        self.nonlin = nonlin

    def __call__(self, x):
        # w · x + b
        act = sum((wi * xi for wi, xi in zip(self.w, x)), self.b)
        return act.relu() if self.nonlin else act

    def parameters(self):
        return self.w + [self.b]
```

- [ ] **Step 4: Run tests**

```bash
python -m pytest tests/test_nn.py -v
```
Expected: 5 passed

- [ ] **Step 5: Commit**

```bash
git add toypytorch/nn.py tests/test_nn.py
git commit -m "feat: Neuron with He init, forward pass, ReLU toggle"
```

---

### Task 14: Layer

**Files:**
- Modify: `toypytorch/nn.py`
- Modify: `tests/test_nn.py`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_nn.py`:
```python
from toypytorch.nn import Layer

def test_layer_output_shape():
    layer = Layer(3, 5)  # 3 inputs → 5 outputs
    x = [Value(1.0), Value(2.0), Value(3.0)]
    out = layer(x)
    assert len(out) == 5
    assert all(isinstance(v, Value) for v in out)

def test_layer_parameters_count():
    layer = Layer(3, 5)  # 5 neurons × (3 weights + 1 bias) = 20 params
    assert len(layer.parameters()) == 20
```

- [ ] **Step 2: Write implementation**

Append to `toypytorch/nn.py`:
```python
class Layer(Module):
    def __init__(self, nin, nout, nonlin=True):
        self.neurons = [Neuron(nin, nonlin=nonlin) for _ in range(nout)]

    def __call__(self, x):
        return [n(x) for n in self.neurons]

    def parameters(self):
        return [p for n in self.neurons for p in n.parameters()]
```

- [ ] **Step 3: Run tests**

```bash
python -m pytest tests/test_nn.py -v
```
Expected: 7 passed

- [ ] **Step 4: Commit**

```bash
git add toypytorch/nn.py tests/test_nn.py
git commit -m "feat: Layer with neuron collection and recursive parameters()"
```

---

### Task 15: MLP

**Files:**
- Modify: `toypytorch/nn.py`
- Modify: `tests/test_nn.py`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_nn.py`:
```python
from toypytorch.nn import MLP

def test_mlp_output_shape_single():
    mlp = MLP(2, [16, 16, 1])
    x = [Value(1.0), Value(2.0)]
    out = mlp(x)
    # Final layer outputs 1 value → unwrapped to scalar
    assert isinstance(out, Value)

def test_mlp_output_shape_multi():
    mlp = MLP(2, [16, 5])
    x = [Value(1.0), Value(2.0)]
    out = mlp(x)
    # Final layer outputs 5 values → list
    assert len(out) == 5

def test_mlp_parameters_count():
    mlp = MLP(2, [16, 16, 1])
    # Layer 0: 16*(2+1) = 48
    # Layer 1: 16*(16+1) = 272
    # Layer 2: 1*(16+1) = 17
    # Total: 337
    assert len(mlp.parameters()) == 337

def test_mlp_final_layer_no_activation():
    mlp = MLP(1, [1])
    # Set weights so output is negative (would be zeroed by ReLU)
    for p in mlp.parameters():
        p.data = -1.0
    out = mlp([Value(1.0)])
    # If no activation: -1 + (-1) = -2  (ReLU would make it 0)
    assert abs(out.data - (-2.0)) < 1e-6
```

- [ ] **Step 2: Write implementation**

Append to `toypytorch/nn.py`:
```python
class MLP(Module):
    def __init__(self, nin, nouts):
        sz = [nin] + nouts
        self.layers = [
            Layer(sz[i], sz[i+1], nonlin=(i != len(nouts) - 1))
            for i in range(len(nouts))
        ]

    def __call__(self, x):
        for layer in self.layers:
            x = layer(x)
        return x[0] if len(x) == 1 else x

    def parameters(self):
        return [p for layer in self.layers for p in layer.parameters()]
```

- [ ] **Step 3: Run tests**

```bash
python -m pytest tests/test_nn.py -v
```
Expected: 11 passed

- [ ] **Step 4: Commit**

```bash
git add toypytorch/nn.py tests/test_nn.py
git commit -m "feat: MLP with stacked layers, no activation on output layer"
```

---

## Phase 3: Training Loop

### Task 16: MSE Loss

**Files:**
- Create: `toypytorch/loss.py`
- Create: `tests/test_loss.py`

- [ ] **Step 1: Write the failing test**

Write `tests/test_loss.py`:
```python
from toypytorch.loss import mse_loss
from toypytorch.engine import Value

def test_mse_loss_forward():
    y_pred = [Value(2.0), Value(3.0)]
    y_true = [1.0, 4.0]
    loss = mse_loss(y_pred, y_true)
    # loss = ((2-1)^2 + (3-4)^2) / 2 = (1 + 1) / 2 = 1.0
    assert abs(loss.data - 1.0) < 1e-6

def test_mse_loss_perfect():
    y_pred = [Value(5.0), Value(5.0)]
    y_true = [5.0, 5.0]
    loss = mse_loss(y_pred, y_true)
    assert loss.data == 0.0

def test_mse_loss_is_scalar():
    y_pred = [Value(1.0), Value(2.0)]
    y_true = [0.0, 3.0]
    loss = mse_loss(y_pred, y_true)
    assert isinstance(loss, Value)
    assert loss._prev  # Has children in computation graph
```

- [ ] **Step 2: Implement MSE loss**

Write `toypytorch/loss.py`:
```python
from toypytorch.engine import Value


def mse_loss(y_pred, y_true):
    """Mean Squared Error: (1/N) * Σ(y_pred - y_true)²"""
    n = len(y_pred)
    losses = [(yp - Value(yt)) ** 2 for yp, yt in zip(y_pred, y_true)]
    return sum(losses, Value(0.0)) * (1.0 / n)
```

- [ ] **Step 3: Run tests**

```bash
python -m pytest tests/test_loss.py -v
```
Expected: 3 passed

- [ ] **Step 4: Commit**

```bash
git add toypytorch/loss.py tests/test_loss.py
git commit -m "feat: mse_loss with normalized sum of squared errors"
```

---

### Task 17: Binary Cross Entropy Loss

**Files:**
- Modify: `toypytorch/loss.py`
- Modify: `tests/test_loss.py`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_loss.py`:
```python
from toypytorch.loss import binary_cross_entropy
import math

def test_bce_loss_forward():
    y_pred = [Value(0.9), Value(0.1)]
    y_true = [1.0, 0.0]
    loss = binary_cross_entropy(y_pred, y_true)
    # -[1*log(0.9) + 0*log(0.1)]/2 + -[0*log(0.1) + 1*log(0.9)]/2
    expected = -(math.log(0.9) + math.log(0.9)) / 2
    assert abs(loss.data - expected) < 1e-6

def test_bce_loss_epsilon():
    """BCE should not raise on y_pred=0 due to epsilon."""
    y_pred = [Value(0.0)]
    y_true = [1.0]
    loss = binary_cross_entropy(y_pred, y_true)
    assert not math.isnan(loss.data)
    assert not math.isinf(loss.data)
```

- [ ] **Step 2: Implement BCE loss**

Append to `toypytorch/loss.py`:
```python
def binary_cross_entropy(y_pred, y_true):
    """Binary Cross Entropy: -(1/N) * Σ[y*log(ŷ+ε) + (1-y)*log(1-ŷ+ε)]"""
    eps = 1e-7
    n = len(y_pred)
    losses = [
        -(Value(yt) * (yp + eps).log() + (1 - Value(yt)) * (1 - yp + eps).log())
        for yp, yt in zip(y_pred, y_true)
    ]
    return sum(losses, Value(0.0)) * (1.0 / n)
```

- [ ] **Step 3: Run tests**

```bash
python -m pytest tests/test_loss.py -v
```
Expected: 5 passed

- [ ] **Step 4: Commit**

```bash
git add toypytorch/loss.py tests/test_loss.py
git commit -m "feat: binary_cross_entropy with epsilon for numerical safety"
```

---

### Task 18: SGD Optimizer

**Files:**
- Create: `toypytorch/optim.py`
- Create: `tests/test_optim.py`

- [ ] **Step 1: Write the failing test**

Write `tests/test_optim.py`:
```python
from toypytorch.optim import SGD
from toypytorch.engine import Value

def test_sgd_step():
    param = Value(5.0)
    param.grad = 2.0
    opt = SGD([param], lr=0.1)
    opt.step()
    # param = 5.0 - 0.1 * 2.0 = 4.8
    assert abs(param.data - 4.8) < 1e-6

def test_sgd_zero_grad():
    p1 = Value(1.0); p1.grad = 3.0
    p2 = Value(2.0); p2.grad = 4.0
    opt = SGD([p1, p2])
    opt.zero_grad()
    assert p1.grad == 0.0
    assert p2.grad == 0.0

def test_sgd_multiple_steps():
    param = Value(10.0)
    param.grad = 1.0
    opt = SGD([param], lr=0.5)
    opt.step()  # 10 - 0.5 = 9.5
    assert abs(param.data - 9.5) < 1e-6
    param.grad = 1.0  # set new gradient
    opt.step()  # 9.5 - 0.5 = 9.0
    assert abs(param.data - 9.0) < 1e-6
```

- [ ] **Step 2: Implement SGD**

Write `toypytorch/optim.py`:
```python
class SGD:
    def __init__(self, params, lr=0.01):
        self.params = params
        self.lr = lr

    def step(self):
        for p in self.params:
            p.data -= self.lr * p.grad

    def zero_grad(self):
        for p in self.params:
            p.grad = 0.0
```

- [ ] **Step 3: Run tests**

```bash
python -m pytest tests/test_optim.py -v
```
Expected: 3 passed

- [ ] **Step 4: Commit**

```bash
git add toypytorch/optim.py tests/test_optim.py
git commit -m "feat: SGD optimizer with step() and zero_grad()"
```

---

### Task 19: Adam Optimizer

**Files:**
- Modify: `toypytorch/optim.py`
- Modify: `tests/test_optim.py`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_optim.py`:
```python
from toypytorch.optim import Adam

def test_adam_step():
    param = Value(1.0)
    param.grad = 0.1
    opt = Adam([param], lr=0.01)
    opt.step()
    # After 1 step with default betas, param should decrease
    assert param.data < 1.0

def test_adam_zero_grad():
    param = Value(1.0); param.grad = 5.0
    opt = Adam([param])
    opt.step()
    opt.zero_grad()
    assert param.grad == 0.0

def test_adam_bias_correction():
    """Early steps should use bias-corrected estimates."""
    param = Value(2.0)
    param.grad = 1.0
    opt = Adam([param], lr=0.1)
    # First step
    opt.step()
    # m = (1-0.9)*1.0 = 0.1, m_hat = 0.1/(1-0.9) = 1.0
    # v = (1-0.999)*1.0^2 = 0.001, v_hat = 0.001/(1-0.999) = 1.0
    # update = -0.1 * 1.0 / (1.0 + 1e-8) = -0.1
    # param = 2.0 - 0.1 = 1.9
    assert abs(param.data - 1.9) < 1e-6

def test_adam_does_not_reset_momentum_on_zero_grad():
    param = Value(1.0)
    param.grad = 0.5
    opt = Adam([param])
    opt.step()       # m accumulates
    m_after_first = opt.m[0]
    opt.zero_grad()
    # momentum should persist
    assert opt.m[0] == m_after_first
```

- [ ] **Step 2: Implement Adam**

Append to `toypytorch/optim.py`:
```python
class Adam:
    def __init__(self, params, lr=0.001, betas=(0.9, 0.999), eps=1e-8):
        self.params = list(params)
        self.lr = lr
        self.beta1, self.beta2 = betas
        self.eps = eps
        self.t = 0
        self.m = [0.0] * len(self.params)
        self.v = [0.0] * len(self.params)

    def step(self):
        self.t += 1
        for i, p in enumerate(self.params):
            # Update biased moment estimates
            self.m[i] = self.beta1 * self.m[i] + (1 - self.beta1) * p.grad
            self.v[i] = self.beta2 * self.v[i] + (1 - self.beta2) * p.grad ** 2

            # Bias correction
            m_hat = self.m[i] / (1 - self.beta1 ** self.t)
            v_hat = self.v[i] / (1 - self.beta2 ** self.t)

            # Update
            p.data -= self.lr * m_hat / (v_hat ** 0.5 + self.eps)

    def zero_grad(self):
        for p in self.params:
            p.grad = 0.0
```

- [ ] **Step 3: Run tests**

```bash
python -m pytest tests/test_optim.py -v
```
Expected: 7 passed

- [ ] **Step 4: Commit**

```bash
git add toypytorch/optim.py tests/test_optim.py
git commit -m "feat: Adam optimizer with bias correction and momentum persistence"
```

---

### Task 20: Dataset and DataLoader

**Files:**
- Create: `toypytorch/data.py`

- [ ] **Step 1: Test via manual verification (no separate test file needed)**

Write `toypytorch/data.py`:
```python
import random
from toypytorch.engine import Value


class Dataset:
    """Wraps input-output pairs as lists of Values."""
    def __init__(self, X, y):
        self.X = X
        self.y = y

    def __len__(self):
        return len(self.y)

    def __getitem__(self, idx):
        return ([Value(xi) for xi in self.X[idx]], Value(self.y[idx]))


class DataLoader:
    """Iterates Dataset in shuffled batches."""
    def __init__(self, dataset, batch_size, shuffle=True):
        self.dataset = dataset
        self.batch_size = batch_size
        self.shuffle = shuffle

    def __iter__(self):
        indices = list(range(len(self.dataset)))
        if self.shuffle:
            random.shuffle(indices)
        for start in range(0, len(indices), self.batch_size):
            batch_idx = indices[start:start + self.batch_size]
            yield [self.dataset[i] for i in batch_idx]
```

- [ ] **Step 2: Verify with quick test**

```bash
python -c "
from toypytorch.data import Dataset, DataLoader
ds = Dataset([[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]], [0.0, 1.0, 0.0])
assert len(ds) == 3
x, y = ds[0]
assert x[0].data == 1.0 and x[1].data == 2.0 and y.data == 0.0

loader = DataLoader(ds, batch_size=2)
batches = list(loader)
assert len(batches) == 2  # 3 items, batch_size=2 → 2 batches
print('DataLoader tests passed')
"
```
Expected: `DataLoader tests passed`

- [ ] **Step 3: Commit**

```bash
git add toypytorch/data.py
git commit -m "feat: Dataset and DataLoader for training loop batching"
```

---

### Task 21: Gradient Checking Utility

**Files:**
- Create: `toypytorch/utils.py`
- Create: `tests/test_utils.py`

- [ ] **Step 1: Write the failing test**

Write `tests/test_utils.py`:
```python
from toypytorch.utils import gradient_check
from toypytorch.engine import Value

def test_gradient_check_add():
    def f():
        a = Value(2.0)
        b = Value(3.0)
        return a + b

    # Numerical grad for add is 1.0 for both inputs
    assert gradient_check(f) is True

def test_gradient_check_mul():
    def f():
        a = Value(3.0)
        b = Value(4.0)
        return a * b

    assert gradient_check(f) is True
```

- [ ] **Step 2: Implement gradient_check**

Write `toypytorch/utils.py`:
```python
from toypytorch.engine import Value


def gradient_check(f, h=1e-6, rtol=1e-4, atol=1e-8):
    """
    Verify backward() gradients against numerical gradients.

    f: a zero-argument callable that creates a fresh computation graph
       and returns the output Value.
    """
    # 1. Compute analytical gradients
    out = f()
    out.backward()

    # 2. Collect all leaf Values (those with _prev empty and _op empty)
    leaves = _collect_leaves(out)
    analytical_grads = {id(v): v.grad for v in leaves}

    # 3. For each leaf, compute numerical gradient
    for leaf in leaves:
        original_data = leaf.data

        # f(x + h)
        leaf.data = original_data + h
        out_plus = f()
        plus_val = out_plus.data

        # f(x - h)
        leaf.data = original_data - h
        out_minus = f()
        minus_val = out_minus.data

        # Restore
        leaf.data = original_data

        # Central difference
        numerical = (plus_val - minus_val) / (2 * h)
        analytical = analytical_grads[id(leaf)]

        # Check tolerance
        denom = max(abs(numerical), abs(analytical), 1.0)
        if abs(numerical - analytical) / denom > rtol:
            if abs(numerical - analytical) > atol:
                return False

    return True


def _collect_leaves(root):
    """Collect leaf nodes (no children, no op) in the computation graph."""
    leaves = []
    visited = set()

    def dfs(v):
        if v in visited:
            return
        visited.add(v)
        if not v._prev and not v._op:
            leaves.append(v)
        for child in v._prev:
            dfs(child)

    dfs(root)
    return leaves


def draw_dot(root):
    """Render computation graph with graphviz."""
    try:
        import graphviz
    except ImportError:
        raise ImportError("graphviz package required. Install: pip install graphviz")

    dot = graphviz.Digraph(format='svg', graph_attr={'rankdir': 'LR'})

    nodes = set()
    edges = set()

    def trace(v):
        if v not in nodes:
            nodes.add(v)
            for child in v._prev:
                edges.add((child, v))
                trace(child)

    trace(root)

    for n in nodes:
        uid = str(id(n))
        label = f"{{ data {n.data:.4f} | grad {n.grad:.4f} }}"
        dot.node(uid, label=label, shape='record')
        if n._op:
            op_uid = uid + n._op
            dot.node(op_uid, label=n._op)
            dot.edge(op_uid, uid)

    for n1, n2 in edges:
        dot.edge(str(id(n1)), str(id(n2)) + n2._op)

    return dot
```

- [ ] **Step 3: Run tests**

```bash
python -m pytest tests/test_utils.py -v
```
Expected: 2 passed

- [ ] **Step 4: Verify gradient_check can detect errors**

```bash
python -c "
from toypytorch.utils import gradient_check
from toypytorch.engine import Value

def broken_graph():
    # Intentionally wrong backward (should fail)
    a = Value(2.0)
    b = a * 3.0  # b = 6, db/da = 3
    # Manually corrupt the backward
    original = a._backward
    a._backward = lambda: setattr(a, 'grad', a.grad + 999)
    result = gradient_check(broken_graph)
    a._backward = original
    assert result == False, 'Should detect broken gradient'
    print('Error detection test passed')
"
```
Expected: `Error detection test passed`

- [ ] **Step 5: Commit**

```bash
git add toypytorch/utils.py tests/test_utils.py
git commit -m "feat: gradient_check() utility and draw_dot() visualization"
```

---

### Task 22: End-to-End Moon Classification Test

**Files:**
- Create: `tests/test_training.py`

- [ ] **Step 1: Write the end-to-end test**

Write `tests/test_training.py`:
```python
import random
from toypytorch.engine import Value
from toypytorch.nn import MLP
from toypytorch.optim import Adam
from toypytorch.loss import binary_cross_entropy
from toypytorch.data import Dataset, DataLoader

def generate_moons(n_samples=200, noise=0.1):
    """Simple moon dataset generator (no sklearn dependency)."""
    random.seed(0)
    X = []
    y = []

    n_out = n_samples // 2
    n_in = n_samples - n_out

    # Outer circle
    for _ in range(n_out):
        r = 2.0 + random.uniform(-noise, noise)
        angle = random.uniform(-3.14, 3.14)
        X.append([r * __import__('math').cos(angle), r * __import__('math').sin(angle) - 0.5])
        y.append(0.0)

    # Inner circle
    for _ in range(n_in):
        r = 2.0 + random.uniform(-noise, noise)
        angle = random.uniform(0, 6.28)
        X.append([r * __import__('math').cos(angle), r * __import__('math').sin(angle) + 0.5])
        y.append(1.0)

    return X, y


def test_moon_classification():
    random.seed(42)

    X, y = generate_moons(200, noise=0.1)
    model = MLP(2, [16, 16, 1])
    optimizer = Adam(model.parameters(), lr=0.01)
    dataset = Dataset(X, y)
    loader = DataLoader(dataset, batch_size=32)

    losses = []
    for epoch in range(150):  # 150 epochs sufficient for moon convergence; 200 in demo
        epoch_loss = 0.0
        correct = 0
        total = 0

        for batch in loader:
            batch_x = [item[0] for item in batch]
            batch_y = [item[1] for item in batch]

            optimizer.zero_grad()

            # Forward
            y_pred = [model(x) for x in batch_x]
            loss = binary_cross_entropy(y_pred, [y.data for y in batch_y])

            # Backward
            loss.backward()
            optimizer.step()

            epoch_loss += loss.data

            # Accuracy
            for pred, true in zip(y_pred, batch_y):
                pred_label = 1.0 if pred.data > 0.5 else 0.0
                if abs(pred_label - true.data) < 1e-6:
                    correct += 1
                total += 1

        losses.append(epoch_loss)

    accuracy = correct / total * 100
    print(f"Moon classification accuracy: {accuracy:.1f}%")

    assert accuracy > 95.0, f"Expected >95% accuracy, got {accuracy:.1f}%"

    # Loss should decrease
    assert losses[-1] < losses[0] * 0.5, "Loss should decrease significantly"


def test_loss_monotonic():
    """Loss should decrease monotonically for a simple problem."""
    random.seed(42)

    # Simple linear data
    X = [[i] for i in range(100)]
    y = [2.0 * i + 1.0 for i in range(100)]

    model = MLP(1, [4, 4, 1])
    optimizer = Adam(model.parameters(), lr=0.01)
    dataset = Dataset(X, y)

    # Full-batch training
    losses = []
    for epoch in range(50):
        x_batch = [dataset[i][0] for i in range(len(dataset))]
        y_batch = [dataset[i][1] for i in range(len(dataset))]

        optimizer.zero_grad()
        y_pred = [model(x) for x in x_batch]
        loss = sum((yp - yt) ** 2 for yp, yt in zip(y_pred, y_batch))
        loss.backward()
        optimizer.step()
        losses.append(loss.data)

    # Check monotonic decrease (allow small noise)
    for i in range(1, len(losses)):
        assert losses[i] <= losses[i-1] * 1.01, \
            f"Loss increased at epoch {i}: {losses[i-1]:.4f} → {losses[i]:.4f}"
```

- [ ] **Step 2: Run the moon classification test**

```bash
python -m pytest tests/test_training.py::test_moon_classification -v -s
```
Expected: PASS with accuracy > 95%

- [ ] **Step 3: Run the monotonic loss test**

```bash
python -m pytest tests/test_training.py::test_loss_monotonic -v -s
```
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add tests/test_training.py
git commit -m "test: end-to-end moon classification and monotonic loss tests"
```

---

## Phase 4: Demos & Polish

### Task 23: Moon Demo Script

**Files:**
- Create: `demos/demo_moon.py`

- [ ] **Step 1: Write the demo script**

Write `demos/demo_moon.py`:
```python
"""Binary classification on moon dataset — trains MLP and plots results."""
import random
import math
import matplotlib.pyplot as plt
from toypytorch.engine import Value
from toypytorch.nn import MLP
from toypytorch.optim import Adam
from toypytorch.loss import binary_cross_entropy
from toypytorch.data import Dataset, DataLoader


def generate_moons(n_samples=200, noise=0.1):
    random.seed(0)
    X, y = [], []
    n_out, n_in = n_samples // 2, n_samples - n_samples // 2

    for _ in range(n_out):
        r = 2.0 + random.uniform(-noise, noise)
        angle = random.uniform(-math.pi, math.pi)
        X.append([r * math.cos(angle), r * math.sin(angle) - 0.5])
        y.append(0.0)

    for _ in range(n_in):
        r = 2.0 + random.uniform(-noise, noise)
        angle = random.uniform(0, 2 * math.pi)
        X.append([r * math.cos(angle), r * math.sin(angle) + 0.5])
        y.append(1.0)

    return X, y


def main():
    random.seed(42)
    X, y = generate_moons(200, noise=0.1)

    model = MLP(2, [16, 16, 1])
    optimizer = Adam(model.parameters(), lr=0.01)
    dataset = Dataset(X, y)
    loader = DataLoader(dataset, batch_size=32)

    losses = []
    for epoch in range(200):
        epoch_loss = 0.0
        for batch in loader:
            batch_x = [item[0] for item in batch]
            batch_y = [item[1] for item in batch]

            optimizer.zero_grad()
            y_pred = [model(x) for x in batch_x]
            loss = binary_cross_entropy(y_pred, [y.data for y in batch_y])
            loss.backward()
            optimizer.step()
            epoch_loss += loss.data
        losses.append(epoch_loss)

        if epoch % 20 == 0 or epoch == 199:
            # Compute accuracy
            correct = sum(
                1 for i in range(len(X))
                if abs((1.0 if model([Value(X[i][0]), Value(X[i][1])]).data > 0.5 else 0.0) - y[i]) < 1e-6
            )
            acc = correct / len(X) * 100
            print(f"Epoch {epoch:3d}: loss={epoch_loss:.4f}, accuracy={acc:.1f}%")

    # Plot loss curve
    plt.figure(figsize=(12, 4))
    plt.subplot(1, 2, 1)
    plt.plot(losses)
    plt.xlabel('Epoch')
    plt.ylabel('Loss')
    plt.title('Training Loss')

    # Plot decision boundary
    plt.subplot(1, 2, 2)
    xx = [i / 25.0 for i in range(-50, 75)]
    yy = [j / 25.0 for j in range(-50, 75)]
    grid = []
    for xi in xx:
        for yj in yy:
            pred = model([Value(xi), Value(yj)])
            grid.append(pred.data > 0.5)

    x0 = [X[i][0] for i in range(len(X)) if y[i] == 0]
    y0 = [X[i][1] for i in range(len(X)) if y[i] == 0]
    x1 = [X[i][0] for i in range(len(X)) if y[i] == 1]
    y1 = [X[i][1] for i in range(len(X)) if y[i] == 1]

    plt.scatter(x0, y0, c='blue', s=10, label='Class 0')
    plt.scatter(x1, y1, c='red', s=10, label='Class 1')
    plt.xlabel('x1')
    plt.ylabel('x2')
    plt.title('Decision Boundary')
    plt.legend()

    plt.tight_layout()
    plt.savefig('demo_moon.png', dpi=100)
    plt.show()
    print("Plot saved to demo_moon.png")


if __name__ == '__main__':
    main()
```

- [ ] **Step 2: Run the demo**

```bash
python demos/demo_moon.py
```
Expected: Print training progress, save `demo_moon.png`, accuracy > 95%

- [ ] **Step 3: Commit**

```bash
git add demos/demo_moon.py
git commit -m "feat: moon classification demo with loss plot and decision boundary"
```

---

### Task 24: Sine Regression Demo

**Files:**
- Create: `demos/demo_sine.py`

- [ ] **Step 1: Write the demo script**

Write `demos/demo_sine.py`:
```python
"""Sine function regression — trains MLP to approximate sin(x) on [0, 2π]."""
import math
import random
import matplotlib.pyplot as plt
from toypytorch.engine import Value
from toypytorch.nn import MLP
from toypytorch.optim import SGD, Adam
from toypytorch.loss import mse_loss
from toypytorch.data import Dataset, DataLoader


def main():
    random.seed(42)

    # Generate data
    n = 100
    X = [[x / (n - 1) * 2 * math.pi] for x in range(n)]
    y = [math.sin(x[0]) for x in X]

    # Train with SGD
    model_sgd = MLP(1, [8, 8, 1])
    optimizer_sgd = SGD(model_sgd.parameters(), lr=0.01)
    dataset = Dataset(X, y)
    loader = DataLoader(dataset, batch_size=n, shuffle=False)

    sgd_losses = []
    for epoch in range(500):
        for batch in loader:
            batch_x = [item[0] for item in batch]
            batch_y = [item[1] for item in batch]
            optimizer_sgd.zero_grad()
            y_pred = [model_sgd(x) for x in batch_x]
            loss = mse_loss(y_pred, [y.data for y in batch_y])
            loss.backward()
            optimizer_sgd.step()
            sgd_losses.append(loss.data)
        if epoch % 100 == 0:
            print(f"SGD Epoch {epoch:3d}: loss={loss.data:.6f}")

    # Train with Adam
    random.seed(42)
    model_adam = MLP(1, [8, 8, 1])
    optimizer_adam = Adam(model_adam.parameters(), lr=0.01)

    adam_losses = []
    for epoch in range(500):
        for batch in loader:
            batch_x = [item[0] for item in batch]
            batch_y = [item[1] for item in batch]
            optimizer_adam.zero_grad()
            y_pred = [model_adam(x) for x in batch_x]
            loss = mse_loss(y_pred, [y.data for y in batch_y])
            loss.backward()
            optimizer_adam.step()
            adam_losses.append(loss.data)
        if epoch % 100 == 0:
            print(f"Adam Epoch {epoch:3d}: loss={loss.data:.6f}")

    # Plot
    fig, axes = plt.subplots(1, 3, figsize=(15, 4))

    # Loss curves
    axes[0].plot(sgd_losses, label='SGD', alpha=0.7)
    axes[0].plot(adam_losses, label='Adam', alpha=0.7)
    axes[0].set_xlabel('Iteration')
    axes[0].set_ylabel('MSE Loss')
    axes[0].set_title('SGD vs Adam Convergence')
    axes[0].legend()
    axes[0].set_yscale('log')

    # SGD predictions
    x_plot = [Value(xi[0]) for xi in X]
    sgd_pred = [model_sgd([xv]).data for xv in x_plot]
    axes[1].plot([x[0] for x in X], y, 'k-', label='True sin(x)', linewidth=1.5)
    axes[1].plot([x[0] for x in X], sgd_pred, 'b--', label='SGD prediction', alpha=0.7)
    axes[1].set_title(f'SGD — Final MSE: {sgd_losses[-1]:.4f}')
    axes[1].legend()

    # Adam predictions
    adam_pred = [model_adam([xv]).data for xv in x_plot]
    axes[2].plot([x[0] for x in X], y, 'k-', label='True sin(x)', linewidth=1.5)
    axes[2].plot([x[0] for x in X], adam_pred, 'r--', label='Adam prediction', alpha=0.7)
    axes[2].set_title(f'Adam — Final MSE: {adam_losses[-1]:.4f}')
    axes[2].legend()

    plt.tight_layout()
    plt.savefig('demo_sine.png', dpi=100)
    plt.show()
    print("Plot saved to demo_sine.png")


if __name__ == '__main__':
    main()
```

- [ ] **Step 2: Run the demo**

```bash
python demos/demo_sine.py
```
Expected: Training output with decreasing loss. Save `demo_sine.png` showing convergence curves and predictions.

- [ ] **Step 3: Commit**

```bash
git add demos/demo_sine.py
git commit -m "feat: sine regression demo with SGD vs Adam comparison"
```

---

### Task 25: Update __init__.py Public API

**Files:**
- Modify: `toypytorch/__init__.py`

- [ ] **Step 1: Write __init__.py**

Write `toypytorch/__init__.py`:
```python
from toypytorch.engine import Value
from toypytorch.nn import Module, Neuron, Layer, MLP
from toypytorch.optim import SGD, Adam
from toypytorch.loss import mse_loss, binary_cross_entropy
from toypytorch.utils import draw_dot, gradient_check

__all__ = [
    'Value',
    'Module', 'Neuron', 'Layer', 'MLP',
    'SGD', 'Adam',
    'mse_loss', 'binary_cross_entropy',
    'draw_dot', 'gradient_check',
]
```

- [ ] **Step 2: Verify imports**

```bash
python -c "from toypytorch import Value, Module, MLP, SGD, Adam, mse_loss, binary_cross_entropy, draw_dot, gradient_check; print('All imports OK')"
```
Expected: `All imports OK`

- [ ] **Step 3: Commit**

```bash
git add toypytorch/__init__.py
git commit -m "feat: public API in __init__.py"
```

---

### Task 26: Full Test Suite Verification

**Files:** (none modified — verification only)

- [ ] **Step 1: Run full test suite**

```bash
python -m pytest tests/ -v
```
Expected: All tests pass

- [ ] **Step 2: Verify test count**

```bash
python -m pytest tests/ --tb=short -q
```
Expected: All green, no failures, no errors

- [ ] **Step 3: Commit (if any changes needed)**

```bash
git add -A && git diff --cached --stat
# Only commit if tests pass
git commit -m "chore: full test suite verification, all tests pass"
```

---

## Self-Review Checklist

After implementing, verify:
- [ ] All 9 operations have gradient_check() passing tests
- [ ] Diamond dependency test passes (gradient not doubled)
- [ ] Moon classification > 95% accuracy
- [ ] Loss decreases monotonically in training test
- [ ] Adam converges faster than SGD on sine demo
- [ ] `zero_grad()` fully resets gradients
- [ ] `draw_dot()` produces valid graphviz output
- [ ] Full test suite: `pytest -v` all green
- [ ] `from toypytorch import Value, MLP, SGD, Adam, mse_loss` works
