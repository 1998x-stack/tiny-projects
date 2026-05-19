from toypytorch.optim import SGD, Adam
from toypytorch.engine import Value


def test_sgd_step():
    param = Value(5.0)
    param.grad = 2.0
    opt = SGD([param], lr=0.1)
    opt.step()
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
    opt.step()
    assert abs(param.data - 9.5) < 1e-6
    param.grad = 1.0
    opt.step()
    assert abs(param.data - 9.0) < 1e-6


def test_adam_step():
    param = Value(1.0)
    param.grad = 0.1
    opt = Adam([param], lr=0.01)
    opt.step()
    assert param.data < 1.0


def test_adam_zero_grad():
    param = Value(1.0); param.grad = 5.0
    opt = Adam([param])
    opt.step()
    opt.zero_grad()
    assert param.grad == 0.0


def test_adam_bias_correction():
    param = Value(2.0)
    param.grad = 1.0
    opt = Adam([param], lr=0.1)
    opt.step()
    assert abs(param.data - 1.9) < 1e-6


def test_adam_does_not_reset_momentum_on_zero_grad():
    param = Value(1.0)
    param.grad = 0.5
    opt = Adam([param])
    opt.step()
    m_after_first = opt.m[0]
    opt.zero_grad()
    assert opt.m[0] == m_after_first
