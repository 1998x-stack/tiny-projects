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
