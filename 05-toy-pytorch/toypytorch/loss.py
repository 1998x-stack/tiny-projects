from toypytorch.engine import Value


def mse_loss(y_pred, y_true):
    """Mean Squared Error: (1/N) * Σ(y_pred - y_true)²"""
    n = len(y_pred)
    losses = [(yp - Value(yt)) ** 2 for yp, yt in zip(y_pred, y_true)]
    return sum(losses, Value(0.0)) * (1.0 / n)


def binary_cross_entropy(y_pred, y_true):
    """Binary Cross Entropy: -(1/N) * Σ[y*log(ŷ+ε) + (1-y)*log(1-ŷ+ε)]"""
    eps = 1e-7
    n = len(y_pred)
    losses = [
        -(Value(yt) * (yp + eps).log() + (Value(1.0) - Value(yt)) * (Value(1.0) - yp + eps).log())
        for yp, yt in zip(y_pred, y_true)
    ]
    return sum(losses, Value(0.0)) * (1.0 / n)
