import os
from types import SimpleNamespace


def load_config():
    return SimpleNamespace(
        model=os.environ.get("MODEL", "anthropic/claude-sonnet-4-20250514"),
        max_tokens=int(os.environ.get("MAX_TOKENS", "100000")),
    )
