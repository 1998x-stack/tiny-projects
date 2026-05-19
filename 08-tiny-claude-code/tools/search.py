import re
import shutil
import subprocess
from pathlib import Path

from tools import BaseTool

MAX_GREP_RESULTS = 50
MAX_GLOB_RESULTS = 100


class GrepTool(BaseTool):
    name = "grep"
    description = "Search file contents for a regex pattern. Returns file:line: match format."
    parameters = {
        "pattern": {"type": "string", "description": "Regex pattern to search for"},
        "path": {"type": "string", "description": "Directory or file to search in"},
    }

    def execute(self, pattern, path) -> str:
        if shutil.which("rg"):
            return self._rg(pattern, path)
        return self._python_grep(pattern, path)

    def _rg(self, pattern, path):
        try:
            result = subprocess.run(
                ["rg", "--no-heading", "-n", "-m", str(MAX_GREP_RESULTS), pattern, path],
                capture_output=True, text=True, timeout=10,
            )
            output = result.stdout.strip()
            return output if output else "No matches found."
        except Exception as e:
            return f"ERROR: {e}"

    def _python_grep(self, pattern, path):
        matches = []
        p = Path(path)
        files = [p] if p.is_file() else p.rglob("*")
        try:
            regex = re.compile(pattern)
        except re.error as e:
            return f"ERROR: Invalid regex: {e}"
        for f in files:
            if not f.is_file():
                continue
            try:
                for i, line in enumerate(f.read_text().splitlines(), 1):
                    if regex.search(line):
                        matches.append(f"{f}:{i}: {line}")
                        if len(matches) >= MAX_GREP_RESULTS:
                            matches.append(f"[truncated at {MAX_GREP_RESULTS} results]")
                            return "\n".join(matches)
            except (UnicodeDecodeError, PermissionError):
                continue
        return "\n".join(matches) if matches else "No matches found."


class GlobTool(BaseTool):
    name = "glob"
    description = "Find files matching a glob pattern. Returns sorted file list."
    parameters = {
        "pattern": {"type": "string", "description": "Glob pattern (e.g., '**/*.py')"},
    }

    def execute(self, pattern) -> str:
        try:
            parts = str(pattern).replace("\\", "/").split("*", 1)
            if len(parts) == 2:
                base = Path(parts[0]) if parts[0] else Path(".")
                glob_part = "*" + parts[1]
            else:
                base = Path(".")
                glob_part = pattern
            files = sorted(str(f) for f in base.glob(glob_part) if f.is_file())
            if not files:
                return "No files matched."
            if len(files) > MAX_GLOB_RESULTS:
                files = files[:MAX_GLOB_RESULTS]
                files.append(f"[truncated at {MAX_GLOB_RESULTS} results]")
            return "\n".join(files)
        except Exception as e:
            return f"ERROR: {e}"
