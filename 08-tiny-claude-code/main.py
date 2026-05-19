import os
import sys

from config import load_config
from provider import Provider
from agent import Agent
from tools import ToolRegistry
from tools.files import ReadTool, WriteTool, EditTool
from tools.bash import BashTool
from tools.search import GrepTool, GlobTool


def build_tool_registry(permission_checker=None):
    registry = ToolRegistry(permission_checker=permission_checker)
    for tool_cls in [ReadTool, WriteTool, EditTool, BashTool, GrepTool, GlobTool]:
        registry.register(tool_cls())
    return registry


def main():
    config = load_config()
    provider = Provider(config.model)
    tools = build_tool_registry()
    agent = Agent(provider, tools, f"You are Tiny Claude Code.\nWorking directory: {os.getcwd()}")

    print("Tiny Claude Code — Ctrl+D to exit\n")

    while True:
        try:
            user_input = input(">>> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nBye!")
            break
        if not user_input:
            continue
        agent.messages.append({"role": "user", "content": user_input})
        result = agent.run()
        last = result[-1]
        for block in last["content"]:
            if block["type"] == "text":
                print(f"\n{block['text']}\n")


if __name__ == "__main__":
    main()
