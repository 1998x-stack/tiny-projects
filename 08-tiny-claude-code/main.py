import os
import sys

from prompt_toolkit import PromptSession
from prompt_toolkit.history import FileHistory

from config import load_config
from provider import Provider
from agent import Agent
from tools import ToolRegistry
from tools.files import ReadTool, WriteTool, EditTool
from tools.bash import BashTool
from tools.search import GrepTool, GlobTool
from tools.todo import TodoTool
from prompt import build_system_prompt
from render import console, render_chunk, render_end


def build_tool_registry(permission_checker=None):
    todo_tool = TodoTool()
    registry = ToolRegistry(permission_checker=permission_checker)
    for tool in [ReadTool(), WriteTool(), EditTool(), BashTool(),
                 GrepTool(), GlobTool(), todo_tool]:
        registry.register(tool)
    return registry, todo_tool


def main():
    cwd = os.getcwd()
    config = load_config()
    provider = Provider(config.model)
    tools, todo_tool = build_tool_registry()

    def system_prompt():
        return build_system_prompt(cwd=cwd, todos=todo_tool.todos)

    agent = Agent(provider, tools, system_prompt)
    session = PromptSession(history=FileHistory(".tiny-claude-history"))

    console.print("[bold]Tiny Claude Code[/bold] — Ctrl+D to exit\n")

    while True:
        try:
            user_input = session.prompt(">>> ").strip()
        except (EOFError, KeyboardInterrupt):
            console.print("\n[dim]Bye![/]")
            break
        if not user_input:
            continue
        agent.messages.append({"role": "user", "content": user_input})
        for chunk in agent.run_stream():
            render_chunk(chunk)
        render_end()


if __name__ == "__main__":
    main()
