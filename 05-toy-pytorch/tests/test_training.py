import random
import math
from toypytorch.engine import Value
from toypytorch.nn import MLP
from toypytorch.optim import Adam
from toypytorch.loss import binary_cross_entropy
from toypytorch.data import Dataset, DataLoader


def generate_moons(n_samples=200, noise=0.1):
    random.seed(0)
    X = []
    y = []

    n_out = n_samples // 2
    n_in = n_samples - n_out

    for _ in range(n_out):
        r = 2.0 + random.uniform(-noise, noise)
        angle = random.uniform(-3.14, 3.14)
        X.append([r * math.cos(angle), r * math.sin(angle) - 0.5])
        y.append(0.0)

    for _ in range(n_in):
        r = 2.0 + random.uniform(-noise, noise)
        angle = random.uniform(0, 6.28)
        X.append([r * math.cos(angle), r * math.sin(angle) + 0.5])
        y.append(1.0)

    return X, y


def test_moon_classification():
    random.seed(42)

    X, y = generate_moons(200, noise=0.1)
    model = MLP(2, [16, 16, 1])
    optimizer = Adam(model.parameters(), lr=0.01)
    dataset = Dataset(X, y)
    loader = DataLoader(dataset, batch_size=32)

    losses = []
    for epoch in range(150):
        epoch_loss = 0.0
        correct = 0
        total = 0

        for batch in loader:
            batch_x = [item[0] for item in batch]
            batch_y = [item[1] for item in batch]

            optimizer.zero_grad()

            y_pred = [model(x).sigmoid() for x in batch_x]
            loss = binary_cross_entropy(y_pred, [y.data for y in batch_y])

            loss.backward()
            optimizer.step()

            epoch_loss += loss.data

            for pred, true in zip(y_pred, batch_y):
                pred_label = 1.0 if pred.data > 0.5 else 0.0
                if abs(pred_label - true.data) < 1e-6:
                    correct += 1
                total += 1

        losses.append(epoch_loss)

    accuracy = correct / total * 100
    print(f"Moon classification accuracy: {accuracy:.1f}%")

    assert accuracy > 95.0, f"Expected >95% accuracy, got {accuracy:.1f}%"
    assert losses[-1] < losses[0] * 0.5, "Loss should decrease significantly"


def test_loss_monotonic():
    random.seed(42)

    X = [[i] for i in range(100)]
    y = [2.0 * i + 1.0 for i in range(100)]

    model = MLP(1, [4, 4, 1])
    optimizer = Adam(model.parameters(), lr=0.01)
    dataset = Dataset(X, y)

    losses = []
    for epoch in range(50):
        x_batch = [dataset[i][0] for i in range(len(dataset))]
        y_batch = [dataset[i][1] for i in range(len(dataset))]

        optimizer.zero_grad()
        y_pred = [model(x) for x in x_batch]
        loss = sum((yp - yt) ** 2 for yp, yt in zip(y_pred, y_batch))
        loss.backward()
        optimizer.step()
        losses.append(loss.data)

    for i in range(1, len(losses)):
        assert losses[i] <= losses[i-1] * 1.01, \
            f"Loss increased at epoch {i}: {losses[i-1]:.4f} -> {losses[i]:.4f}"
