# Tiny Claude Code Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a ~1,500 LoC toy Claude Code clone — an interactive AI coding agent with 7 tools, streaming output, context compression, permissions, session persistence, and subagent support.

**Architecture:** 7-layer stack. Agent loop (Layer 1) calls Provider (Layer 2) and dispatches Tools (Layer 3). Context compression, permissions, persistence, and CLI wrap around via hooks and composition. Agent owns `messages[]`; system prompt is a callable rebuilt each turn. Provider normalizes LiteLLM's OpenAI wire format to Anthropic-style content blocks.

**Tech Stack:** Python 3.11+, LiteLLM (provider abstraction), Rich (terminal rendering), prompt_toolkit (REPL input), PyYAML (permissions config)

**Spec:** `spec.md` (full architecture, code, gotchas). **Glossary:** `CONTEXT.md` (domain terms). **ADRs:** `docs/adr/` (2 decisions recorded).

---

## File Structure

| File | Responsibility | Created in |
|------|---------------|------------|
| `requirements.txt` | Dependencies | Task 1 |
| `config.py` | Env/config loading | Task 1 |
| `provider.py` | LiteLLM wrapper, normalization, streaming | Task 1 |
| `tools/__init__.py` | BaseTool ABC, ToolRegistry, dispatch | Task 2 |
| `tools/files.py` | ReadTool, WriteTool, EditTool | Task 2, Task 4 |
| `tools/bash.py` | BashTool — shell execution with timeout | Task 2 |
| `agent.py` | Agent class — sync loop, streaming loop, subagent | Task 3 |
| `main.py` | CLI entry, bare REPL | Task 3, modified in Tasks 7, 8, 9, 10, 11 |
| `tools/search.py` | GrepTool, GlobTool | Task 5 |
| `tools/todo.py` | TodoTool — plan-before-execute | Task 6 |
| `prompt.py` | System prompt builder, todo nag, skill loading | Task 6 |
| `render.py` | Rich markdown rendering, panels, spinners, diffs | Task 7 |
| `context.py` | ContextManager — 3-layer compression | Task 8 |
| `permissions.py` | PermissionChecker — YAML rules, cwd-aware | Task 9 |
| `permissions.yaml` | Default permission rules | Task 9 |
| `session.py` | SessionManager — save/resume to JSON | Task 10 |
| `tools/subagent.py` | SubagentTool — child agent spawning | Task 11 |

---

### Task 1: Project Setup + Provider Layer

**Files:**
- Create: `requirements.txt`
- Create: `config.py`
- Create: `provider.py`
- Create: `tests/test_provider.py`

- [ ] **Step 1: Create requirements.txt**

```
litellm>=1.40.0
pyyaml>=6.0
rich>=13.0
prompt_toolkit>=3.0
pytest>=8.0
```

- [ ] **Step 2: Install dependencies**

Run: `pip install -r requirements.txt`
Expected: All packages install successfully

- [ ] **Step 3: Write config.py**

```python
import os
from types import SimpleNamespace


def load_config():
    return SimpleNamespace(
        model=os.environ.get("MODEL", "anthropic/claude-sonnet-4-20250514"),
        max_tokens=int(os.environ.get("MAX_TOKENS", "100000")),
    )
```

- [ ] **Step 4: Write the failing test for Provider._normalize**

```python
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
```

- [ ] **Step 5: Run tests to verify they fail**

Run: `pytest tests/test_provider.py -v`
Expected: FAIL — `provider` module does not exist

- [ ] **Step 6: Write provider.py**

```python
import json
from types import SimpleNamespace

from litellm import completion


class Provider:
    def __init__(self, model="anthropic/claude-sonnet-4-20250514"):
        self.model = model

    def create(self, system, messages, tools):
        response = completion(
            model=self.model,
            messages=[{"role": "system", "content": system}] + messages,
            tools=self._to_litellm_tools(tools),
        )
        return self._normalize(response)

    def stream(self, system, messages, tools):
        raw = completion(
            model=self.model,
            messages=[{"role": "system", "content": system}] + messages,
            tools=self._to_litellm_tools(tools),
            stream=True,
        )
        tc_accum = {}
        for chunk in raw:
            delta = chunk.choices[0].delta
            if delta.content:
                yield {"type": "text", "text": delta.content}
            for tc_delta in (delta.tool_calls or []):
                idx = tc_delta.index
                if idx not in tc_accum:
                    tc_accum[idx] = {
                        "id": tc_delta.id,
                        "name": tc_delta.function.name,
                        "args": "",
                    }
                else:
                    tc_accum[idx]["args"] += tc_delta.function.arguments or ""
        for tc in tc_accum.values():
            yield {
                "type": "tool_use",
                "id": tc["id"],
                "name": tc["name"],
                "input": json.loads(tc["args"]),
            }

    def _normalize(self, response):
        msg = response.choices[0].message
        content = []
        if msg.content:
            content.append({"type": "text", "text": msg.content})
        for tc in (msg.tool_calls or []):
            content.append({
                "type": "tool_use",
                "id": tc.id,
                "name": tc.function.name,
                "input": json.loads(tc.function.arguments),
            })
        stop = "tool_use" if msg.tool_calls else "end_turn"
        return SimpleNamespace(
            content=content,
            stop_reason=stop,
            tool_calls=[
                SimpleNamespace(name=b["name"], id=b["id"], input=b["input"])
                for b in content
                if b["type"] == "tool_use"
            ],
        )

    def _to_litellm_tools(self, tools):
        return [
            {
                "type": "function",
                "function": {
                    "name": t["name"],
                    "description": t["description"],
                    "parameters": t["input_schema"],
                },
            }
            for t in tools
        ]
```

- [ ] **Step 7: Run tests to verify they pass**

Run: `pytest tests/test_provider.py -v`
Expected: 4 passed

- [ ] **Step 8: Commit**

```bash
git add requirements.txt config.py provider.py tests/test_provider.py
git commit -m "feat: project setup + provider layer with LiteLLM normalization"
```

---

### Task 2: Tool System — BaseTool, Registry, ReadTool, BashTool

**Files:**
- Create: `tools/__init__.py`
- Create: `tools/files.py` (ReadTool only — WriteTool and EditTool in Task 4)
- Create: `tools/bash.py`
- Create: `tests/test_tools.py`

- [ ] **Step 1: Write failing tests for BaseTool and ToolRegistry**

```python
import pytest
from types import SimpleNamespace


def test_tool_definition_schema():
    from tools import BaseTool

    class DummyTool(BaseTool):
        name = "dummy"
        description = "A test tool"
        parameters = {"x": {"type": "string", "description": "input"}}

        def execute(self, x):
            return f"got {x}"

    tool = DummyTool()
    defn = tool.definition()
    assert defn["name"] == "dummy"
    assert defn["input_schema"]["properties"]["x"]["type"] == "string"
    assert defn["input_schema"]["required"] == ["x"]


def test_registry_definitions():
    from tools import BaseTool, ToolRegistry

    class FooTool(BaseTool):
        name = "foo"
        description = "foo tool"
        parameters = {"a": {"type": "string", "description": "a"}}
        def execute(self, a):
            return a

    reg = ToolRegistry()
    reg.register(FooTool())
    defs = reg.definitions()
    assert len(defs) == 1
    assert defs[0]["name"] == "foo"


def test_registry_execute_all_success():
    from tools import BaseTool, ToolRegistry

    class EchoTool(BaseTool):
        name = "echo"
        description = "echo"
        parameters = {"msg": {"type": "string", "description": "msg"}}
        def execute(self, msg):
            return msg

    reg = ToolRegistry()
    reg.register(EchoTool())
    calls = [SimpleNamespace(name="echo", id="c1", input={"msg": "hi"})]
    results = reg.execute_all(calls)
    assert len(results) == 1
    assert results[0]["content"] == "hi"
    assert results[0]["tool_use_id"] == "c1"


def test_registry_execute_unknown_tool():
    from tools import ToolRegistry

    reg = ToolRegistry()
    calls = [SimpleNamespace(name="nope", id="c2", input={})]
    results = reg.execute_all(calls)
    assert "ERROR" in results[0]["content"]
    assert "nope" in results[0]["content"]


def test_registry_execute_catches_exceptions():
    from tools import BaseTool, ToolRegistry

    class BrokenTool(BaseTool):
        name = "broken"
        description = "always fails"
        parameters = {}
        def execute(self):
            raise ValueError("boom")

    reg = ToolRegistry()
    reg.register(BrokenTool())
    calls = [SimpleNamespace(name="broken", id="c3", input={})]
    results = reg.execute_all(calls)
    assert "ERROR" in results[0]["content"]
    assert "boom" in results[0]["content"]
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_tools.py -v`
Expected: FAIL — `tools` module does not exist

- [ ] **Step 3: Write tools/__init__.py**

```python
from abc import ABC, abstractmethod


class BaseTool(ABC):
    name: str
    description: str
    parameters: dict

    @abstractmethod
    def execute(self, **kwargs) -> str:
        pass

    def definition(self) -> dict:
        return {
            "name": self.name,
            "description": self.description,
            "input_schema": {
                "type": "object",
                "properties": self.parameters,
                "required": list(self.parameters.keys()),
            },
        }


class ToolRegistry:
    def __init__(self, permission_checker=None):
        self._tools: dict[str, BaseTool] = {}
        self._permissions = permission_checker

    def register(self, tool: BaseTool):
        self._tools[tool.name] = tool

    def definitions(self) -> list[dict]:
        return [t.definition() for t in self._tools.values()]

    def execute_all(self, tool_calls) -> list[dict]:
        results = []
        for call in tool_calls:
            tool = self._tools.get(call.name)
            if not tool:
                results.append({
                    "type": "tool_result",
                    "tool_use_id": call.id,
                    "content": f"ERROR: Unknown tool '{call.name}'",
                })
                continue
            if self._permissions:
                allowed, reason = self._permissions.check(call.name, call.input)
                if not allowed:
                    results.append({
                        "type": "tool_result",
                        "tool_use_id": call.id,
                        "content": f"Permission denied: {reason}",
                    })
                    continue
            try:
                output = tool.execute(**call.input)
            except Exception as e:
                output = f"ERROR: {e}"
            results.append({
                "type": "tool_result",
                "tool_use_id": call.id,
                "content": str(output),
            })
        return results
```

- [ ] **Step 4: Run BaseTool/ToolRegistry tests**

Run: `pytest tests/test_tools.py -v`
Expected: 5 passed

- [ ] **Step 5: Write failing tests for ReadTool and BashTool**

```python
import os
import tempfile


def test_read_tool_returns_numbered_lines(tmp_path):
    from tools.files import ReadTool

    f = tmp_path / "hello.py"
    f.write_text("line one\nline two\nline three\n")
    tool = ReadTool()
    result = tool.execute(file_path=str(f))
    assert "1\t" in result
    assert "line one" in result
    assert "3\t" in result


def test_read_tool_truncates_large_files(tmp_path):
    from tools.files import ReadTool

    f = tmp_path / "big.txt"
    f.write_text("x" * 10000)
    tool = ReadTool()
    result = tool.execute(file_path=str(f))
    assert len(result) <= 8200  # 8000 + marker
    assert "[truncated]" in result


def test_read_tool_missing_file():
    from tools.files import ReadTool

    tool = ReadTool()
    result = tool.execute(file_path="/nonexistent/path/xyz.py")
    assert "ERROR" in result


def test_bash_tool_captures_stdout():
    from tools.bash import BashTool

    tool = BashTool()
    result = tool.execute(command="echo hello")
    assert "hello" in result


def test_bash_tool_captures_stderr():
    from tools.bash import BashTool

    tool = BashTool()
    result = tool.execute(command="echo oops >&2")
    assert "oops" in result


def test_bash_tool_truncates_output():
    from tools.bash import BashTool

    tool = BashTool()
    result = tool.execute(command="python3 -c \"print('x' * 10000)\"")
    assert len(result) <= 8200
    assert "[truncated]" in result


def test_bash_tool_timeout():
    from tools.bash import BashTool

    tool = BashTool()
    result = tool.execute(command="sleep 60")
    assert "ERROR" in result or "timed out" in result.lower()
```

- [ ] **Step 6: Run tests to verify they fail**

Run: `pytest tests/test_tools.py::test_read_tool_returns_numbered_lines -v`
Expected: FAIL — `tools.files` does not exist

- [ ] **Step 7: Write tools/files.py (ReadTool only)**

```python
from pathlib import Path

from tools import BaseTool

MAX_OUTPUT = 8000


class ReadTool(BaseTool):
    name = "read"
    description = "Read a file from the local filesystem. Returns content with line numbers."
    parameters = {
        "file_path": {"type": "string", "description": "Absolute path to the file to read"},
    }

    def execute(self, file_path) -> str:
        try:
            content = Path(file_path).read_text()
        except Exception as e:
            return f"ERROR: {e}"
        lines = content.splitlines()
        numbered = "\n".join(f"{i + 1}\t{line}" for i, line in enumerate(lines))
        if len(numbered) > MAX_OUTPUT:
            return numbered[:MAX_OUTPUT] + "\n[truncated]"
        return numbered
```

- [ ] **Step 8: Write tools/bash.py**

```python
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
```

- [ ] **Step 9: Run all tool tests**

Run: `pytest tests/test_tools.py -v`
Expected: 12 passed

- [ ] **Step 10: Commit**

```bash
git add tools/__init__.py tools/files.py tools/bash.py tests/test_tools.py
git commit -m "feat: tool system — BaseTool, ToolRegistry, ReadTool, BashTool"
```

---

### Task 3: Agent Loop + Bare REPL

**Files:**
- Create: `agent.py`
- Create: `main.py`
- Create: `tests/test_agent.py`

- [ ] **Step 1: Write failing tests for Agent.run**

```python
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
    # Should have: user, assistant (tool_use), user (tool_result), assistant (text)
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
    assert counter[0] == 2  # called once per turn
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_agent.py -v`
Expected: FAIL — `agent` module does not exist

- [ ] **Step 3: Write agent.py**

```python
from types import SimpleNamespace


class TurnLimitExceeded(Exception):
    pass


class Agent:
    def __init__(self, provider, tool_registry, system_prompt,
                 pre_call=None, max_turns=100):
        self.provider = provider
        self.tools = tool_registry
        self._system = system_prompt
        self.pre_call = pre_call
        self.max_turns = max_turns
        self.messages = []

    @property
    def system(self):
        return self._system() if callable(self._system) else self._system

    def run(self):
        for turn in range(self.max_turns):
            if self.pre_call:
                self.pre_call(self.messages)
            response = self.provider.create(
                system=self.system,
                messages=self.messages,
                tools=self.tools.definitions(),
            )
            self.messages.append({"role": "assistant", "content": response.content})
            if response.stop_reason != "tool_use":
                return self.messages
            results = self.tools.execute_all(response.tool_calls)
            self.messages.append({"role": "user", "content": results})
        raise TurnLimitExceeded(f"Exceeded {self.max_turns} turns")

    def run_stream(self):
        for turn in range(self.max_turns):
            if self.pre_call:
                self.pre_call(self.messages)
            content = []
            tool_calls = []
            for chunk in self.provider.stream(
                system=self.system,
                messages=self.messages,
                tools=self.tools.definitions(),
            ):
                if chunk["type"] == "text":
                    content.append(chunk)
                    yield chunk
                elif chunk["type"] == "tool_use":
                    content.append(chunk)
                    tool_calls.append(
                        SimpleNamespace(
                            name=chunk["name"], id=chunk["id"], input=chunk["input"]
                        )
                    )
            if tool_calls:
                for tc in tool_calls:
                    yield {"type": "tool_call", "name": tc.name, "input": tc.input}
                self.messages.append({"role": "assistant", "content": content})
                results = self.tools.execute_all(tool_calls)
                for r in results:
                    yield {"type": "tool_result", "content": r["content"]}
                self.messages.append({"role": "user", "content": results})
            else:
                self.messages.append({"role": "assistant", "content": content})
                return

    def spawn_subagent(self, prompt, tool_subset=None):
        child = Agent(
            provider=self.provider,
            tool_registry=tool_subset or self.tools,
            system_prompt=self._system,
            max_turns=50,
        )
        child.messages = [{"role": "user", "content": prompt}]
        result = child.run()
        return extract_final_text(result)


def extract_final_text(messages):
    for msg in reversed(messages):
        if msg["role"] == "assistant":
            for block in msg["content"]:
                if block["type"] == "text":
                    return block["text"]
    return ""
```

- [ ] **Step 4: Run agent tests**

Run: `pytest tests/test_agent.py -v`
Expected: 5 passed

- [ ] **Step 5: Write main.py — bare REPL (no streaming, no Rich yet)**

```python
import os
import sys

from config import load_config
from provider import Provider
from agent import Agent
from tools import ToolRegistry
from tools.files import ReadTool
from tools.bash import BashTool


def build_tool_registry(permission_checker=None):
    registry = ToolRegistry(permission_checker=permission_checker)
    for tool_cls in [ReadTool, BashTool]:
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
```

- [ ] **Step 6: Manual smoke test**

Run: `MODEL=anthropic/claude-sonnet-4-20250514 python main.py`
Type: `read the file config.py`
Expected: Agent calls read tool, shows config.py contents, then gives a text summary

- [ ] **Step 7: Commit**

```bash
git add agent.py main.py tests/test_agent.py
git commit -m "feat: agent loop + bare REPL with read and bash tools"
```

---

### Task 4: WriteTool + EditTool

**Files:**
- Modify: `tools/files.py` — add WriteTool and EditTool
- Create: `tests/test_files.py`

- [ ] **Step 1: Write failing tests for WriteTool and EditTool**

```python
import pytest


def test_write_creates_file(tmp_path):
    from tools.files import WriteTool

    tool = WriteTool()
    path = str(tmp_path / "new.txt")
    result = tool.execute(file_path=path, content="hello world")
    assert "11" in result  # 11 bytes
    assert (tmp_path / "new.txt").read_text() == "hello world"


def test_write_creates_parent_dirs(tmp_path):
    from tools.files import WriteTool

    tool = WriteTool()
    path = str(tmp_path / "a" / "b" / "c.txt")
    result = tool.execute(file_path=path, content="deep")
    assert "4" in result
    assert (tmp_path / "a" / "b" / "c.txt").read_text() == "deep"


def test_write_overwrites_existing(tmp_path):
    from tools.files import WriteTool

    f = tmp_path / "exist.txt"
    f.write_text("old")
    tool = WriteTool()
    tool.execute(file_path=str(f), content="new")
    assert f.read_text() == "new"


def test_edit_unique_match(tmp_path):
    from tools.files import EditTool

    f = tmp_path / "code.py"
    f.write_text("def hello():\n    print('Helo')\n")
    tool = EditTool()
    result = tool.execute(
        file_path=str(f),
        old_string="print('Helo')",
        new_string="print('Hello')",
    )
    assert "Hello" in f.read_text()
    assert "---" in result  # unified diff


def test_edit_no_match(tmp_path):
    from tools.files import EditTool

    f = tmp_path / "code.py"
    f.write_text("def hello():\n    pass\n")
    tool = EditTool()
    result = tool.execute(
        file_path=str(f),
        old_string="nonexistent string",
        new_string="replacement",
    )
    assert "ERROR" in result
    assert "not found" in result


def test_edit_multiple_matches(tmp_path):
    from tools.files import EditTool

    f = tmp_path / "code.py"
    f.write_text("x = 1\nx = 2\nx = 3\n")
    tool = EditTool()
    result = tool.execute(
        file_path=str(f),
        old_string="x = ",
        new_string="y = ",
    )
    assert "ERROR" in result
    assert "3 matches" in result
    assert f.read_text() == "x = 1\nx = 2\nx = 3\n"  # unchanged
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_files.py -v`
Expected: FAIL — WriteTool/EditTool not defined

- [ ] **Step 3: Add WriteTool and EditTool to tools/files.py**

Append to the existing `tools/files.py`:

```python
import difflib


class WriteTool(BaseTool):
    name = "write"
    description = "Create or overwrite a file. Creates parent directories if needed."
    parameters = {
        "file_path": {"type": "string", "description": "Absolute path to the file"},
        "content": {"type": "string", "description": "Content to write"},
    }

    def execute(self, file_path, content) -> str:
        try:
            p = Path(file_path)
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(content)
            return f"Wrote {len(content.encode())} bytes to {file_path}"
        except Exception as e:
            return f"ERROR: {e}"


class EditTool(BaseTool):
    name = "edit"
    description = "Replace old_string with new_string in a file. old_string must match exactly once."
    parameters = {
        "file_path": {"type": "string", "description": "Absolute path to file"},
        "old_string": {"type": "string", "description": "Exact text to find (must be unique)"},
        "new_string": {"type": "string", "description": "Replacement text"},
    }

    def execute(self, file_path, old_string, new_string) -> str:
        try:
            p = Path(file_path)
            content = p.read_text()
        except Exception as e:
            return f"ERROR: {e}"
        count = content.count(old_string)
        if count == 0:
            return f"ERROR: old_string not found in {file_path}"
        if count > 1:
            return f"ERROR: Found {count} matches. Provide more surrounding context to make the match unique."
        new_content = content.replace(old_string, new_string, 1)
        p.write_text(new_content)
        return "\n".join(difflib.unified_diff(
            old_string.splitlines(), new_string.splitlines(),
            fromfile=file_path, tofile=file_path, lineterm="",
        ))
```

- [ ] **Step 4: Run tests**

Run: `pytest tests/test_files.py -v`
Expected: 6 passed

- [ ] **Step 5: Commit**

```bash
git add tools/files.py tests/test_files.py
git commit -m "feat: WriteTool and EditTool with unique-match search-replace"
```

---

### Task 5: GrepTool + GlobTool

**Files:**
- Create: `tools/search.py`
- Create: `tests/test_search.py`

- [ ] **Step 1: Write failing tests**

```python
import os


def test_grep_finds_pattern(tmp_path):
    from tools.search import GrepTool

    (tmp_path / "a.py").write_text("def hello():\n    pass\n")
    (tmp_path / "b.py").write_text("def world():\n    pass\n")
    tool = GrepTool()
    result = tool.execute(pattern="hello", path=str(tmp_path))
    assert "a.py" in result
    assert "hello" in result
    assert "b.py" not in result


def test_grep_max_results(tmp_path):
    from tools.search import GrepTool

    f = tmp_path / "big.txt"
    f.write_text("\n".join(f"match line {i}" for i in range(100)))
    tool = GrepTool()
    result = tool.execute(pattern="match", path=str(tmp_path))
    lines = [l for l in result.strip().splitlines() if l.strip()]
    assert len(lines) <= 51  # 50 results + possible truncation message


def test_grep_no_matches(tmp_path):
    from tools.search import GrepTool

    (tmp_path / "a.txt").write_text("nothing here")
    tool = GrepTool()
    result = tool.execute(pattern="zzzzz", path=str(tmp_path))
    assert "no matches" in result.lower() or result.strip() == ""


def test_glob_finds_files(tmp_path):
    from tools.search import GlobTool

    (tmp_path / "a.py").write_text("")
    (tmp_path / "b.txt").write_text("")
    (tmp_path / "sub").mkdir()
    (tmp_path / "sub" / "c.py").write_text("")
    tool = GlobTool()
    result = tool.execute(pattern=str(tmp_path / "**" / "*.py"))
    assert "a.py" in result
    assert "c.py" in result
    assert "b.txt" not in result


def test_glob_max_results(tmp_path):
    from tools.search import GlobTool

    for i in range(150):
        (tmp_path / f"f{i}.txt").write_text("")
    tool = GlobTool()
    result = tool.execute(pattern=str(tmp_path / "*.txt"))
    lines = [l for l in result.strip().splitlines() if l.strip()]
    assert len(lines) <= 101  # 100 + possible truncation
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_search.py -v`
Expected: FAIL — `tools.search` does not exist

- [ ] **Step 3: Write tools/search.py**

```python
import os
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
            # Support absolute patterns by splitting at the glob part
            p = Path(pattern)
            # Use the parent as anchor if pattern is absolute
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
```

- [ ] **Step 4: Run tests**

Run: `pytest tests/test_search.py -v`
Expected: 5 passed

- [ ] **Step 5: Register new tools in main.py**

Add imports and registration in `main.py`:

```python
from tools.files import ReadTool, WriteTool, EditTool
from tools.search import GrepTool, GlobTool
```

Update `build_tool_registry`:

```python
def build_tool_registry(permission_checker=None):
    registry = ToolRegistry(permission_checker=permission_checker)
    for tool_cls in [ReadTool, WriteTool, EditTool, BashTool, GrepTool, GlobTool]:
        registry.register(tool_cls())
    return registry
```

- [ ] **Step 6: Commit**

```bash
git add tools/search.py tests/test_search.py main.py
git commit -m "feat: GrepTool (rg + fallback) and GlobTool"
```

---

### Task 6: TodoTool + System Prompt Builder

**Files:**
- Create: `tools/todo.py`
- Create: `prompt.py`
- Create: `tests/test_todo.py`
- Create: `tests/test_prompt.py`

- [ ] **Step 1: Write failing tests for TodoTool**

```python
def test_todo_write_creates_list():
    from tools.todo import TodoTool

    tool = TodoTool()
    result = tool.execute(todos=[
        {"content": "Read the code", "status": "completed"},
        {"content": "Fix the bug", "status": "in_progress"},
        {"content": "Write tests", "status": "pending"},
    ])
    assert "Read the code" in result
    assert "Fix the bug" in result
    assert "Write tests" in result


def test_todo_write_replaces_list():
    from tools.todo import TodoTool

    tool = TodoTool()
    tool.execute(todos=[{"content": "old task", "status": "pending"}])
    assert len(tool.todos) == 1
    tool.execute(todos=[
        {"content": "new task 1", "status": "pending"},
        {"content": "new task 2", "status": "pending"},
    ])
    assert len(tool.todos) == 2
    assert tool.todos[0]["content"] == "new task 1"


def test_todo_get_todos():
    from tools.todo import TodoTool

    tool = TodoTool()
    tool.execute(todos=[{"content": "task", "status": "pending"}])
    assert tool.todos == [{"content": "task", "status": "pending"}]
```

- [ ] **Step 2: Write failing tests for prompt.py**

```python
def test_build_system_prompt_minimal():
    from prompt import build_system_prompt, BASE_PROMPT

    result = build_system_prompt()
    assert BASE_PROMPT in result


def test_build_system_prompt_with_cwd():
    from prompt import build_system_prompt

    result = build_system_prompt(cwd="/my/project")
    assert "/my/project" in result


def test_build_system_prompt_with_todos():
    from prompt import build_system_prompt

    todos = [
        {"content": "Fix bug", "status": "in_progress"},
        {"content": "Write docs", "status": "pending"},
    ]
    result = build_system_prompt(todos=todos)
    assert "Fix bug" in result
    assert "Write docs" in result


def test_build_system_prompt_with_skills():
    from prompt import build_system_prompt

    skills = [{"name": "git-expert", "content": "You know git well."}]
    result = build_system_prompt(skills=skills)
    assert "git-expert" in result
    assert "You know git well." in result
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `pytest tests/test_todo.py tests/test_prompt.py -v`
Expected: FAIL — modules don't exist

- [ ] **Step 4: Write tools/todo.py**

```python
from tools import BaseTool

STATUS_ICONS = {
    "pending": "⬜",
    "in_progress": "\U0001f504",
    "completed": "✅",
}


class TodoTool(BaseTool):
    name = "todo_write"
    description = "Update the task list. Replaces the entire list with the provided todos."
    parameters = {
        "todos": {
            "type": "array",
            "description": "List of todo items",
            "items": {
                "type": "object",
                "properties": {
                    "content": {"type": "string"},
                    "status": {"type": "string", "enum": ["pending", "in_progress", "completed"]},
                },
                "required": ["content", "status"],
            },
        },
    }

    def __init__(self):
        self.todos = []

    def execute(self, todos) -> str:
        self.todos = todos
        return self._format()

    def _format(self) -> str:
        if not self.todos:
            return "(no tasks)"
        lines = []
        for todo in self.todos:
            icon = STATUS_ICONS.get(todo["status"], "?")
            lines.append(f"{icon} {todo['content']}")
        return "\n".join(lines)
```

- [ ] **Step 5: Write prompt.py**

```python
from pathlib import Path

BASE_PROMPT = """You are Tiny Claude Code, an AI coding assistant.
You help users with software engineering tasks using the tools available to you.
Always read files before editing. Use todo_write to plan multi-step tasks."""


def build_system_prompt(cwd=None, todos=None, skills=None):
    parts = [BASE_PROMPT]
    if cwd:
        parts.append(f"Working directory: {cwd}")
    if todos:
        parts.append(f"Current tasks:\n{_format_todos(todos)}")
    if skills:
        for skill in skills:
            parts.append(f"--- Skill: {skill['name']} ---\n{skill['content']}")
    return "\n\n".join(parts)


def _format_todos(todos):
    icons = {"pending": "⬜", "in_progress": "\U0001f504", "completed": "✅"}
    lines = []
    for t in todos:
        icon = icons.get(t["status"], "?")
        lines.append(f"{icon} {t['content']}")
    return "\n".join(lines)


def load_skill(name, skills_dir=".tiny-claude-skills"):
    path = Path(skills_dir) / f"{name}.md"
    if not path.exists():
        return None
    return {"name": name, "content": path.read_text()}
```

- [ ] **Step 6: Run tests**

Run: `pytest tests/test_todo.py tests/test_prompt.py -v`
Expected: 7 passed

- [ ] **Step 7: Register TodoTool in main.py and wire system prompt callable**

Update `main.py` imports:

```python
from tools.todo import TodoTool
from prompt import build_system_prompt
```

Update `build_tool_registry` to include `TodoTool`:

```python
def build_tool_registry(permission_checker=None):
    registry = ToolRegistry(permission_checker=permission_checker)
    todo_tool = TodoTool()
    for tool in [ReadTool(), WriteTool(), EditTool(), BashTool(), GrepTool(), GlobTool(), todo_tool]:
        registry.register(tool)
    return registry, todo_tool
```

Update `main()` to use callable system prompt:

```python
def main():
    config = load_config()
    provider = Provider(config.model)
    tools, todo_tool = build_tool_registry()

    def system_prompt():
        return build_system_prompt(cwd=os.getcwd(), todos=todo_tool.todos)

    agent = Agent(provider, tools, system_prompt)
    # ... rest of REPL unchanged
```

- [ ] **Step 8: Commit**

```bash
git add tools/todo.py prompt.py tests/test_todo.py tests/test_prompt.py main.py
git commit -m "feat: TodoTool + dynamic system prompt builder"
```

---

### Task 7: Streaming + Rich Rendering

**Files:**
- Create: `render.py`
- Modify: `main.py` — switch from sync to streaming, add Rich rendering

- [ ] **Step 1: Write render.py**

```python
import json

from rich.console import Console
from rich.markdown import Markdown
from rich.panel import Panel
from rich.syntax import Syntax
from rich.text import Text

console = Console()


def render_chunk(chunk):
    if chunk["type"] == "text":
        console.print(chunk["text"], end="", highlight=False)
    elif chunk["type"] == "tool_call":
        console.print()  # newline after text
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
            # Indent tool results
            for line in content.splitlines()[:30]:
                console.print(f"  {line}", highlight=False)
            if content.count("\n") > 30:
                console.print(f"  [dim]... ({content.count(chr(10)) - 30} more lines)[/]")


def render_end():
    console.print()  # final newline
```

- [ ] **Step 2: Update main.py to use streaming + Rich**

Replace the REPL section in `main.py`:

```python
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
```

- [ ] **Step 3: Manual smoke test with streaming**

Run: `MODEL=anthropic/claude-sonnet-4-20250514 python main.py`
Type: `what files are in the current directory?`
Expected: Text streams in real-time, tool calls show in cyan panels, results are indented

- [ ] **Step 4: Commit**

```bash
git add render.py main.py
git commit -m "feat: streaming output + Rich rendering (panels, diffs, markdown)"
```

---

### Task 8: Context Management — 3-Layer Compression

**Files:**
- Create: `context.py`
- Create: `tests/test_context.py`

- [ ] **Step 1: Write failing tests**

```python
import json


def _make_messages(count, content_size=100):
    msgs = []
    for i in range(count):
        if i % 2 == 0:
            msgs.append({"role": "user", "content": f"message {i}"})
        else:
            msgs.append({
                "role": "user",
                "content": [
                    {"type": "tool_result", "tool_use_id": f"t{i}", "content": "x" * content_size}
                ],
            })
    return msgs


def test_no_compaction_under_threshold():
    from context import ContextManager

    class FakeProvider:
        pass

    cm = ContextManager(FakeProvider(), max_tokens=10000)
    msgs = _make_messages(4, content_size=50)
    original = [m.copy() for m in msgs]
    cm.maybe_compact(msgs)
    assert len(msgs) == len(original)


def test_snip_tool_results_at_70_percent():
    from context import ContextManager

    class FakeProvider:
        pass

    cm = ContextManager(FakeProvider(), max_tokens=500)
    # Create messages that push past 70% of 500 tokens (~125 tokens = ~500 chars)
    msgs = _make_messages(10, content_size=300)
    original_len = len(msgs)
    cm.maybe_compact(msgs)
    # Tool results in the middle should be snipped
    snipped = False
    for msg in msgs:
        if isinstance(msg.get("content"), list):
            for block in msg["content"]:
                if "snipped" in str(block.get("content", "")):
                    snipped = True
    assert snipped


def test_estimate_tokens():
    from context import ContextManager

    class FakeProvider:
        pass

    cm = ContextManager(FakeProvider(), max_tokens=1000)
    msgs = [{"role": "user", "content": "x" * 400}]
    tokens = cm.estimate_tokens(msgs)
    assert 80 < tokens < 200  # ~100 tokens for 400 chars


def test_compaction_preserves_head_and_tail():
    from context import ContextManager

    class FakeProvider:
        pass

    cm = ContextManager(FakeProvider(), max_tokens=500)
    msgs = _make_messages(20, content_size=200)
    first_msg = msgs[0].copy()
    last_msgs = [m.copy() for m in msgs[-5:]]
    cm.maybe_compact(msgs)
    assert msgs[0] == first_msg
    assert msgs[-5:] == last_msgs
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_context.py -v`
Expected: FAIL — `context` module does not exist

- [ ] **Step 3: Write context.py**

```python
import json


class ContextManager:
    def __init__(self, provider, max_tokens=100_000):
        self.provider = provider
        self.max_tokens = max_tokens

    def maybe_compact(self, messages):
        ratio = self.estimate_tokens(messages) / self.max_tokens
        if ratio < 0.70:
            return
        if ratio < 0.85:
            compacted = self._snip_tool_results(messages)
        elif ratio < 0.95:
            compacted = self._summarize_head(messages)
        else:
            compacted = self._collapse_all(messages)
        messages[:] = compacted

    def estimate_tokens(self, messages):
        return sum(len(json.dumps(m)) // 4 for m in messages)

    def _snip_tool_results(self, messages):
        keep_head, keep_tail = 3, 5
        if len(messages) <= keep_head + keep_tail:
            return messages
        result = messages[:keep_head]
        for msg in messages[keep_head:-keep_tail]:
            if isinstance(msg.get("content"), list):
                snipped = []
                for block in msg["content"]:
                    if (block.get("type") == "tool_result"
                            and len(str(block.get("content", ""))) > 200):
                        snipped.append({
                            **block,
                            "content": f"[snipped: {len(str(block['content']))} chars]",
                        })
                    else:
                        snipped.append(block)
                result.append({**msg, "content": snipped})
            else:
                result.append(msg)
        result.extend(messages[-keep_tail:])
        return result

    def _summarize_head(self, messages):
        mid = len(messages) // 2
        summary = self._summarize(messages[:mid])
        return [{"role": "user", "content": f"[Conversation summary]\n{summary}"}] + messages[mid:]

    def _collapse_all(self, messages):
        summary = self._summarize(messages)
        return [{"role": "user", "content": f"[Full conversation summary]\n{summary}"}]

    def _summarize(self, messages):
        response = self.provider.create(
            system="Summarize this conversation concisely. Focus on decisions, code changes, and current state.",
            messages=messages,
            tools=[],
        )
        return response.content[0]["text"]
```

- [ ] **Step 4: Run tests**

Run: `pytest tests/test_context.py -v`
Expected: 4 passed

- [ ] **Step 5: Wire ContextManager into main.py**

Add to `main.py` imports:

```python
from context import ContextManager
```

In `main()`, after creating the provider:

```python
    context_mgr = ContextManager(provider, max_tokens=config.max_tokens)
    # ...
    agent = Agent(provider, tools, system_prompt, pre_call=context_mgr.maybe_compact)
```

- [ ] **Step 6: Commit**

```bash
git add context.py tests/test_context.py main.py
git commit -m "feat: 3-layer context compression via pre_call hook"
```

---

### Task 9: Permission System

**Files:**
- Create: `permissions.py`
- Create: `permissions.yaml`
- Create: `tests/test_permissions.py`

- [ ] **Step 1: Write failing tests**

```python
import yaml
import tempfile
import os


def _write_config(tmp_path, config):
    path = tmp_path / "permissions.yaml"
    path.write_text(yaml.dump(config))
    return str(path)


def test_allow_by_default(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {"tools": {"read": {"default": "allow"}}})
    pc = PermissionChecker(config_path=cfg)
    allowed, reason = pc.check("read", {"file_path": "/any/path"})
    assert allowed is True


def test_deny_by_pattern(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {
        "tools": {"bash": {"default": "ask", "deny_patterns": ["rm -rf", "sudo"]}}
    })
    pc = PermissionChecker(config_path=cfg)
    allowed, reason = pc.check("bash", {"command": "sudo rm -rf /"})
    assert allowed is False
    assert "deny pattern" in reason


def test_deny_by_path(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {
        "tools": {"write": {"default": "ask", "deny_paths": ["/etc/"]}}
    })
    pc = PermissionChecker(config_path=cfg)
    allowed, reason = pc.check("write", {"file_path": "/etc/passwd"})
    assert allowed is False


def test_auto_allow_under_cwd(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {
        "tools": {"write": {"default": "ask"}}
    })
    pc = PermissionChecker(config_path=cfg, cwd="/home/user/project")
    allowed, reason = pc.check("write", {"file_path": "/home/user/project/foo.py"})
    assert allowed is True
    assert "cwd" in reason


def test_ask_calls_prompt_fn(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {
        "tools": {"bash": {"default": "ask"}}
    })
    responses = [True]
    pc = PermissionChecker(
        config_path=cfg,
        prompt_fn=lambda tool, args: responses.pop(0),
    )
    allowed, reason = pc.check("bash", {"command": "ls"})
    assert allowed is True


def test_unknown_tool_defaults_to_ask(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {"tools": {}})
    pc = PermissionChecker(
        config_path=cfg,
        prompt_fn=lambda tool, args: False,
    )
    allowed, reason = pc.check("unknown_tool", {})
    assert allowed is False
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_permissions.py -v`
Expected: FAIL — `permissions` module does not exist

- [ ] **Step 3: Write permissions.py**

```python
import json
import re

import yaml


class PermissionChecker:
    def __init__(self, config_path="permissions.yaml", cwd="/", prompt_fn=None):
        with open(config_path) as f:
            self.rules = yaml.safe_load(f) or {}
        self.cwd = cwd
        self.prompt_fn = prompt_fn or self._default_prompt

    def check(self, tool_name, args):
        rule = self.rules.get("tools", {}).get(tool_name, {"default": "ask"})
        if "deny_patterns" in rule:
            cmd = args.get("command", "")
            for pattern in rule["deny_patterns"]:
                if re.search(pattern, cmd):
                    return False, f"Command matches deny pattern: {pattern}"
        path = args.get("file_path", "")
        if path:
            for dp in rule.get("deny_paths", []):
                if path.startswith(dp):
                    return False, f"Path blocked: {dp}"
            if path.startswith(self.cwd):
                return True, "allowed (under cwd)"
        default = rule.get("default", "ask")
        if default == "allow":
            return True, "allowed"
        if default == "deny":
            return False, "denied by default"
        return self.prompt_fn(tool_name, args), "user decision"

    @staticmethod
    def _default_prompt(tool_name, args):
        display = f"{tool_name}({json.dumps(args, indent=None)[:100]})"
        response = input(f"Allow {display}? [y/N] ").strip().lower()
        return response in ("y", "yes")
```

- [ ] **Step 4: Write permissions.yaml**

```yaml
tools:
  read:
    default: allow
  write:
    default: ask
    deny_paths: ["/etc/", "/usr/", "/bin/"]
  edit:
    default: ask
    deny_paths: ["/etc/", "/usr/", "/bin/"]
  bash:
    default: ask
    deny_patterns:
      - "rm -rf"
      - "sudo"
      - "chmod 777"
      - "> /dev/"
      - "curl.*\\|.*sh"
  grep:
    default: allow
  glob:
    default: allow
  todo_write:
    default: allow
  subagent:
    default: allow
```

- [ ] **Step 5: Run tests**

Run: `pytest tests/test_permissions.py -v`
Expected: 6 passed

- [ ] **Step 6: Wire permissions into main.py**

Add to `main.py` imports:

```python
from permissions import PermissionChecker
from render import console
```

In `main()`, before `build_tool_registry`:

```python
    permissions = PermissionChecker(
        cwd=cwd,
        prompt_fn=lambda tool, args: console.input(
            f"[yellow]Allow {tool}({json.dumps(args)[:80]})?[/] [y/N] "
        ).strip().lower() in ("y", "yes"),
    )
    tools, todo_tool = build_tool_registry(permission_checker=permissions)
```

Add `import json` to main.py imports.

- [ ] **Step 7: Commit**

```bash
git add permissions.py permissions.yaml tests/test_permissions.py main.py
git commit -m "feat: YAML permission system with cwd-aware auto-allow"
```

---

### Task 10: Session Persistence + Slash Commands

**Files:**
- Create: `session.py`
- Create: `tests/test_session.py`
- Modify: `main.py` — add slash command handling

- [ ] **Step 1: Write failing tests for SessionManager**

```python
def test_save_and_resume(tmp_path):
    from session import SessionManager

    sm = SessionManager(session_dir=str(tmp_path))
    msgs = [{"role": "user", "content": "hi"}]
    todos = [{"content": "task 1", "status": "pending"}]
    sm.save("test-session", msgs, todos)
    loaded_msgs, loaded_todos = sm.resume("test-session")
    assert loaded_msgs == msgs
    assert loaded_todos == todos


def test_list_sessions(tmp_path):
    from session import SessionManager

    sm = SessionManager(session_dir=str(tmp_path))
    sm.save("alpha", [], [])
    sm.save("beta", [], [])
    sessions = sm.list_sessions()
    assert sessions == ["alpha", "beta"]


def test_resume_nonexistent(tmp_path):
    from session import SessionManager

    sm = SessionManager(session_dir=str(tmp_path))
    try:
        sm.resume("nonexistent")
        assert False, "Should have raised"
    except FileNotFoundError:
        pass
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_session.py -v`
Expected: FAIL — `session` module does not exist

- [ ] **Step 3: Write session.py**

```python
import json
from datetime import datetime
from pathlib import Path


class SessionManager:
    def __init__(self, session_dir=".tiny-claude-sessions"):
        self.dir = Path(session_dir)
        self.dir.mkdir(exist_ok=True)

    def save(self, name, messages, todos):
        data = {
            "messages": messages,
            "todos": todos,
            "saved_at": datetime.now().isoformat(),
        }
        (self.dir / f"{name}.json").write_text(json.dumps(data, indent=2))

    def resume(self, name):
        path = self.dir / f"{name}.json"
        if not path.exists():
            raise FileNotFoundError(f"Session '{name}' not found")
        data = json.loads(path.read_text())
        return data["messages"], data.get("todos", [])

    def list_sessions(self):
        return sorted(p.stem for p in self.dir.glob("*.json"))
```

- [ ] **Step 4: Run tests**

Run: `pytest tests/test_session.py -v`
Expected: 3 passed

- [ ] **Step 5: Add slash commands to main.py**

Add to `main.py`:

```python
from session import SessionManager


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
```

Update the REPL loop's slash command handler call:

```python
        if user_input.startswith("/"):
            handle_slash_command(user_input, agent, todo_tool, session_mgr, context_mgr, provider)
            continue
```

And add `session_mgr = SessionManager()` and `context_mgr` references in `main()`.

- [ ] **Step 6: Manual smoke test**

Run: `python main.py`
Type: `/help`, `/save test`, `/sessions`, `/resume test`
Expected: Each command works correctly

- [ ] **Step 7: Commit**

```bash
git add session.py tests/test_session.py main.py
git commit -m "feat: session persistence + slash commands (/save, /resume, /model, etc.)"
```

---

### Task 11: Subagent Tool

**Files:**
- Create: `tools/subagent.py`
- Create: `tests/test_subagent.py`
- Modify: `main.py` — register SubagentTool

- [ ] **Step 1: Write failing test**

```python
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
    # Parent messages should NOT contain child messages
    assert len(agent.messages) == 1
    assert agent.messages[0]["content"] == "parent context"
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_subagent.py -v`
Expected: FAIL — `tools.subagent` does not exist

- [ ] **Step 3: Write tools/subagent.py**

```python
from tools import BaseTool


class SubagentTool(BaseTool):
    name = "subagent"
    description = "Spawn a child agent with isolated context to handle a subtask. The child cannot see the parent conversation."
    parameters = {
        "prompt": {"type": "string", "description": "Task description for the child agent"},
    }

    def __init__(self, parent_agent):
        self.parent = parent_agent

    def execute(self, prompt) -> str:
        return self.parent.spawn_subagent(prompt)
```

- [ ] **Step 4: Run tests**

Run: `pytest tests/test_subagent.py -v`
Expected: 2 passed

- [ ] **Step 5: Register SubagentTool in main.py**

This tool needs a reference to the agent, which creates a circular dependency (agent needs tools, subagent tool needs agent). Resolve by registering it after agent creation:

```python
from tools.subagent import SubagentTool
```

In `main()`, after creating the agent:

```python
    subagent_tool = SubagentTool(agent)
    tools.register(subagent_tool)
```

- [ ] **Step 6: Run full test suite**

Run: `pytest tests/ -v`
Expected: All tests pass

- [ ] **Step 7: Commit**

```bash
git add tools/subagent.py tests/test_subagent.py main.py
git commit -m "feat: SubagentTool — child agent with isolated context"
```

---

### Task 12: Final Integration + End-to-End Smoke Test

**Files:**
- Modify: `main.py` — final assembly check
- Create: `tests/test_integration.py`

- [ ] **Step 1: Write integration test**

```python
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
    """All 7 tools register and produce valid schemas."""
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
    """Simulate: model reads a file, then edits it."""
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
```

- [ ] **Step 2: Run integration tests**

Run: `pytest tests/test_integration.py -v`
Expected: 3 passed

- [ ] **Step 3: Run full test suite**

Run: `pytest tests/ -v`
Expected: All tests pass (should be ~40+ tests total)

- [ ] **Step 4: Manual end-to-end test**

Run: `MODEL=anthropic/claude-sonnet-4-20250514 python main.py`
Type: `create a file /tmp/tiny-test.py with a function that adds two numbers, then write a test for it`
Expected: Agent uses todo_write to plan, write to create the file, then write again for the test file. Both files should be valid Python.

- [ ] **Step 5: Commit**

```bash
git add tests/test_integration.py
git commit -m "test: integration tests — full registry, read-edit flow, config"
```

- [ ] **Step 6: Update Dockerfile**

Verify the Dockerfile matches the current deps:

```dockerfile
FROM python:3.11-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    ripgrep \
    && rm -rf /var/lib/apt/lists/*

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

WORKDIR /workspace

COPY . /app/
ENV PYTHONPATH=/app

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["python", "/app/main.py"]
```

- [ ] **Step 7: Final commit**

```bash
git add Dockerfile entrypoint.sh
git commit -m "chore: update Dockerfile for final project structure"
```
