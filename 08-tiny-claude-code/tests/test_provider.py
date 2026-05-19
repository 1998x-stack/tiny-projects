import json
import pytest
from types import SimpleNamespace


def _make_openai_response(content=None, tool_calls=None):
    """Build a fake LiteLLM/OpenAI response object."""
    msg = SimpleNamespace(content=content, tool_calls=tool_calls)
    choice = SimpleNamespace(message=msg)
    return SimpleNamespace(choices=[choice])


def test_normalize_text_response():
    from provider import Provider
    p = Provider.__new__(Provider)
    resp = _make_openai_response(content="Hello world")
    result = p._normalize(resp)
    assert result.stop_reason == "end_turn"
    assert len(result.content) == 1
    assert result.content[0] == {"type": "text", "text": "Hello world"}
    assert result.tool_calls == []


def test_normalize_tool_use_response():
    from provider import Provider
    p = Provider.__new__(Provider)
    tc = SimpleNamespace(
        id="call_123",
        function=SimpleNamespace(name="read", arguments='{"file_path": "/tmp/x.py"}'),
    )
    resp = _make_openai_response(content=None, tool_calls=[tc])
    result = p._normalize(resp)
    assert result.stop_reason == "tool_use"
    assert len(result.content) == 1
    assert result.content[0]["type"] == "tool_use"
    assert result.content[0]["name"] == "read"
    assert result.content[0]["input"] == {"file_path": "/tmp/x.py"}
    assert len(result.tool_calls) == 1
    assert result.tool_calls[0].name == "read"


def test_normalize_mixed_response():
    from provider import Provider
    p = Provider.__new__(Provider)
    tc = SimpleNamespace(
        id="call_456",
        function=SimpleNamespace(name="bash", arguments='{"command": "ls"}'),
    )
    resp = _make_openai_response(content="Let me check.", tool_calls=[tc])
    result = p._normalize(resp)
    assert result.stop_reason == "tool_use"
    assert len(result.content) == 2
    assert result.content[0]["type"] == "text"
    assert result.content[1]["type"] == "tool_use"


def test_to_litellm_messages_text():
    from provider import Provider
    p = Provider.__new__(Provider)
    msgs = [
        {"role": "user", "content": "hello"},
        {"role": "assistant", "content": [{"type": "text", "text": "hi"}]},
    ]
    result = p._to_litellm_messages(msgs)
    assert result[0] == {"role": "user", "content": "hello"}
    assert result[1]["role"] == "assistant"
    assert result[1]["content"] == "hi"


def test_to_litellm_messages_tool_cycle():
    from provider import Provider
    p = Provider.__new__(Provider)
    msgs = [
        {"role": "user", "content": "read foo"},
        {"role": "assistant", "content": [
            {"type": "tool_use", "id": "c1", "name": "read", "input": {"file_path": "foo.py"}},
        ]},
        {"role": "user", "content": [
            {"type": "tool_result", "tool_use_id": "c1", "content": "file contents here"},
        ]},
    ]
    result = p._to_litellm_messages(msgs)
    assert result[0] == {"role": "user", "content": "read foo"}
    assert result[1]["role"] == "assistant"
    assert result[1]["tool_calls"][0]["function"]["name"] == "read"
    assert result[2]["role"] == "tool"
    assert result[2]["tool_call_id"] == "c1"
    assert result[2]["content"] == "file contents here"


def test_to_litellm_tools():
    from provider import Provider
    p = Provider.__new__(Provider)
    internal_tools = [
        {
            "name": "read",
            "description": "Read a file",
            "input_schema": {
                "type": "object",
                "properties": {"file_path": {"type": "string"}},
                "required": ["file_path"],
            },
        }
    ]
    result = p._to_litellm_tools(internal_tools)
    assert len(result) == 1
    assert result[0]["type"] == "function"
    assert result[0]["function"]["name"] == "read"
    assert result[0]["function"]["parameters"] == internal_tools[0]["input_schema"]
