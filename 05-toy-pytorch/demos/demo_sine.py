"""Sine function regression — trains MLP to approximate sin(x) on [0, 2π]."""
import math
import random
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from toypytorch.engine import Value
from toypytorch.nn import MLP
from toypytorch.optim import SGD, Adam
from toypytorch.loss import mse_loss
from toypytorch.data import Dataset, DataLoader


def main():
    random.seed(42)

    n = 100
    X = [[x / (n - 1) * 2 * math.pi] for x in range(n)]
    y = [math.sin(x[0]) for x in X]

    model_sgd = MLP(1, [8, 8, 1])
    optimizer_sgd = SGD(model_sgd.parameters(), lr=0.01)
    dataset = Dataset(X, y)
    loader = DataLoader(dataset, batch_size=n, shuffle=False)

    sgd_losses = []
    for epoch in range(500):
        for batch in loader:
            batch_x = [item[0] for item in batch]
            batch_y = [item[1] for item in batch]
            optimizer_sgd.zero_grad()
            y_pred = [model_sgd(x) for x in batch_x]
            loss = mse_loss(y_pred, [y.data for y in batch_y])
            loss.backward()
            optimizer_sgd.step()
            sgd_losses.append(loss.data)
        if epoch % 100 == 0:
            print(f"SGD Epoch {epoch:3d}: loss={loss.data:.6f}")

    random.seed(42)
    model_adam = MLP(1, [8, 8, 1])
    optimizer_adam = Adam(model_adam.parameters(), lr=0.01)

    adam_losses = []
    for epoch in range(500):
        for batch in loader:
            batch_x = [item[0] for item in batch]
            batch_y = [item[1] for item in batch]
            optimizer_adam.zero_grad()
            y_pred = [model_adam(x) for x in batch_x]
            loss = mse_loss(y_pred, [y.data for y in batch_y])
            loss.backward()
            optimizer_adam.step()
            adam_losses.append(loss.data)
        if epoch % 100 == 0:
            print(f"Adam Epoch {epoch:3d}: loss={loss.data:.6f}")

    fig, axes = plt.subplots(1, 3, figsize=(15, 4))

    axes[0].plot(sgd_losses, label='SGD', alpha=0.7)
    axes[0].plot(adam_losses, label='Adam', alpha=0.7)
    axes[0].set_xlabel('Iteration')
    axes[0].set_ylabel('MSE Loss')
    axes[0].set_title('SGD vs Adam Convergence')
    axes[0].legend()
    axes[0].set_yscale('log')

    x_plot = [Value(xi[0]) for xi in X]
    sgd_pred = [model_sgd([xv]).data for xv in x_plot]
    axes[1].plot([x[0] for x in X], y, 'k-', label='True sin(x)', linewidth=1.5)
    axes[1].plot([x[0] for x in X], sgd_pred, 'b--', label='SGD prediction', alpha=0.7)
    axes[1].set_title(f'SGD — Final MSE: {sgd_losses[-1]:.4f}')
    axes[1].legend()

    adam_pred = [model_adam([xv]).data for xv in x_plot]
    axes[2].plot([x[0] for x in X], y, 'k-', label='True sin(x)', linewidth=1.5)
    axes[2].plot([x[0] for x in X], adam_pred, 'r--', label='Adam prediction', alpha=0.7)
    axes[2].set_title(f'Adam — Final MSE: {adam_losses[-1]:.4f}')
    axes[2].legend()

    plt.tight_layout()
    plt.savefig('demo_sine.png', dpi=100)
    print("Plot saved to demo_sine.png")


if __name__ == '__main__':
    main()
