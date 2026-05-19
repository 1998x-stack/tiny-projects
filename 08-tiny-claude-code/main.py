import json
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
from context import ContextManager
from permissions import PermissionChecker
from session import SessionManager
from tools.subagent import SubagentTool
from render import console, render_chunk, render_end


def build_tool_registry(permission_checker=None):
    todo_tool = TodoTool()
    registry = ToolRegistry(permission_checker=permission_checker)
    for tool in [ReadTool(), WriteTool(), EditTool(), BashTool(),
                 GrepTool(), GlobTool(), todo_tool]:
        registry.register(tool)
    return registry, todo_tool


def handle_slash_command(cmd, agent, todo_tool, session_mgr, context_mgr, provider):
    parts = cmd.strip().split(maxsplit=1)
    command = parts[0].lower()
    arg = parts[1] if len(parts) > 1 else None

    if command == "/help":
        console.print("""[bold]Commands:[/]
  /help            Show this help
  /compact         Force context compression
  /save [name]     Save session
  /resume [name]   Load a saved session
  /sessions        List saved sessions
  /model [name]    Switch model
  /todo            Show current tasks
  /clear           Clear conversation
  /exit            Quit""")
    elif command == "/compact":
        context_mgr.maybe_compact(agent.messages)
        console.print("[dim]Context compacted.[/]")
    elif command == "/save":
        name = arg or "default"
        session_mgr.save(name, agent.messages, todo_tool.todos)
        console.print(f"[dim]Session saved: {name}[/]")
    elif command == "/resume":
        if not arg:
            console.print("[red]Usage: /resume <name>[/]")
            return
        try:
            msgs, todos = session_mgr.resume(arg)
            agent.messages = msgs
            todo_tool.todos = todos
            console.print(f"[dim]Session resumed: {arg} ({len(msgs)} messages)[/]")
        except FileNotFoundError as e:
            console.print(f"[red]{e}[/]")
    elif command == "/sessions":
        sessions = session_mgr.list_sessions()
        if sessions:
            for s in sessions:
                console.print(f"  {s}")
        else:
            console.print("[dim]No saved sessions.[/]")
    elif command == "/model":
        if not arg:
            console.print(f"[dim]Current model: {provider.model}[/]")
        else:
            provider.model = arg
            console.print(f"[dim]Model switched to: {arg}[/]")
    elif command == "/todo":
        if todo_tool.todos:
            console.print(todo_tool._format())
        else:
            console.print("[dim]No tasks.[/]")
    elif command == "/clear":
        agent.messages.clear()
        console.print("[dim]Conversation cleared.[/]")
    elif command == "/exit":
        raise SystemExit(0)
    else:
        console.print(f"[red]Unknown command: {command}. Type /help for commands.[/]")


def main():
    cwd = os.getcwd()
    config = load_config()
    provider = Provider(config.model)
    permissions = PermissionChecker(
        cwd=cwd,
        prompt_fn=lambda tool, args: console.input(
            f"[yellow]Allow {tool}({json.dumps(args)[:80]})?[/] [y/N] "
        ).strip().lower() in ("y", "yes"),
    )
    tools, todo_tool = build_tool_registry(permission_checker=permissions)
    context_mgr = ContextManager(provider, max_tokens=config.max_tokens)
    session_mgr = SessionManager()

    def system_prompt():
        return build_system_prompt(cwd=cwd, todos=todo_tool.todos)

    agent = Agent(provider, tools, system_prompt, pre_call=context_mgr.maybe_compact)
    tools.register(SubagentTool(agent))
    session = PromptSession(history=FileHistory(".tiny-claude-history"))

    console.print("[bold]Tiny Claude Code[/bold] — /help for commands, Ctrl+D to exit\n")

    while True:
        try:
            user_input = session.prompt(">>> ").strip()
        except (EOFError, KeyboardInterrupt):
            console.print("\n[dim]Bye![/]")
            break
        if not user_input:
            continue
        if user_input.startswith("/"):
            handle_slash_command(user_input, agent, todo_tool, session_mgr, context_mgr, provider)
            continue
        agent.messages.append({"role": "user", "content": user_input})
        for chunk in agent.run_stream():
            render_chunk(chunk)
        render_end()


if __name__ == "__main__":
    main()
