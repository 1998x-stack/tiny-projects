import json

from rich.console import Console
from rich.markdown import Markdown
from rich.panel import Panel
from rich.syntax import Syntax

console = Console()


def render_chunk(chunk):
    if chunk["type"] == "text":
        console.print(chunk["text"], end="", highlight=False)
    elif chunk["type"] == "tool_call":
        console.print()
        args_str = json.dumps(chunk["input"], indent=2)
        console.print(Panel(
            Syntax(args_str, "json", theme="monokai"),
            title=f"[bold cyan]{chunk['name']}[/]",
            border_style="cyan",
            expand=False,
        ))
    elif chunk["type"] == "tool_result":
        content = chunk["content"]
        if content.startswith("ERROR") or content.startswith("Permission denied"):
            console.print(Panel(content, border_style="red", expand=False))
        elif content.startswith("---") or content.startswith("+++"):
            console.print(Syntax(content, "diff", theme="monokai"))
        else:
            for line in content.splitlines()[:30]:
                console.print(f"  {line}", highlight=False)
            if content.count("\n") > 30:
                console.print(f"  [dim]... ({content.count(chr(10)) - 30} more lines)[/]")


def render_end():
    console.print()
