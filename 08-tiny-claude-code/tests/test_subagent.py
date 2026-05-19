from types import SimpleNamespace

from tools import ToolRegistry


class MockProvider:
    def __init__(self, responses):
        self._responses = iter(responses)

    def create(self, system, messages, tools):
        return next(self._responses)


def test_subagent_tool_returns_text():
    from agent import Agent
    from tools.subagent import SubagentTool

    provider = MockProvider([
        SimpleNamespace(
            content=[{"type": "text", "text": "The answer is 42."}],
            stop_reason="end_turn",
            tool_calls=[],
        ),
    ])
    registry = ToolRegistry()
    agent = Agent(provider, registry, "You are helpful.")
    tool = SubagentTool(agent)
    result = tool.execute(prompt="What is the answer?")
    assert "42" in result


def test_subagent_isolates_messages():
    from agent import Agent
    from tools.subagent import SubagentTool

    provider = MockProvider([
        SimpleNamespace(
            content=[{"type": "text", "text": "sub result"}],
            stop_reason="end_turn",
            tool_calls=[],
        ),
    ])
    registry = ToolRegistry()
    agent = Agent(provider, registry, "You are helpful.")
    agent.messages = [{"role": "user", "content": "parent context"}]
    tool = SubagentTool(agent)
    tool.execute(prompt="child task")
    assert len(agent.messages) == 1
    assert agent.messages[0]["content"] == "parent context"
