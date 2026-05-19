import math
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


def test_backward_simple():
    a = Value(2.0)
    b = Value(3.0)
    c = a * b
    c.backward()
    assert a.grad == 3.0
    assert b.grad == 2.0


def test_backward_chained():
    a = Value(2.0)
    b = a + a
    c = b * b
    c.backward()
    assert a.grad == 16.0


def test_diamond_dependency():
    a = Value(2.0)
    b = Value(3.0)
    c = Value(4.0)
    d = a * b + a * c
    d.backward()
    assert abs(a.grad - 7.0) < 1e-6
    assert abs(b.grad - 2.0) < 1e-6
    assert abs(c.grad - 2.0) < 1e-6


def test_pow_forward():
    a = Value(2.0)
    out = a ** 3
    assert out.data == 8.0
    assert out._op == '**3'


def test_pow_backward():
    a = Value(2.0)
    b = a ** 3
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


def test_radd():
    out = 3.0 + Value(2.0)
    assert out.data == 5.0


def test_rmul():
    out = 3.0 * Value(2.0)
    assert out.data == 6.0


def test_truediv_forward():
    a = Value(6.0)
    b = Value(2.0)
    out = a / b
    assert out.data == 3.0


def test_truediv_backward():
    a = Value(6.0)
    b = Value(2.0)
    c = a / b
    c.backward()
    assert abs(a.grad - 0.5) < 1e-6
    assert abs(b.grad - (-1.5)) < 1e-6


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
    expected = 1 - math.tanh(0.5) ** 2
    assert abs(a.grad - expected) < 1e-6


def test_tanh_clamping():
    a = Value(100.0)
    b = a.tanh()
    assert abs(b.data - 1.0) < 1e-6


def test_exp_forward():
    a = Value(0.0)
    out = a.exp()
    assert abs(out.data - 1.0) < 1e-6


def test_exp_backward():
    a = Value(2.0)
    b = a.exp()
    b.backward()
    assert abs(a.grad - math.exp(2.0)) < 1e-6


def test_exp_overflow_clamping():
    a = Value(100.0)
    out = a.exp()
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
    assert abs(a.grad - 0.5) < 1e-6


def test_log_near_zero():
    a = Value(0.0)
    out = a.log()
    assert not math.isnan(out.data)
    assert not math.isinf(out.data)
