from tools import BaseTool

STATUS_ICONS = {
    "pending": "⬜",
    "in_progress": "\U0001f504",
    "completed": "✅",
}


class TodoTool(BaseTool):
    name = "todo_write"
    description = "Update the task list. Replaces the entire list with the provided todos."
    parameters = {
        "todos": {
            "type": "array",
            "description": "List of todo items",
            "items": {
                "type": "object",
                "properties": {
                    "content": {"type": "string"},
                    "status": {"type": "string", "enum": ["pending", "in_progress", "completed"]},
                },
                "required": ["content", "status"],
            },
        },
    }

    def __init__(self):
        self.todos = []

    def execute(self, todos) -> str:
        self.todos = todos
        return self._format()

    def _format(self) -> str:
        if not self.todos:
            return "(no tasks)"
        lines = []
        for todo in self.todos:
            icon = STATUS_ICONS.get(todo["status"], "?")
            lines.append(f"{icon} {todo['content']}")
        return "\n".join(lines)
