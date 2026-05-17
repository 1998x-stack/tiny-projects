# Toy PyTorch — Specification

> Based on: micrograd (Andrej Karpathy), tinygrad

## References

| Project | Stars | Language | Description |
|---------|-------|----------|-------------|
| [micrograd](https://github.com/karpathy/micrograd) | 15K | Python | Scalar autograd engine (~100 lines) + neural net (~50 lines) |
| [tinygrad](https://github.com/tinygrad/tinygrad) | 32K | Python | Full tensor autograd + IR compiler + GPU backend |

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│                  nn.Module                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐         │
│  │  Neuron  │  │  Layer   │  │   MLP    │         │
│  │(weights+ │  │(Neurons) │  │(Layers)  │         │
│  │ bias)    │  │          │  │          │         │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘         │
│       │             │             │                 │
├───────┴─────────────┴─────────────┴─────────────────┤
│                  Engine (autograd)                   │
│  ┌──────────────────────────────────────────────┐   │
│  │              Value class                      │   │
│  │  data: float     grad: float                  │   │
│  │  _prev: set[Value]    _op: str                │   │
│  │  _backward: callable                          │   │
│  │                                               │   │
│  │  Operations: +, *, **, relu, tanh, exp, log   │   │
│  │  backward(): topological sort → chain rule    │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

## Feature Specification

### 1. Core Autograd Engine — `Value` Class

**The micrograd approach** (single scalar values in a computation graph):

```python
class Value:
    def __init__(self, data, _children=(), _op=''):
        self.data = data          # float — the actual value
        self.grad = 0.0           # float — gradient accumulated here
        self._backward = lambda: None  # closure for local gradient
        self._prev = set(_children)    # children in computation graph
        self._op = _op                  # operation name (for debugging)

    def __add__(self, other):
        other = other if isinstance(other, Value) else Value(other)
        out = Value(self.data + other.data, (self, other), '+')
        def _backward():
            self.grad += out.grad   # d(out)/d(self) = 1
            other.grad += out.grad  # d(out)/d(other) = 1
        out._backward = _backward
        return out

    def __mul__(self, other):
        other = other if isinstance(other, Value) else Value(other)
        out = Value(self.data * other.data, (self, other), '*')
        def _backward():
            self.grad += other.data * out.grad   # d(a*b)/da = b
            other.grad += self.data * out.grad   # d(a*b)/db = a
        out._backward = _backward
        return out
```

**Key insight:** Each operation creates a new `Value` with a `_backward` closure that knows how to propagate gradients. This is the "define-by-run" pattern — the computation graph is built dynamically during forward pass.

**Supported Operations:**

| Operation | Forward | Backward (local gradient) |
|-----------|---------|---------------------------|
| `a + b` | `a.data + b.data` | `a.grad += out.grad` ; `b.grad += out.grad` |
| `a * b` | `a.data * b.data` | `a.grad += b.data * out.grad` ; `b.grad += a.data * out.grad` |
| `a ** n` | `a.data ** n` | `a.grad += n * a.data**(n-1) * out.grad` |
| `relu(a)` | `max(0, a.data)` | `a.grad += (out.data > 0) * out.grad` |
| `tanh(a)` | `tanh(a.data)` | `a.grad += (1 - out.data**2) * out.grad` |
| `exp(a)` | `exp(a.data)` | `a.grad += out.data * out.grad` |
| `log(a)` | `log(a.data)` | `a.grad += (1/a.data) * out.grad` |
| `a / b` | `a.data / b.data` | `a.grad += (1/b.data) * out.grad` ; `b.grad += (-a.data / b.data**2) * out.grad` |
| `-a` | `-a.data` | `a.grad += -out.grad` |

**The Backward Pass:**

```python
def backward(self):
    # 1. Topological sort of computation graph
    topo = []
    visited = set()
    def build_topo(v):
        if v not in visited:
            visited.add(v)
            for child in v._prev:
                build_topo(child)
            topo.append(v)
    build_topo(self)

    # 2. Seed gradient
    self.grad = 1.0

    # 3. Reverse topological order → apply chain rule
    for v in reversed(topo):
        v._backward()
```

### 2. Neural Network Module — `nn`

```python
class Module:
    def zero_grad(self):
        for p in self.parameters():
            p.grad = 0

    def parameters(self):
        return []

class Neuron(Module):
    def __init__(self, nin, nonlin=True):
        self.w = [Value(random.uniform(-1,1)) for _ in range(nin)]
        self.b = Value(0)
        self.nonlin = nonlin

    def __call__(self, x):
        # w·x + b
        act = sum((wi*xi for wi,xi in zip(self.w, x)), self.b)
        return act.relu() if self.nonlin else act

    def parameters(self):
        return self.w + [self.b]

class Layer(Module):
    def __init__(self, nin, nout, **kwargs):
        self.neurons = [Neuron(nin, **kwargs) for _ in range(nout)]

    def __call__(self, x):
        return [n(x) for n in self.neurons]

class MLP(Module):
    def __init__(self, nin, nouts):
        sz = [nin] + nouts
        self.layers = [Layer(sz[i], sz[i+1], nonlin=i!=len(nouts)-1)
                       for i in range(len(nouts))]

    def __call__(self, x):
        for layer in self.layers:
            x = layer(x)
        return x[0] if len(x) == 1 else x
```

### 3. Loss Functions

```python
def mse_loss(y_pred, y_true):
    """Mean Squared Error"""
    return sum((yp - yt)**2 for yp, yt in zip(y_pred, y_true))

def binary_cross_entropy(y_pred, y_true):
    """Binary Cross Entropy for classification"""
    eps = 1e-7
    return -sum(yt * (yp+eps).log() + (1-yt) * (1-yp+eps).log())
```

### 4. Optimizer — SGD

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
            p.grad = 0
```

### 5. Training Loop

```python
# Initialize
model = MLP(2, [16, 16, 1])  # 2 inputs, 2 hidden layers, 1 output
optimizer = SGD(model.parameters(), lr=0.01)

# Training
for epoch in range(100):
    # Forward pass
    y_pred = [model(x) for x in X_train]
    loss = mse_loss(y_pred, Y_train)

    # Backward pass
    optimizer.zero_grad()
    loss.backward()
    optimizer.step()

    print(f"Epoch {epoch}: loss={loss.data:.4f}")
```

### 6. Visualization (Graphviz)

```python
def draw_dot(root):
    """Render computation graph with Graphviz"""
    dot = graphviz.Digraph(format='svg', graph_attr={'rankdir':'LR'})
    # ... traverse graph, create nodes and edges ...
    return dot
```

## Future Extension: Tensor Engine

The scalar engine is an excellent proof of concept. For a more usable system, extend to multi-dimensional tensors:

```python
class Tensor:
    def __init__(self, data, requires_grad=False):
        self.data = np.array(data)  # N-dimensional array
        self.grad = np.zeros_like(self.data) if requires_grad else None
        # ... similar _backward pattern with broadcast rules ...

    def matmul(self, other):
        # np.dot with gradient: dC/dA = C.grad @ B.T
        pass
```

## Development Roadmap

### Phase 1: Scalar Autograd Engine (Week 1)
- Value class with +, *, pow
- `backward()` with topological sort
- Gradient checking against PyTorch (as reference)
- Unit tests for every operation

### Phase 2: More Operations (Week 2)
- ReLU, tanh, exp, log
- Subtraction, division (via add/mul composition)
- Activation function gradients

### Phase 3: Neural Network (Week 3)
- Neuron, Layer, MLP classes
- Parameters() traversal
- Visual verification with draw_dot()

### Phase 4: Training (Week 4)
- MSE and BCE loss functions
- SGD optimizer
- Training loop
- Demo: binary classification on moon dataset

### Phase 5: Tensor Extension (Week 5+)
- N-dimensional Tensor class
- Broadcasting rules
- Matrix multiplication with gradients
- Simple CNN demonstration

## Success Criteria

1. All 8 operations compute correct gradients (verified against PyTorch)
2. `backward()` handles diamond dependencies correctly
3. MLP with 2 hidden layers can classify non-linear data
4. Training loss decreases monotonically
5. Graphviz visualization shows correct DAG structure
6. Gradient checking passes for random computation graphs
7. Training a 2-16-16-1 MLP on moon dataset achieves >95% accuracy
