import difflib
from pathlib import Path

from tools import BaseTool

MAX_OUTPUT = 8000


class ReadTool(BaseTool):
    name = "read"
    description = "Read a file from the local filesystem. Returns content with line numbers."
    parameters = {
        "file_path": {"type": "string", "description": "Absolute path to the file to read"},
    }

    def execute(self, file_path) -> str:
        try:
            content = Path(file_path).read_text()
        except Exception as e:
            return f"ERROR: {e}"
        lines = content.splitlines()
        numbered = "\n".join(f"{i + 1}\t{line}" for i, line in enumerate(lines))
        if len(numbered) > MAX_OUTPUT:
            return numbered[:MAX_OUTPUT] + "\n[truncated]"
        return numbered


class WriteTool(BaseTool):
    name = "write"
    description = "Create or overwrite a file. Creates parent directories if needed."
    parameters = {
        "file_path": {"type": "string", "description": "Absolute path to the file"},
        "content": {"type": "string", "description": "Content to write"},
    }

    def execute(self, file_path, content) -> str:
        try:
            p = Path(file_path)
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(content)
            return f"Wrote {len(content.encode())} bytes to {file_path}"
        except Exception as e:
            return f"ERROR: {e}"


class EditTool(BaseTool):
    name = "edit"
    description = "Replace old_string with new_string in a file. old_string must match exactly once."
    parameters = {
        "file_path": {"type": "string", "description": "Absolute path to file"},
        "old_string": {"type": "string", "description": "Exact text to find (must be unique)"},
        "new_string": {"type": "string", "description": "Replacement text"},
    }

    def execute(self, file_path, old_string, new_string) -> str:
        try:
            p = Path(file_path)
            content = p.read_text()
        except Exception as e:
            return f"ERROR: {e}"
        count = content.count(old_string)
        if count == 0:
            return f"ERROR: old_string not found in {file_path}"
        if count > 1:
            return f"ERROR: Found {count} matches. Provide more surrounding context to make the match unique."
        new_content = content.replace(old_string, new_string, 1)
        p.write_text(new_content)
        return "\n".join(difflib.unified_diff(
            old_string.splitlines(), new_string.splitlines(),
            fromfile=file_path, tofile=file_path, lineterm="",
        ))
