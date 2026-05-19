# Toy PyTorch — Specification

> Based on: micrograd (Andrej Karpathy), tinygrad
> Build a scalar autograd engine + neural network library from scratch in pure Python. No numpy, no PyTorch — just Python floats and the chain rule.

## References

| Project | Stars | Language | Description |
|---------|-------|----------|-------------|
| [micrograd](https://github.com/karpathy/micrograd) | 15K | Python | Scalar autograd engine (~100 lines) + neural net (~50 lines) |
| [tinygrad](https://github.com/tinygrad/tinygrad) | 32K | Python | Full tensor autograd + IR compiler + GPU backend |
| [Karpathy's micrograd video](https://youtu.be/VMj-3S1tku0) | — | — | Line-by-line walkthrough of micrograd |

---

## Package Architecture

```
toypytorch/
├── __init__.py           # Public API: Value, Module, MLP, SGD, Adam, losses
├── engine.py             # Value class — scalar autograd engine
├── nn.py                 # Module, Neuron, Layer, MLP
├── optim.py              # SGD, Adam
├── loss.py               # mse_loss, binary_cross_entropy
├── data.py               # Dataset, DataLoader (minimal)
└── utils.py              # draw_dot(), gradient_check()

tests/
├── test_engine.py        # Every operation's forward + backward
├── test_nn.py            # Module.parameters(), forward pass shapes
├── test_optim.py         # SGD step, Adam step, zero_grad
├── test_loss.py          # MSE, BCE correctness vs PyTorch
├── test_training.py      # Full training loop on moon dataset
└── conftest.py           # PyTorch reference fixtures, random seed

demos/
├── demo_moon.py          # Binary classification on moon dataset
├── demo_sine.py          # Sine function regression
└── demo.ipynb            # Jupyter notebook walkthrough
```

### Dependency Rules

```
engine.py   → (zero internal imports)
nn.py       → engine.py
optim.py    → (operates on .data / .grad directly, no internal imports)
loss.py     → engine.py
data.py     → (standard library only)
utils.py    → engine.py
```

### Public API (`toypytorch/__init__.py`)

```python
from toypytorch.engine import Value
from toypytorch.nn import Module, Neuron, Layer, MLP
from toypytorch.optim import SGD, Adam
from toypytorch.loss import mse_loss, binary_cross_entropy
from toypytorch.utils import draw_dot, gradient_check
```

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     nn.Module                               │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐             │
│  │  Neuron  │    │  Layer   │    │   MLP    │             │
│  │(weights+ │───▶│(Neurons) │───▶│(Layers)  │             │
│  │ bias)    │    │          │    │          │             │
│  └────┬─────┘    └────┬─────┘    └────┬─────┘             │
│       │               │               │                    │
├───────┴───────────────┴───────────────┴────────────────────┤
│                  Engine (autograd)                          │
│  ┌────────────────────────────────────────────────────┐    │
│  │              Value class                            │    │
│  │  data: float       grad: float                      │    │
│  │  _prev: set[Value]    _op: str                      │    │
│  │  _backward: callable                                │    │
│  │                                                     │    │
│  │  Operations: +, *, **, relu, tanh, exp, log, /, -  │    │
│  │  backward(): topological sort → reverse chain rule  │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘

Data flow:
  Forward:  inputs → Neuron(w·x+b) → Layer → MLP → loss
  Backward: loss.backward() → topological sort → chain rule → .grad populated
  Update:   optimizer.step() → w -= lr * w.grad
```

---

## Feature Specification

### 1. Core Autograd Engine — `Value` Class

**Philosophy**: Each arithmetic operation creates a new `Value` with a `_backward` closure that knows how to propagate gradients. This is the "define-by-run" pattern — the computation graph is built dynamically during the forward pass.

#### 1.1 Constructor

```python
class Value:
    def __init__(self, data: float, _children: tuple = (), _op: str = ''):
        self.data = data              # float — the actual value
        self.grad = 0.0               # float — gradient accumulated here
        self._backward = lambda: None  # closure for local gradient propagation
        self._prev = set(_children)    # children in computation graph
        self._op = _op                 # operation name (for debugging/visualization)
```

**Design notes:**
- `grad` starts at `0.0`, never `None`. This simplifies the `backward()` pass.
- `_children` is a tuple in the signature (hashable for the set), converted to `set` for dedup.
- `_backward` is a closure — captures the specific `self` and `other` at operation time.

#### 1.2 Supported Operations

| Operation | Forward | Backward (applied via `_backward` closure) | Edge Cases |
|-----------|---------|----------------------------------------------|------------|
| `a + b` | `a.data + b.data` | `a.grad += out.grad` ; `b.grad += out.grad` | — |
| `a * b` | `a.data * b.data` | `a.grad += b.data * out.grad` ; `b.grad += a.data * out.grad` | — |
| `a ** n` | `a.data ** n` (n: int) | `a.grad += n * a.data**(n-1) * out.grad` | Integer n only (no fractional-power gradient) |
| `a.relu()` | `max(0, a.data)` | `a.grad += (out.data > 0) * out.grad` | Subgradient = 0 at x=0 (standard practice) |
| `a.tanh()` | `tanh(a.data)` | `a.grad += (1 - out.data**2) * out.grad` | Clamp input to ±20 to prevent overflow |
| `a.exp()` | `exp(a.data)` | `a.grad += out.data * out.grad` | Clamp input to ±20 (`exp(20) ≈ 4.8e8`) |
| `a.log()` | `log(a.data + 1e-7)` | `a.grad += (1/(a.data + 1e-7)) * out.grad` | Epsilon prevents log(0) and division by zero |
| `a / b` | `a.data / b.data` | `a.grad += (1/b.data) * out.grad` ; `b.grad += (-a.data/b.data**2) * out.grad` | Implement as `a * (b ** -1)` |
| `-a` | `-a.data` | `a.grad += -out.grad` | Implement as `a * -1` |

**Auto-wrapping**: All binary operators accept `float` as the other operand and auto-wrap it into `Value(other)`. Example: `Value(3.0) * 2.0` works.

**Composition over duplication**: `__truediv__`, `__neg__`, and `__sub__` are implemented by composing primitive operations, not by writing new backward closures. This reduces surface area for bugs.

#### 1.3 The Backward Pass

```python
def backward(self):
    # 1. Topological sort of computation graph (DFS post-order)
    topo = []
    visited = set()
    def build_topo(v):
        if v not in visited:
            visited.add(v)
            for child in v._prev:
                build_topo(child)
            topo.append(v)
    build_topo(self)

    # 2. Seed the gradient at the output
    self.grad = 1.0

    # 3. Reverse topological order → apply chain rule
    for v in reversed(topo):
        v._backward()
```

**Critical invariant**: `visited` set ensures each node is processed exactly once. Without this, diamond-shaped graphs (node used by multiple children) accumulate gradients incorrectly.

**Constraint**: `backward()` must be called on a scalar `Value` (typically the loss). Calling it on a vector output requires a gradient seed vector — out of scope for the scalar engine.

---

### 2. Neural Network Module — `nn`

#### 2.1 `Module` Base Class

```python
class Module:
    def zero_grad(self) -> None:
        """Set all parameter gradients to zero."""
        for p in self.parameters():
            p.grad = 0.0

    def parameters(self) -> list[Value]:
        """Return all trainable parameters (recursive)."""
        return []

    def __call__(self, x) -> Value | list[Value]:
        """Forward pass. Subclasses override."""
        raise NotImplementedError
```

**Contract**: `parameters()` must be recursive — when `MLP.parameters()` is called, it walks through `self.layers`, each `Layer` walks through `self.neurons`, each `Neuron` returns `self.w + [self.b]`.

#### 2.2 `Neuron`

```python
class Neuron(Module):
    def __init__(self, nin: int, nonlin: bool = True):
        # Weight initialization — He for ReLU, Xavier for tanh
        self.w = [Value(random.uniform(-1, 1) * (2.0 / nin) ** 0.5) for _ in range(nin)]
        self.b = Value(0.0)
        self.nonlin = nonlin

    def __call__(self, x: list[Value]) -> Value:
        # w₁x₁ + w₂x₂ + ... + b
        act = sum((wi * xi for wi, xi in zip(self.w, x)), self.b)
        return act.relu() if self.nonlin else act

    def parameters(self) -> list[Value]:
        return self.w + [self.b]
```

**Weight initialization**: Uses He initialization (`sqrt(2/nin)`) by default — appropriate for ReLU activations. For tanh networks, Xavier (`sqrt(1/nin)`) should be used. The gain factor is hardcoded for simplicity; switching activation functions requires changing the gain.

#### 2.3 `Layer`

```python
class Layer(Module):
    def __init__(self, nin: int, nout: int, nonlin: bool = True):
        self.neurons = [Neuron(nin, nonlin=nonlin) for _ in range(nout)]

    def __call__(self, x: list[Value]) -> list[Value]:
        return [n(x) for n in self.neurons]

    def parameters(self) -> list[Value]:
        return [p for n in self.neurons for p in n.parameters()]
```

**Output shape**: A `Layer(nin, nout)` takes a list of `nin` Values and returns a list of `nout` Values.

#### 2.4 `MLP` (Multi-Layer Perceptron)

```python
class MLP(Module):
    def __init__(self, nin: int, nouts: list[int]):
        """
        nin: number of input features
        nouts: list of layer sizes, e.g. [16, 16, 1] for 2 hidden layers + 1 output
        """
        sz = [nin] + nouts
        self.layers = [
            Layer(sz[i], sz[i+1], nonlin=(i != len(nouts) - 1))
            for i in range(len(nouts))
        ]

    def __call__(self, x: list[Value]) -> Value | list[Value]:
        for layer in self.layers:
            x = layer(x)
        return x[0] if len(x) == 1 else x

    def parameters(self) -> list[Value]:
        return [p for layer in self.layers for p in layer.parameters()]
```

**Design note**: The final layer has `nonlin=False` (no activation), which is standard for regression outputs and binary classification with BCE loss (the sigmoid is implicit in BCE).

---

### 3. Loss Functions

```python
def mse_loss(y_pred: list[Value], y_true: list[float]) -> Value:
    """Mean Squared Error: (1/N) * Σ(y_pred - y_true)²"""
    n = len(y_pred)
    losses = [(yp - Value(yt)) ** 2 for yp, yt in zip(y_pred, y_true)]
    return sum(losses, Value(0.0)) * (1.0 / n)

def binary_cross_entropy(y_pred: list[Value], y_true: list[float]) -> Value:
    """Binary Cross Entropy: -Σ[y*log(ŷ+ε) + (1-y)*log(1-ŷ+ε)]"""
    eps = 1e-7
    n = len(y_pred)
    losses = [
        -(Value(yt) * (yp + eps).log() + (1 - Value(yt)) * (1 - yp + eps).log())
        for yp, yt in zip(y_pred, y_true)
    ]
    return sum(losses, Value(0.0)) * (1.0 / n)
```

**BCE note**: The `+ eps` prevents `log(0)`. Loss functions return a scalar `Value` so `backward()` works without a gradient seed. MSE divides by `n` (normalized) so loss magnitudes are comparable across batch sizes.

---

### 4. Optimizers

#### 4.1 SGD (Stochastic Gradient Descent)

```python
class SGD:
    def __init__(self, params: list[Value], lr: float = 0.01):
        self.params = params
        self.lr = lr

    def step(self) -> None:
        """w = w - lr * w.grad"""
        for p in self.params:
            p.data -= self.lr * p.grad

    def zero_grad(self) -> None:
        """Reset all gradients to zero."""
        for p in self.params:
            p.grad = 0.0
```

#### 4.2 Adam (Adaptive Moment Estimation)

```python
class Adam:
    def __init__(self, params: list[Value], lr: float = 0.001,
                 betas: tuple[float, float] = (0.9, 0.999), eps: float = 1e-8):
        self.params = params
        self.lr = lr
        self.beta1, self.beta2 = betas
        self.eps = eps
        self.t = 0  # timestep counter
        self.m = [0.0] * len(params)   # first moment estimates
        self.v = [0.0] * len(params)   # second moment estimates

    def step(self) -> None:
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

    def zero_grad(self) -> None:
        for p in self.params:
            p.grad = 0.0
        # NOTE: m and v are NOT reset — they persist across steps
```

**Adam vs SGD**: Adam adapts learning rates per-parameter using running averages of gradients. It typically converges faster and requires less learning rate tuning. Both are included so the demos can compare their behavior.

---

### 5. Data Utilities

```python
class Dataset:
    """Wraps input-output pairs as lists of Values."""
    def __init__(self, X: list[list[float]], y: list[float]):
        self.X = X
        self.y = y

    def __len__(self) -> int:
        return len(self.y)

    def __getitem__(self, idx: int) -> tuple[list[Value], Value]:
        return ([Value(xi) for xi in self.X[idx]], Value(self.y[idx]))

class DataLoader:
    """Iterates Dataset in shuffled batches."""
    def __init__(self, dataset: Dataset, batch_size: int, shuffle: bool = True):
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

**Design note**: These are minimal wrappers — just enough to structure training code. No multiprocessing, no prefetching, no collation transforms. Each element is a `(list[Value], Value)` pair.

---

### 6. Training Loop

```python
# Initialize
model = MLP(2, [16, 16, 1])        # 2 inputs, 2 hidden layers, 1 output
optimizer = Adam(model.parameters(), lr=0.01)
dataset = Dataset(X_train, Y_train)
loader = DataLoader(dataset, batch_size=32)

# Training
for epoch in range(200):
    epoch_loss = 0.0
    for batch_x, batch_y in loader:
        # Forward pass (batched)
        y_pred = [model(x) for x in batch_x]
        loss = binary_cross_entropy(y_pred, [y.data for y in batch_y])

        # Backward pass
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()

        epoch_loss += loss.data

    print(f"Epoch {epoch}: loss={epoch_loss:.4f}")
```

**Critical ordering**:
1. `zero_grad()` — reset gradients from previous iteration
2. `loss.backward()` — compute gradients via chain rule
3. `optimizer.step()` — update parameters using gradients

Reversing step 1 and 2 causes gradient accumulation across epochs → exploding loss.

---

### 7. Visualization (Graphviz)

```python
def draw_dot(root: Value) -> graphviz.Digraph:
    """Render computation graph with Graphviz.
    - Rectangles: data values
    - Ovals: operation nodes
    - Edges: data flow (forward)
    """
    dot = graphviz.Digraph(format='svg', graph_attr={'rankdir': 'LR'})
    # Traverse graph, create nodes for Values and operations, draw edges
    return dot
```

Used for debugging: verify the computation graph structure matches expectations. Should handle diamond dependencies and show gradient flow.

---

## Testing Strategy

### Test Framework

- **Runner**: `pytest` with `-v` for verbose output
- **Reproducibility**: `random.seed(42)` in `conftest.py`
- **PyTorch dependency**: `torch` required in test environment for cross-validation (not for production code)

### Gradient Checking

```python
def gradient_check(op, *inputs, h: float = 1e-6, rtol: float = 1e-4, atol: float = 1e-8) -> bool:
    """
    Verifies that Value.backward() matches the numerical gradient.

    numerical_gradient = (f(x+h) - f(x-h)) / (2h)

    Uses relative tolerance rtol for large gradients,
    falls back to absolute tolerance atol for near-zero gradients.
    """
```

### Test Matrix (per operation)

| Test | What It Checks |
|---|---|
| `test_<op>_forward` | `out.data` matches manual computation |
| `test_<op>_backward_<input>` | Each input's gradient matches numerical gradient |
| `test_<op>_diamond` | Gradient correct when value is used by multiple children |
| `test_<op>_float_input` | Auto-wrapping of `float` literals works |

### PyTorch Cross-Validation Pattern

```python
def test_tanh_vs_pytorch():
    # Our engine
    x = Value(0.5); y = x.tanh(); y.backward()

    # PyTorch reference (float64 to match Python float precision)
    x_torch = torch.tensor(0.5, dtype=torch.float64, requires_grad=True)
    y_torch = torch.tanh(x_torch); y_torch.backward()

    assert abs(y.data - y_torch.item()) < 1e-6
    assert abs(x.grad - x_torch.grad.item()) < 1e-6
```

### Property-Based Tests

```python
def test_topological_order_includes_all_nodes(root):
    """Every Value reachable from root appears exactly once in topological sort."""

def test_zero_grad_resets_all_grads(model):
    """After backward() + zero_grad(), all p.grad == 0.0."""

def test_random_graph_gradients():
    """50 random computation graphs — all pass gradient_check()."""
```

### Test Coverage Targets

| Module | Target | Rationale |
|--------|--------|-----------|
| `engine.py` | 100% branch | Every operation, every backward closure path |
| `nn.py` | 100% function | Forward pass, parameter collection |
| `optim.py` | 100% | Step updates data correctly, zero_grad works |
| `loss.py` | 100% | Correct values + correct gradients |
| `data.py` | 80%+ | Simple wrappers — iteration, shuffling |

---

## Demo Specifications

### Demo 1: Binary Classification (Moon Dataset)

```
┌─────────────────────────────────────────────────────────┐
│ Goal: Train a 2-16-16-1 MLP to separate two interleaving │
│       half-moons (non-linearly separable data)           │
├─────────────────────────────────────────────────────────┤
│ Data:      sklearn.datasets.make_moons(n_samples=200,    │
│            noise=0.1)                                    │
│ Model:     MLP(2, [16, 16, 1])                          │
│ Loss:      binary_cross_entropy                          │
│ Optimizer: Adam(lr=0.01)                                 │
│ Epochs:    200                                           │
│ Batch:     32                                            │
├─────────────────────────────────────────────────────────┤
│ Expected outputs:                                        │
│  1. Final accuracy > 95% on training set                 │
│  2. Loss curve (matplotlib) — monotonic decrease         │
│  3. Decision boundary contour plot                       │
│  4. Computation graph (draw_dot) for first forward pass  │
└─────────────────────────────────────────────────────────┘
```

### Demo 2: Sine Function Regression

```
┌─────────────────────────────────────────────────────────┐
│ Goal: Train a 1-8-8-1 MLP to approximate sin(x)         │
│       on [0, 2π]                                        │
├─────────────────────────────────────────────────────────┤
│ Data:      x = np.linspace(0, 2π, 100)                  │
│            y = sin(x)                                    │
│ Model:     MLP(1, [8, 8, 1]) — no activation on output  │
│ Loss:      mse_loss                                      │
│ Optimizer: SGD(lr=0.01)                                  │
│ Epochs:    500                                           │
│ Batch:     full-batch (100)                             │
├─────────────────────────────────────────────────────────┤
│ Expected outputs:                                        │
│  1. Predicted curve overlaid on ground truth             │
│  2. Final MSE < 0.01                                     │
│  3. Side-by-side: SGD vs Adam convergence comparison     │
└─────────────────────────────────────────────────────────┘
```

---

## Development Roadmap

### Phase 1: Engine Core

**Deliverables**: `engine.py`, `utils.py`, `tests/test_engine.py`

| # | Task | Verification |
|---|---|---|
| 1.1 | `Value.__init__()` — data, grad, _prev, _op, _backward | Constructor creates valid Value |
| 1.2 | `Value.__add__()`, `Value.__mul__()` — with backward closures | gradient_check passes for both inputs |
| 1.3 | `backward()` — topological sort + chain rule | Diamond dependency test passes |
| 1.4 | `Value.__pow__()`, `Value.__neg__()`, `Value.__sub__()` | gradient_check passes; composability verified |
| 1.5 | `Value.__truediv__()` — composite via mul + pow(-1) | gradient_check passes |
| 1.6 | `Value.relu()`, `Value.tanh()` — with numerical clamping | gradient_check passes; tanh saturation handled |
| 1.7 | `Value.exp()`, `Value.log()` — with numerical clamping | gradient_check passes; no NaN for edge values |
| 1.8 | `gradient_check()` utility in `utils.py` | Correctly flags broken gradients |
| 1.9 | `draw_dot()` graphviz visualization | Renders correct DAG with edge counts |
| 1.10 | Write all engine tests | 100% branch coverage; all ops pass |

**Verification gate**: `pytest tests/test_engine.py -v` → all green. `draw_dot()` renders a 10-node graph correctly.

### Phase 2: Neural Network Modules

**Deliverables**: `nn.py`, `tests/test_nn.py`

| # | Task | Verification |
|---|---|---|
| 2.1 | `Module` base class — `parameters()`, `zero_grad()`, `__call__` | Subclass contract works |
| 2.2 | `Neuron` — weight init, forward pass, activation toggle | Correct output value for known inputs |
| 2.3 | `Layer` — collection of neurons | Output is list of correct length |
| 2.4 | `MLP` — stacked layers, final layer without activation | Forward pass for 2→16→16→1 produces scalar |
| 2.5 | `parameters()` recursive traversal | MLP(2,[16,16,1]) returns 2×16+16 + 16×16+16 + 16×1+1 params |
| 2.6 | Write NN tests | Forward shapes correct; parameters discoverable |

**Verification gate**: `pytest tests/test_nn.py -v` → all green. MLP.parameters() count matches manual calculation.

### Phase 3: Training Loop

**Deliverables**: `loss.py`, `optim.py`, `data.py`, `tests/test_*.py`

| # | Task | Verification |
|---|---|---|
| 3.1 | `mse_loss()` — correct value, correct gradient | vs PyTorch MSE: value & gradient match |
| 3.2 | `binary_cross_entropy()` — correct value, correct gradient | vs PyTorch BCE: value & gradient match |
| 3.3 | `SGD` — step() and zero_grad() | Parameter updates verified numerically |
| 3.4 | `Adam` — step() with bias correction, zero_grad() | vs PyTorch Adam: parameter updates match |
| 3.5 | `Dataset`, `DataLoader` | Correct batch shapes, shuffling works |
| 3.6 | Training loop integration (moon demo) | Model achieves >95% accuracy |
| 3.7 | Write optimizer + loss tests | All pass vs PyTorch reference |

**Verification gate**: `pytest tests/test_optim.py tests/test_loss.py tests/test_training.py -v` → all green. Moon classification >95%.

### Phase 4: Demos & Polish

**Deliverables**: `demos/demo_moon.py`, `demos/demo_sine.py`, `demos/demo.ipynb`

| # | Task | Verification |
|---|---|---|
| 4.1 | Moon demo — training + visualizations | Accuracy >95%, loss curve monotonic |
| 4.2 | Sine demo — training + visualizations | MSE < 0.01, curve overlay matches |
| 4.3 | Adam vs SGD comparison plot | Adam converges faster (lower loss at epoch 100) |
| 4.4 | Jupyter notebook walkthrough | All cells execute end-to-end without errors |
| 4.5 | Full test suite | `pytest -v` — all tests green |

**Verification gate**: Both demos produce expected plots. Notebook runs clean. Full test suite green.

### Phase 5: Tensor Engine (Future)

**Deliverables** (out of current scope): `tensor.py`, CNN demo on MNIST.

| # | Task |
|---|---|
| 5.1 | `Tensor` class — N-dimensional `np.ndarray` wrapper with autograd |
| 5.2 | Broadcasting rules (NumPy-compatible) |
| 5.3 | `matmul`, `reshape`, `sum`, `transpose` with gradient formulas |
| 5.4 | `Conv2d`, `MaxPool2d` layers |
| 5.5 | CNN demo: LeNet-5 on MNIST (>90% accuracy) |

---

## Success Criteria

| # | Criterion | Verifiable Test |
|---|-----------|-----------------|
| 1 | All 9 operations compute correct gradients | `gradient_check()` passes for +, \*, \*\*, relu, tanh, exp, log, /, - |
| 2 | `backward()` handles diamond dependencies | `test_diamond_dependency()` — node with 2 children, gradient not doubled |
| 3 | MLP classifies non-linear data | `test_moon_classification()` — 2-16-16-1 MLP, 200 epochs, >95% accuracy |
| 4 | Training loss decreases monotonically | `test_loss_monotonic()` — loss[i+1] ≤ loss[i] for all epochs |
| 5 | Graphviz shows correct DAG | `test_dag_structure()` — correct node count, edge count, op labels |
| 6 | Gradient checking for random graphs | `test_random_graph()` — 50 random DAGs, all pass gradient_check() |
| 7 | Adam converges faster than SGD | `test_adam_vs_sgd()` — same model/lr, Adam reaches lower loss in fewer epochs |
| 8 | `zero_grad()` fully resets gradients | `test_zero_grad()` — after backward() + zero_grad(), all p.grad == 0.0 |
| 9 | PyTorch cross-validation passes | All ops match PyTorch gradients to 1e-6 relative tolerance |
| 10 | Full test suite passes | `pytest -v` → 100% pass rate, no skipped tests |

---

## Out of Scope

These are explicitly NOT part of the current build:

- **GPU / CUDA backend** — scalar engine only; Python floats on CPU
- **JIT compilation / IR optimization** — no intermediate representation, no graph optimization
- **Distributed training / data parallelism** — single machine, single process
- **Model serialization** (`state_dict`, `load_state_dict`) — can be added later
- **Convolution layers, recurrent layers, batch norm, dropout** — MLP only
- **Learning rate scheduling** — fixed learning rate per optimizer
- **Early stopping / checkpointing** — manual epoch count only
- **Mixed precision (float16/bfloat16)** — float64 only (Python `float`)
- **ONNX / TorchScript export** — no model export

---

## Future Extension: Tensor Engine (Design Sketch)

When the scalar engine is complete, extend to N-dimensional tensors:

```python
class Tensor:
    def __init__(self, data: np.ndarray, requires_grad: bool = False):
        self.data = np.array(data)
        self.grad = np.zeros_like(data) if requires_grad else None
        # _backward closure pattern identical to Value

    def matmul(self, other: Tensor) -> Tensor:
        """C = A @ B.  dC/dA = C.grad @ B.T,  dC/dB = A.T @ C.grad"""

    def reshape(self, *shape) -> Tensor:
        """View. Gradient: reshape back."""

    def sum(self, axis=None) -> Tensor:
        """Reduce. Gradient: broadcast back."""
```

### Broadcasting Rules (NumPy-compatible)

| Operation | Shapes | Broadcast Result | Gradient Sum Axis |
|-----------|--------|------------------|-------------------|
| `(3, 4) + (4,)` | auto | `(3, 4)` | axis=1 on the broadcast input |
| `(3, 1) + (1, 4)` | auto | `(3, 4)` | axis=0 and axis=1 respectively |
| `(3, 4) + (3, 1)` | auto | `(3, 4)` | axis=1 |

**Design constraint**: Tensor engine builds on `Value`'s scalar autograd pattern — the computation graph structure and backward pass are conceptually identical, just operating on arrays instead of scalars.
