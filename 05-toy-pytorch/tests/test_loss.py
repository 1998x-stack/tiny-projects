import math
from toypytorch.loss import mse_loss, binary_cross_entropy
from toypytorch.engine import Value


def test_mse_loss_forward():
    y_pred = [Value(2.0), Value(3.0)]
    y_true = [1.0, 4.0]
    loss = mse_loss(y_pred, y_true)
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
    assert loss._prev


def test_bce_loss_forward():
    y_pred = [Value(0.9), Value(0.1)]
    y_true = [1.0, 0.0]
    loss = binary_cross_entropy(y_pred, y_true)
    expected = -(math.log(0.9) + math.log(0.9)) / 2
    assert abs(loss.data - expected) < 1e-6


def test_bce_loss_epsilon():
    y_pred = [Value(0.0)]
    y_true = [1.0]
    loss = binary_cross_entropy(y_pred, y_true)
    assert not math.isnan(loss.data)
    assert not math.isinf(loss.data)
