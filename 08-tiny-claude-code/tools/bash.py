import subprocess

from tools import BaseTool

MAX_OUTPUT = 8000
DEFAULT_TIMEOUT = 30


class BashTool(BaseTool):
    name = "bash"
    description = "Execute a shell command and return stdout+stderr."
    parameters = {
        "command": {"type": "string", "description": "The shell command to execute"},
    }

    def execute(self, command) -> str:
        try:
            result = subprocess.run(
                command,
                shell=True,
                capture_output=True,
                text=True,
                timeout=DEFAULT_TIMEOUT,
            )
            output = result.stdout + result.stderr
        except subprocess.TimeoutExpired:
            return f"ERROR: Command timed out after {DEFAULT_TIMEOUT}s"
        except Exception as e:
            return f"ERROR: {e}"
        if len(output) > MAX_OUTPUT:
            return output[:MAX_OUTPUT] + "\n[truncated]"
        return output if output else "(no output)"
