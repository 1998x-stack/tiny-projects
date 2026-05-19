import json
from datetime import datetime
from pathlib import Path


class SessionManager:
    def __init__(self, session_dir=".tiny-claude-sessions"):
        self.dir = Path(session_dir)
        self.dir.mkdir(exist_ok=True)

    def save(self, name, messages, todos):
        data = {
            "messages": messages,
            "todos": todos,
            "saved_at": datetime.now().isoformat(),
        }
        (self.dir / f"{name}.json").write_text(json.dumps(data, indent=2))

    def resume(self, name):
        path = self.dir / f"{name}.json"
        if not path.exists():
            raise FileNotFoundError(f"Session '{name}' not found")
        data = json.loads(path.read_text())
        return data["messages"], data.get("todos", [])

    def list_sessions(self):
        return sorted(p.stem for p in self.dir.glob("*.json"))
