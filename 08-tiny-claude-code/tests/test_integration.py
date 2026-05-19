from types import SimpleNamespace

from tools import ToolRegistry
from tools.files import ReadTool, WriteTool, EditTool
from tools.bash import BashTool
from tools.search import GrepTool, GlobTool
from tools.todo import TodoTool


class MockProvider:
    def __init__(self, responses):
        self._responses = iter(responses)

    def create(self, system, messages, tools):
        return next(self._responses)


def test_full_tool_registry():
    todo = TodoTool()
    reg = ToolRegistry()
    for tool in [ReadTool(), WriteTool(), EditTool(), BashTool(),
                 GrepTool(), GlobTool(), todo]:
        reg.register(tool)
    defs = reg.definitions()
    assert len(defs) == 7
    names = {d["name"] for d in defs}
    assert names == {"read", "write", "edit", "bash", "grep", "glob", "todo_write"}
    for d in defs:
        assert "input_schema" in d
        assert d["input_schema"]["type"] == "object"


def test_agent_read_then_edit_flow(tmp_path):
    from agent import Agent

    target = tmp_path / "hello.py"
    target.write_text("print('Helo world')\n")

    provider = MockProvider([
        SimpleNamespace(
            content=[{"type": "tool_use", "id": "r1", "name": "read",
                       "input": {"file_path": str(target)}}],
            stop_reason="tool_use",
            tool_calls=[SimpleNamespace(name="read", id="r1",
                                        input={"file_path": str(target)})],
        ),
        SimpleNamespace(
            content=[{"type": "tool_use", "id": "e1", "name": "edit",
                       "input": {"file_path": str(target),
                                 "old_string": "print('Helo world')",
                                 "new_string": "print('Hello world')"}}],
            stop_reason="tool_use",
            tool_calls=[SimpleNamespace(name="edit", id="e1",
                                        input={"file_path": str(target),
                                               "old_string": "print('Helo world')",
                                               "new_string": "print('Hello world')"})],
        ),
        SimpleNamespace(
            content=[{"type": "text", "text": "Fixed the typo."}],
            stop_reason="end_turn",
            tool_calls=[],
        ),
    ])

    reg = ToolRegistry()
    for tool in [ReadTool(), WriteTool(), EditTool(), BashTool()]:
        reg.register(tool)

    agent = Agent(provider, reg, "You are helpful.")
    agent.messages = [{"role": "user", "content": "fix the typo"}]
    result = agent.run()

    assert target.read_text() == "print('Hello world')\n"
    assert result[-1]["content"][0]["text"] == "Fixed the typo."


def test_config_loads():
    from config import load_config

    config = load_config()
    assert hasattr(config, "model")
    assert hasattr(config, "max_tokens")
