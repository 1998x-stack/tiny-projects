from toypytorch.utils import gradient_check
from toypytorch.engine import Value


def test_gradient_check_add():
    a = Value(2.0)
    b = Value(3.0)

    def f():
        return a + b

    assert gradient_check(f) is True


def test_gradient_check_mul():
    a = Value(3.0)
    b = Value(4.0)

    def f():
        return a * b

    assert gradient_check(f) is True


def test_gradient_check_detects_error():
    a = Value(2.0)

    def broken_graph():
        b = a * 3.0
        return b

    original_backward = a._backward
    a._backward = lambda: setattr(a, 'grad', a.grad + 999)
    result = gradient_check(broken_graph)
    a._backward = original_backward
    assert result is False
