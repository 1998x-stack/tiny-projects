import pytest
from types import SimpleNamespace

from tools import BaseTool, ToolRegistry


class MockProvider:
    def __init__(self, responses):
        self._responses = iter(responses)

    def create(self, system, messages, tools):
        return next(self._responses)


def _text_response(text):
    return SimpleNamespace(
        content=[{"type": "text", "text": text}],
        stop_reason="end_turn",
        tool_calls=[],
    )


def _tool_response(tool_name, tool_id, tool_input):
    return SimpleNamespace(
        content=[{"type": "tool_use", "id": tool_id, "name": tool_name, "input": tool_input}],
        stop_reason="tool_use",
        tool_calls=[SimpleNamespace(name=tool_name, id=tool_id, input=tool_input)],
    )


class EchoTool(BaseTool):
    name = "echo"
    description = "echo"
    parameters = {"msg": {"type": "string", "description": "msg"}}
    def execute(self, msg):
        return f"echoed: {msg}"


def test_agent_text_response():
    from agent import Agent

    provider = MockProvider([_text_response("Hello!")])
    registry = ToolRegistry()
    agent = Agent(provider, registry, "You are helpful.")
    agent.messages = [{"role": "user", "content": "Hi"}]
    result = agent.run()
    assert result[-1]["role"] == "assistant"
    assert result[-1]["content"][0]["text"] == "Hello!"


def test_agent_tool_then_text():
    from agent import Agent

    provider = MockProvider([
        _tool_response("echo", "c1", {"msg": "test"}),
        _text_response("Done."),
    ])
    registry = ToolRegistry()
    registry.register(EchoTool())
    agent = Agent(provider, registry, "You are helpful.")
    agent.messages = [{"role": "user", "content": "echo something"}]
    result = agent.run()
    assert len(result) == 4
    assert result[1]["content"][0]["type"] == "tool_use"
    assert result[2]["content"][0]["content"] == "echoed: test"
    assert result[3]["content"][0]["text"] == "Done."


def test_agent_max_turns():
    from agent import Agent, TurnLimitExceeded

    provider = MockProvider(
        [_tool_response("echo", f"c{i}", {"msg": "x"}) for i in range(10)]
    )
    registry = ToolRegistry()
    registry.register(EchoTool())
    agent = Agent(provider, registry, "You are helpful.", max_turns=3)
    agent.messages = [{"role": "user", "content": "loop"}]
    with pytest.raises(TurnLimitExceeded):
        agent.run()


def test_agent_pre_call_hook():
    from agent import Agent

    called = []

    def hook(messages):
        called.append(len(messages))

    provider = MockProvider([_text_response("Ok")])
    registry = ToolRegistry()
    agent = Agent(provider, registry, "test", pre_call=hook)
    agent.messages = [{"role": "user", "content": "hi"}]
    agent.run()
    assert len(called) == 1


def test_agent_system_callable():
    from agent import Agent

    counter = [0]

    def sys_fn():
        counter[0] += 1
        return f"prompt v{counter[0]}"

    provider = MockProvider([
        _tool_response("echo", "c1", {"msg": "x"}),
        _text_response("done"),
    ])
    registry = ToolRegistry()
    registry.register(EchoTool())
    agent = Agent(provider, registry, sys_fn)
    agent.messages = [{"role": "user", "content": "go"}]
    agent.run()
    assert counter[0] == 2
