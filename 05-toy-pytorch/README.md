# 🔥 Toy PyTorch

A pure-Python scalar autograd engine + neural network library built from scratch. No numpy, no PyTorch — just Python floats and the chain rule.

Based on [micrograd](https://github.com/karpathy/micrograd) by Andrej Karpathy.

```python
from toypytorch import Value, MLP, SGD, mse_loss

# Define a computation graph
a = Value(2.0)
b = Value(3.0)
c = a * b + a ** 2   # c = 10.0
c.backward()          # dc/da = 7.0, dc/db = 2.0
print(f"a.grad = {a.grad:.1f}")  # a.grad = 7.0
```

---

## Architecture

```
User Code
    │
    ▼
┌──────────────────────────────────────────┐
│  Value (scalar autograd graph node)       │
│  data: float    grad: float               │
│  _backward: closure    _prev: set[Value]  │
│                                           │
│   +  *  **  /  -  relu  tanh  sigmoid     │
│   exp  log                                │
│                                           │
│   backward(): topo sort → chain rule      │
└──────────────┬───────────────────────────┘
               │
    ┌──────────┴──────────┐
    ▼                     ▼
┌─────────┐         ┌──────────┐
│   nn    │         │  optim   │
│ Module  │         │ SGD(0.01)│
│ Neuron  │         │ Adam     │
│ Layer   │         └──────────┘
│ MLP     │              │
└────┬────┘              │
     │                   │
     ▼                   ▼
┌─────────┐         ┌──────────┐
│  loss   │         │  data    │
│ MSE     │         │ Dataset  │
│ BCE     │         │Loader    │
└─────────┘         └──────────┘
```

**Define-by-run**: the computation graph is built dynamically during the forward pass. Each operation creates a new `Value` node with a `_backward` closure that knows how to propagate gradients.

---

## Quick Start

```bash
# Run tests
python -m pytest tests/ -v

# Train an MLP on the moon dataset (classification)
python demos/demo_moon.py

# Train an MLP to approximate sin(x) (regression)
python demos/demo_sine.py
```

### Training a model — minimal example

```python
from toypytorch import Value, MLP, Adam, binary_cross_entropy

# 2-class moons data (200 samples, 2 features)
X = [[0.5, -0.2], [1.1, 0.8], ...]   # list[list[float]]
y = [0.0, 1.0, ...]                   # list[float]

model = MLP(2, [16, 16, 1])           # 2→16→16→1
optimizer = Adam(model.parameters(), lr=0.01)

for epoch in range(200):
    # Forward
    y_pred = [model(x).sigmoid() for x in X]

    # Loss
    loss = binary_cross_entropy(y_pred, y)

    # Backward + update
    optimizer.zero_grad()
    loss.backward()
    optimizer.step()
```

### Gradient Checking

```python
from toypytorch import Value, gradient_check

a = Value(2.0)
b = Value(3.0)

def f():
    return a * b + a.relu()   # arbitrary computation

assert gradient_check(f) is True  # analytical ≈ numerical
```

---

## API Reference

### `Value` — scalar autograd engine

| Operation | Example | Gradient |
|-----------|---------|----------|
| `a + b` | `Value(2) + 3` | `a.grad += out.grad` |
| `a * b` | `Value(3) * 4` | `a.grad += b.data * out.grad` |
| `a ** n` | `Value(2) ** 3` | `a.grad += n * aⁿ⁻¹ * out.grad` |
| `a / b` | `Value(6) / 2` | via `a * b⁻¹` |
| `-a` | `-Value(3)` | via `a * -1` |
| `a.relu()` | `Value(-2).relu()` | 0 at x≤0, 1 at x>0 |
| `a.tanh()` | `Value(0.5).tanh()` | `1 - tanh²(x)` |
| `a.sigmoid()` | `Value(0).sigmoid()` | `σ(x)(1 - σ(x))` |
| `a.exp()` | `Value(1).exp()` | `exp(x)` |
| `a.log()` | `Value(2).log()` | `1/x` |
| `a.backward()` | — | Topo sort + chain rule |

### `nn` — neural network layers

```python
model = MLP(nin=2, nouts=[16, 16, 1])    # 2→16→16→1
output = model([Value(1.0), Value(2.0)])  # forward pass
params = model.parameters()               # list[Value] — trainable weights
model.zero_grad()                         # reset all gradients
```

### `optim` — optimizers

```python
SGD(params, lr=0.01)       # Stochastic Gradient Descent
Adam(params, lr=0.001)     # Adam (β₁=0.9, β₂=0.999)
    .step()                # update params: w -= lr * gradient
    .zero_grad()           # reset gradients to zero
```

### `loss` — loss functions

```python
mse_loss(y_pred, y_true)            # Mean Squared Error
binary_cross_entropy(y_pred, y_true) # Binary Cross Entropy (+ε for stability)
```

### `data` — training utilities

```python
ds = Dataset(X, y)           # wraps data
loader = DataLoader(ds, 32)  # shuffled batches
```

### `utils` — debugging

```python
gradient_check(f)          # verify backward() against numerical gradient
draw_dot(root)             # render computation graph with graphviz
```

---

## Design Decisions

| Decision | Choice |
|----------|--------|
| Engine type | Scalar (one `float` per node), not tensor |
| Graph construction | Define-by-run (dynamic, eager) |
| Backward | Topological sort + reverse chain rule |
| Optimizers | SGD + Adam (with bias correction) |
| Weight init | He initialization (`√(2/nin)`) for ReLU |
| Numerical stability | exp clamp ±50, log ε=1e-7, tanh no clamp |
| Testing | 59 tests, `gradient_check()` + PyTorch cross-val (optional) |
| Dependencies | Zero ML dependencies in core. `graphviz`, `matplotlib` for viz. `torch` optional for test cross-val. |

---

## Project Structure

```
toypytorch/
├── __init__.py      # public API
├── engine.py        # Value — core autograd (128 lines)
├── nn.py            # Module, Neuron, Layer, MLP
├── optim.py         # SGD, Adam
├── loss.py          # mse_loss, binary_cross_entropy
├── data.py          # Dataset, DataLoader
└── utils.py         # gradient_check, draw_dot

tests/               # 59 tests (57 unit + 2 integration)
demos/               # moon classification, sine regression
spec.md              # full specification
gotchas.md           # common pitfalls
```

---

## Demos

### Moon Classification (95.5% accuracy)

```bash
python demos/demo_moon.py
```

Trains a 2-16-16-1 MLP to separate two interleaving half-moons. Outputs a loss curve and decision boundary plot.

### Sine Regression

```bash
python demos/demo_sine.py
```

Trains 1-8-8-1 MLPs with SGD and Adam to approximate sin(x). Outputs convergence comparison and prediction plots.

---

## License

MIT — built as a learning exercise. See [micrograd](https://github.com/karpathy/micrograd) for the original.
