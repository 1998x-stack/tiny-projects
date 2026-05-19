"""Binary classification on moon dataset — trains MLP and plots results."""
import random
import math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from toypytorch.engine import Value
from toypytorch.nn import MLP
from toypytorch.optim import Adam
from toypytorch.loss import binary_cross_entropy
from toypytorch.data import Dataset, DataLoader


def generate_moons(n_samples=200, noise=0.1):
    random.seed(0)
    X, y = [], []
    n_out, n_in = n_samples // 2, n_samples - n_samples // 2

    for _ in range(n_out):
        r = 2.0 + random.uniform(-noise, noise)
        angle = random.uniform(-math.pi, math.pi)
        X.append([r * math.cos(angle), r * math.sin(angle) - 0.5])
        y.append(0.0)

    for _ in range(n_in):
        r = 2.0 + random.uniform(-noise, noise)
        angle = random.uniform(0, 2 * math.pi)
        X.append([r * math.cos(angle), r * math.sin(angle) + 0.5])
        y.append(1.0)

    return X, y


def main():
    random.seed(42)
    X, y = generate_moons(200, noise=0.1)

    model = MLP(2, [16, 16, 1])
    optimizer = Adam(model.parameters(), lr=0.01)
    dataset = Dataset(X, y)
    loader = DataLoader(dataset, batch_size=32)

    losses = []
    for epoch in range(200):
        epoch_loss = 0.0
        for batch in loader:
            batch_x = [item[0] for item in batch]
            batch_y = [item[1] for item in batch]

            optimizer.zero_grad()
            y_pred = [model(x).sigmoid() for x in batch_x]
            loss = binary_cross_entropy(y_pred, [y.data for y in batch_y])
            loss.backward()
            optimizer.step()
            epoch_loss += loss.data
        losses.append(epoch_loss)

        if epoch % 20 == 0 or epoch == 199:
            correct = sum(
                1 for i in range(len(X))
                if abs((1.0 if model([Value(X[i][0]), Value(X[i][1])]).sigmoid().data > 0.5 else 0.0) - y[i]) < 1e-6
            )
            acc = correct / len(X) * 100
            print(f"Epoch {epoch:3d}: loss={epoch_loss:.4f}, accuracy={acc:.1f}%")

    plt.figure(figsize=(12, 4))
    plt.subplot(1, 2, 1)
    plt.plot(losses)
    plt.xlabel('Epoch')
    plt.ylabel('Loss')
    plt.title('Training Loss')

    plt.subplot(1, 2, 2)
    x0 = [X[i][0] for i in range(len(X)) if y[i] == 0]
    y0 = [X[i][1] for i in range(len(X)) if y[i] == 0]
    x1 = [X[i][0] for i in range(len(X)) if y[i] == 1]
    y1 = [X[i][1] for i in range(len(X)) if y[i] == 1]

    plt.scatter(x0, y0, c='blue', s=10, label='Class 0')
    plt.scatter(x1, y1, c='red', s=10, label='Class 1')
    plt.xlabel('x1')
    plt.ylabel('x2')
    plt.title('Decision Boundary')
    plt.legend()

    plt.tight_layout()
    plt.savefig('demo_moon.png', dpi=100)
    print("Plot saved to demo_moon.png")


if __name__ == '__main__':
    main()
