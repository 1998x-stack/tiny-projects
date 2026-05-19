"""Minimal autograd example."""
import sys
sys.path.insert(0, '.')
from toypytorch import Value

a = Value(2.0)
b = Value(3.0)
c = a * b + a ** 2       # c = 6 + 4 = 10.0

print(f"Forward:  f(2,3) = {c.data}")

c.backward()
print(f"Backward: df/da  = {a.grad}")   # b + 2a = 7
print(f"Backward: df/db  = {b.grad}")   # a = 2
