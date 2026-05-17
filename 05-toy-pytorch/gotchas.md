# Toy PyTorch — Gotchas

## 1. Gradient Accumulation (Zero Grad)

**Problem:** Gradients accumulate across backward passes. If you don't zero them, they keep summing.

```python
# WRONG — gradients accumulate forever
for epoch in range(100):
    loss.backward()       # grad += dL/dw
    optimizer.step()      # w -= lr * grad  (grad keeps growing!)

# CORRECT
for epoch in range(100):
    optimizer.zero_grad()  # grad = 0
    loss.backward()
    optimizer.step()
```

**Symptom:** Loss explodes after a few epochs. Gradients are 100x larger than expected.

## 2. Topological Sort and Diamond Dependencies

**Problem:** In a DAG with diamond shapes (node used by multiple children), topological sort must visit each node once.

```
     a
    / \
   b   c
    \ /
     d
```

**Gotcha:** If you process `b` before `c` and don't deduplicate, `a` gets visited twice → `a.grad` doubled.

**Fix:** Use a `visited` set in `build_topo()`:
```python
def build_topo(v):
    if v not in visited:
        visited.add(v)
        for child in v._prev:
            build_topo(child)
        topo.append(v)
```

## 3. ReLU Gradient at Zero

**Problem:** ReLU is not differentiable at x=0. The subgradient can be anything in [0, 1].

**Standard approach:** Use 0 at exactly 0 (or any value — practically doesn't matter):
```python
# Either works in practice
def _backward():
    self.grad += (out.data > 0) * out.grad    # grad=0 at x=0
# OR
def _backward():
    self.grad += (out.data >= 0) * out.grad   # grad=1 at x=0
```

## 4. Numerical Stability: tanh and exp

**Problem:** `tanh(x)` for large x → 1.0 (saturation). Gradient = 1 - tanh²(x) ≈ 0 → vanishing gradient.

**Problem:** `exp(x)` for large x → overflow → inf → NaN gradients.

**Mitigation:**
```python
def exp(self):
    # Clamp to prevent overflow
    x = max(min(self.data, 20), -20)  # exp(20) ≈ 4.8e8, safe enough
    out = Value(math.exp(x), (self,), 'exp')
    ...
```

## 5. Division by Zero

**Problem:** `a / b` when b ≈ 0 → division by zero.

**Problem:** `log(a)` when a ≤ 0 → math domain error.

**Mitigation:** Add epsilon:
```python
def log(self):
    out = Value(math.log(self.data + 1e-7), (self,), 'log')
    # gradient is 1/x, also safe with epsilon
```

## 6. Circular References

**Problem:** `Value._prev` creates reference cycles (child → parent via `_prev`, parent → child via local variable). Python GC handles this, but it prevents `__del__` from being called.

**Not an issue for toy project** (Python's cycle detector handles it), but a real framework would need weakrefs or arena allocation.

## 7. Backward Pass on Non-Scalar

**Problem:** `backward()` only works correctly when called on a scalar. Calling it on a vector output requires a gradient seed.

**micrograd approach:** Always call on scalar (the loss):
```python
loss = mse_loss(y_pred, y_true)  # loss is a scalar Value
loss.backward()  # correct
```

**For Tensors:** Need `backward(gradient=...)` parameter to seed the vector gradient.

## 8. Float Precision in Gradient Checking

**Problem:** Comparing computed gradients against numerical gradients is sensitive to epsilon.

**Numerical gradient:**
```python
def numerical_grad(f, x, h=1e-6):
    return (f(x + h) - f(x - h)) / (2 * h)
```

**Tolerance:** Use relative error < 1e-4 for float32, or absolute error for very small gradients.

## 9. Comparison with PyTorch — Pitfalls

**When validating against PyTorch:**
- PyTorch uses float32 by default — micrograd uses Python float (float64)
- PyTorch accumulates gradients (same gotcha — must zero_grad)
- PyTorch's `backward()` doesn't zero gradients either
- Activation functions may differ slightly (tanh implementations vary)

## 10. Broadcasting (Tensor Extension)

**Problem:** In tensor operations, shapes must match or broadcast.

**Broadcasting rules (NumPy-style):**
1. Pad smaller shape with 1s on the left
2. Dimensions must be equal OR one must be 1
3. Gradient sums along broadcasted dimensions

**Gotcha:** `(3, 1) + (3,)` broadcasts to `(3, 3)` because `(3, 1)` pads to `(1, 3)` and `(3,)` pads to `(1, 3)`. But intuitive expectation might be `(3, 1)`.

## 11. Weight Initialization

**Problem:** All weights initialized to same value → symmetry → all neurons learn identical features.

**Solution:** Random initialization (Xavier/He depending on activation):
```python
# Xavier (for tanh):
w = Value(random.uniform(-1, 1) * sqrt(1/nin))

# He (for ReLU):
w = Value(random.uniform(-1, 1) * sqrt(2/nin))
```

## 12. Learning Rate Tuning

**Too high:** Loss oscillates or diverges.
**Too low:** Training takes forever.

**Simple heuristic:** Start at 0.01, try [0.001, 0.01, 0.1, 1.0]. Plot loss curve.
