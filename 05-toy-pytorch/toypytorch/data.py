import random
from toypytorch.engine import Value


class Dataset:
    """Wraps input-output pairs as lists of Values."""
    def __init__(self, X, y):
        self.X = X
        self.y = y

    def __len__(self):
        return len(self.y)

    def __getitem__(self, idx):
        return ([Value(xi) for xi in self.X[idx]], Value(self.y[idx]))


class DataLoader:
    """Iterates Dataset in shuffled batches."""
    def __init__(self, dataset, batch_size, shuffle=True):
        self.dataset = dataset
        self.batch_size = batch_size
        self.shuffle = shuffle

    def __iter__(self):
        indices = list(range(len(self.dataset)))
        if self.shuffle:
            random.shuffle(indices)
        for start in range(0, len(indices), self.batch_size):
            batch_idx = indices[start:start + self.batch_size]
            yield [self.dataset[i] for i in batch_idx]
