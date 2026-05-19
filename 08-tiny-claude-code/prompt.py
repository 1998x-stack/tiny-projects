from pathlib import Path

BASE_PROMPT = """You are Tiny Claude Code, an AI coding assistant.
You help users with software engineering tasks using the tools available to you.
Always read files before editing. Use todo_write to plan multi-step tasks."""


def build_system_prompt(cwd=None, todos=None, skills=None):
    parts = [BASE_PROMPT]
    if cwd:
        parts.append(f"Working directory: {cwd}")
    if todos:
        parts.append(f"Current tasks:\n{_format_todos(todos)}")
    if skills:
        for skill in skills:
            parts.append(f"--- Skill: {skill['name']} ---\n{skill['content']}")
    return "\n\n".join(parts)


def _format_todos(todos):
    icons = {"pending": "⬜", "in_progress": "\U0001f504", "completed": "✅"}
    lines = []
    for t in todos:
        icon = icons.get(t["status"], "?")
        lines.append(f"{icon} {t['content']}")
    return "\n".join(lines)


def load_skill(name, skills_dir=".tiny-claude-skills"):
    path = Path(skills_dir) / f"{name}.md"
    if not path.exists():
        return None
    return {"name": name, "content": path.read_text()}
