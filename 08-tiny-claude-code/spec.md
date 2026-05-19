# Tiny Claude Code — Specification

> A ~1,500 LoC toy Claude Code clone. Learn by building something that actually works.
>
> References: [claude-code-from-scratch](https://github.com/FareedKhan-dev/claude-code-from-scratch), [CoreCoder](https://github.com/he-yufeng/CoreCoder), [learn-claude-code](https://github.com/Humbertzhang/learn-claude-code), [Aider](https://github.com/paul-gauthier/aider)

## Core Philosophy

> The model is the only source of decisions — the harness never branches on model output, it only executes what the model requests. Tools are the only interface between the model and the world.

---

## Architecture

Seven layers, each with one job and a clear interface to its neighbors:

```
┌─────────────────────────────────────┐
│  7. CLI / REPL                      │  User-facing: input, output, rendering
├─────────────────────────────────────┤
│  6. Safety                          │  Permissions, dangerous-command detection
├─────────────────────────────────────┤
│  5. Context Management              │  Token tracking, 3-layer compression
├─────────────────────────────────────┤
│  4. Persistence                     │  Session save/resume, todo state
├─────────────────────────────────────┤
│  3. Tool System                     │  BaseTool class, registry, dispatch
├─────────────────────────────────────┤
│  2. Provider Layer (LiteLLM)        │  Model calls, streaming, retry
├─────────────────────────────────────┤
│  1. Agent Loop                      │  The core while-True — never changes
└─────────────────────────────────────┘
```

**Key constraint:** The agent loop (Layer 1) only knows about layers 2 and 3. It calls the provider to get responses, dispatches tools, and loops. Everything else — permissions, compression, persistence, UI rendering — wraps around the loop from the outside via hooks. The sync loop stays under 25 lines; the streaming variant is a separate parallel path with its own complexity.

### File Layout (~1,500 LoC budget)

```
tiny-claude-code/
├── main.py              # 120 LoC  CLI entry, REPL, arg parsing
├── agent.py             #  80 LoC  Agent loop + subagent dispatch
├── provider.py          # 100 LoC  LiteLLM wrapper, streaming, retry
├── context.py           # 120 LoC  Token estimation, 3-layer compression
├── permissions.py       # 100 LoC  YAML rules, permission checks
├── session.py           #  80 LoC  Save/resume to JSON
├── prompt.py            #  80 LoC  Dynamic system prompt builder
├── render.py            # 100 LoC  Rich markdown rendering, spinner, diffs
├── config.py            #  40 LoC  Env/config loading
├── tools/
│   ├── __init__.py      #  60 LoC  BaseTool, ToolRegistry, dispatch
│   ├── bash.py          #  80 LoC  Shell execution
│   ├── files.py         # 120 LoC  read, write, edit
│   ├── search.py        #  80 LoC  grep + glob
│   ├── todo.py          #  50 LoC  Todo list management
│   └── subagent.py      #  60 LoC  Child agent spawning
├── permissions.yaml     #          Tool permission rules
└── requirements.txt     #          litellm, pyyaml, rich, prompt_toolkit
```

---

## Layer 1: Agent Loop

The loop that never changes. Adding features only adds tools and context management — the sync loop stays under 25 lines.

```
┌──────────┐    ┌──────────┐    ┌──────────┐
│  User    │───►│ messages │───►│   LLM    │
│  Input   │    │   []     │    │  (API)   │
└──────────┘    └──────────┘    └────┬─────┘
                                     │
                       stop_reason == "tool_use"?
                           /            \
                         YES             NO
                          │               │
                          ▼               ▼
                 ┌──────────────┐   ┌──────────┐
                 │  EXECUTE     │   │  RETURN   │
                 │  TOOLS       │   │  TEXT     │
                 │  (dispatch)  │   │  (done)   │
                 └──────┬───────┘   └──────────┘
                        │
                 Append tool_results
                 to messages[]
                        │
                        └──────► LOOP BACK
```

### Agent class

```python
class Agent:
    def __init__(self, provider, tool_registry, system_prompt,
                 pre_call=None, max_turns=100):
        self.provider = provider
        self.tools = tool_registry
        self._system = system_prompt  # str or callable() -> str
        self.pre_call = pre_call      # hook: (messages) -> None (mutates in place)
        self.max_turns = max_turns
        self.messages = []            # Agent owns conversation state

    @property
    def system(self):
        return self._system() if callable(self._system) else self._system

    def run(self):
        """Core loop — call model, dispatch tools, repeat until text response."""
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

        raise TurnLimitExceeded(turn)

    def run_stream(self):
        """Streaming variant — yields normalized chunks for rendering.

        Yields dicts with type: "text", "tool_call", or "tool_result".
        The REPL consumes these for real-time display.
        """
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
                # Provider yields normalized chunks — never raw wire format
                if chunk["type"] == "text":
                    content.append(chunk)
                    yield chunk
                elif chunk["type"] == "tool_use":
                    content.append(chunk)
                    tool_calls.append(
                        SimpleNamespace(name=chunk["name"], id=chunk["id"], input=chunk["input"])
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
```

### Subagent

A subagent is just another Agent with its own `messages[]`. The parent only sees the final text result — no context contamination.

```python
def spawn_subagent(self, prompt, tool_subset=None):
    """Spawn a child agent with isolated context."""
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
    """Get the text from the last assistant message."""
    for msg in reversed(messages):
        if msg["role"] == "assistant":
            for block in msg["content"]:
                if block["type"] == "text":
                    return block["text"]
    return ""
```

### Design decisions

- **Class, not function:** Subagents share provider/tools but isolate state. Agent owns `messages` as instance state — no risk of caller/callee list divergence after compaction.
- **`system` as callable:** Accepts a string or `() -> str`. Rebuilt each turn so todos, skills, and nag reminders stay current.
- **`pre_call` hook:** `(messages: list) -> None` — mutates the list in place (e.g., compaction replaces entries via `messages[:] = compacted`). Single extension point; compression and logging compose here without the loop knowing.
- **`max_turns`:** Prevents infinite loops (gotchas #1). Default 100 for main agent, 50 for subagents.
- **Errors → tool_results:** Tool errors are returned as strings, never crash the loop (gotchas #2).

---

## Layer 2: Provider (LiteLLM)

LiteLLM translates between providers automatically — you write against one interface and get 100+ providers. Model set via env var: `MODEL=anthropic/claude-sonnet-4-20250514` or `MODEL=openai/gpt-4o` or `MODEL=ollama/llama3`.

```python
from litellm import completion

class Provider:
    def __init__(self, model="anthropic/claude-sonnet-4-20250514"):
        self.model = model

    def create(self, system, messages, tools):
        """Synchronous model call. Returns normalized response."""
        response = completion(
            model=self.model,
            messages=[{"role": "system", "content": system}] + messages,
            tools=self._to_litellm_tools(tools),
        )
        return self._normalize(response)

    def stream(self, system, messages, tools):
        """Streaming variant — yields normalized chunks in internal format.

        Handles the complexity of OpenAI-style streaming deltas so the
        Agent never sees wire format. Yields complete dicts:
          - {"type": "text", "text": "..."}
          - {"type": "tool_use", "id": "...", "name": "...", "input": {...}}
        Tool use blocks are accumulated from deltas and yielded once complete.
        """
        raw = completion(
            model=self.model,
            messages=[{"role": "system", "content": system}] + messages,
            tools=self._to_litellm_tools(tools),
            stream=True,
        )
        # Accumulate tool call fragments: index -> {id, name, arguments_json}
        tc_accum = {}
        for chunk in raw:
            delta = chunk.choices[0].delta
            if delta.content:
                yield {"type": "text", "text": delta.content}
            for tc_delta in (delta.tool_calls or []):
                idx = tc_delta.index
                if idx not in tc_accum:
                    tc_accum[idx] = {"id": tc_delta.id, "name": tc_delta.function.name, "args": ""}
                else:
                    tc_accum[idx]["args"] += tc_delta.function.arguments or ""

        # Yield completed tool calls after stream ends
        for tc in tc_accum.values():
            yield {
                "type": "tool_use",
                "id": tc["id"],
                "name": tc["name"],
                "input": json.loads(tc["args"]),
            }

    def _normalize(self, response):
        """Normalize sync LiteLLM response to internal format."""
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
        return SimpleNamespace(content=content, stop_reason=stop, tool_calls=[
            SimpleNamespace(name=b["name"], id=b["id"], input=b["input"])
            for b in content if b["type"] == "tool_use"
        ])

    def _to_litellm_tools(self, tools):
        """Translate internal tool schemas (Anthropic-style) to OpenAI-style for LiteLLM."""
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

### Design decisions

- **Normalization:** Both `create()` and `stream()` translate LiteLLM's OpenAI-style wire format to an internal format matching Anthropic-style content blocks. The agent loop never sees `choices[0].delta` or `function.arguments` — only `{"type": "text"}` and `{"type": "tool_use"}` dicts.
- **Stream accumulation:** `stream()` handles the complexity of OpenAI streaming tool call deltas (fragmented JSON arriving across multiple chunks) and yields complete, parsed tool_use blocks only after the stream ends. Text chunks yield immediately.
- **Retry:** Uses LiteLLM's built-in retry with exponential backoff — no hand-rolled retry loop.
- **Streaming vs sync:** Two methods. The REPL uses `stream()`, subagents use `create()`.

---

## Layer 3: Tool System

Class-based tools (from CoreCoder). Each tool is self-contained: name, schema, and handler in one class.

### BaseTool

```python
from abc import ABC, abstractmethod

class BaseTool(ABC):
    name: str
    description: str
    parameters: dict  # JSON Schema properties

    @abstractmethod
    def execute(self, **kwargs) -> str:
        """Run the tool. Always returns a string — even errors."""

    def definition(self) -> dict:
        """Auto-generate the API tool schema."""
        return {
            "name": self.name,
            "description": self.description,
            "input_schema": {
                "type": "object",
                "properties": self.parameters,
                "required": list(self.parameters.keys()),
            }
        }
```

### ToolRegistry

```python
class ToolRegistry:
    def __init__(self):
        self._tools: dict[str, BaseTool] = {}

    def register(self, tool: BaseTool):
        self._tools[tool.name] = tool

    def definitions(self) -> list[dict]:
        """All tool schemas for the API call."""
        return [t.definition() for t in self._tools.values()]

    def execute_all(self, tool_calls) -> list[dict]:
        """Execute tool calls sequentially. Return results for the model."""
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

### Tool Inventory (7 tools)

| Tool | Parameters | Key behavior |
|------|-----------|-------------|
| `read` | `file_path: str` | Returns file content with `cat -n` style line numbers. Truncates at 8,000 chars with `"[truncated]"` marker. |
| `write` | `file_path: str, content: str` | Creates or overwrites file. Creates parent directories. Returns byte count written. |
| `edit` | `file_path: str, old_string: str, new_string: str` | Unique-match search-replace. Errors on 0 matches ("not found") or >1 matches ("provide more context"). Returns unified diff. |
| `bash` | `command: str` | `subprocess.run(command, shell=True, timeout=30)`. Captures stdout+stderr. Truncates output at 8,000 chars. Permission check delegated to Layer 6. |
| `grep` | `pattern: str, path: str` | Runs `rg` (ripgrep) if available, falls back to Python `re` walk. Returns `file:line: match` format. Max 50 results. |
| `glob` | `pattern: str` | `pathlib.Path.glob()`. Returns sorted file list. Max 100 results. |
| `todo_write` | `todos: list[{content, status}]` | Replaces the global todo list. Status: `pending`, `in_progress`, `completed`. Formats with status icons (⬜🔄✅). |

### Subagent Tool

```python
class SubagentTool(BaseTool):
    name = "subagent"
    description = "Spawn a child agent with isolated context to handle a subtask."
    parameters = {
        "prompt": {"type": "string", "description": "Task for the child agent"},
    }

    def __init__(self, parent_agent):
        self.parent = parent_agent

    def execute(self, prompt):
        return self.parent.spawn_subagent(prompt)
```

### Edit tool detail

The edit pattern is Claude Code's most important UX innovation. Requiring a unique match prevents accidental multi-edits. The model learns to provide enough surrounding context.

```python
class EditTool(BaseTool):
    name = "edit"
    description = "Replace old_string with new_string in a file. old_string must match exactly once."
    parameters = {
        "file_path": {"type": "string", "description": "Absolute path to file"},
        "old_string": {"type": "string", "description": "Exact text to find (must be unique in file)"},
        "new_string": {"type": "string", "description": "Replacement text"},
    }

    def execute(self, file_path, old_string, new_string):
        content = Path(file_path).read_text()
        count = content.count(old_string)

        if count == 0:
            return f"ERROR: old_string not found in {file_path}"
        if count > 1:
            return f"ERROR: Found {count} matches. Provide more surrounding context to make the match unique."

        new_content = content.replace(old_string, new_string, 1)
        Path(file_path).write_text(new_content)
        return self._diff(old_string, new_string, file_path)

    @staticmethod
    def _diff(old, new, path):
        import difflib
        return "\n".join(difflib.unified_diff(
            old.splitlines(), new.splitlines(),
            fromfile=path, tofile=path, lineterm=""
        ))
```

---

## Layer 4: Persistence

### Session save/resume

```python
class SessionManager:
    def __init__(self, session_dir=".tiny-claude-sessions"):
        self.dir = Path(session_dir)
        self.dir.mkdir(exist_ok=True)

    def save(self, name: str, messages: list, todos: list):
        data = {
            "messages": messages,
            "todos": todos,
            "saved_at": datetime.now().isoformat(),
        }
        (self.dir / f"{name}.json").write_text(json.dumps(data, indent=2))

    def resume(self, name: str) -> tuple[list, list]:
        data = json.loads((self.dir / f"{name}.json").read_text())
        return data["messages"], data.get("todos", [])

    def list_sessions(self) -> list[str]:
        return sorted(p.stem for p in self.dir.glob("*.json"))
```

### Skill loading

Skills are markdown files that get concatenated into the system prompt. No routing, no triggers, no registry — just content injection.

```python
def load_skill(name: str, skills_dir=".tiny-claude-skills") -> dict | None:
    path = Path(skills_dir) / f"{name}.md"
    if not path.exists():
        return None
    return {"name": name, "content": path.read_text()}
```

### System prompt builder

```python
BASE_PROMPT = """You are Tiny Claude Code, an AI coding assistant.
You help users with software engineering tasks using the tools available to you.
Always read files before editing. Use todo_write to plan multi-step tasks."""

def build_system_prompt(cwd=None, todos=None, skills=None):
    parts = [BASE_PROMPT]
    if cwd:
        parts.append(f"Working directory: {cwd}")
    if todos:
        parts.append(f"Current tasks:\n{format_todos(todos)}")
    if skills:
        for skill in skills:
            parts.append(f"--- Skill: {skill['name']} ---\n{skill['content']}")
    return "\n\n".join(parts)
```

**Todo nag:** If >5 turns pass since the last `todo_write` call and there are incomplete todos, append a reminder to the system prompt: `"[Reminder: You have incomplete tasks. Update your todo list.]"`

---

## Layer 5: Context Management

Three-layer progressive compression. Each layer is more aggressive and more expensive. Thresholds are percentages of `max_tokens`.

| Threshold | Layer | Action | Cost |
|-----------|-------|--------|------|
| <70% | None | Do nothing | Free |
| 70–85% | 1: Snip | Replace old tool_result content with `"[snipped: N chars]"`. Keep first 3 and last 5 messages intact. | Free |
| 85–95% | 2: Summarize | Send the first half of conversation to LLM: "Summarize this conversation concisely." Replace with summary. | 1 API call |
| 95%+ | 3: Collapse | Summarize entire conversation. Restart with `[{"role": "user", "content": summary}]`. | 1 API call |

```python
class ContextManager:
    def __init__(self, provider, max_tokens=100_000):
        self.provider = provider  # needed for Layer 2/3 summarization calls
        self.max_tokens = max_tokens

    def maybe_compact(self, messages) -> None:
        """Called via pre_call hook before each model call. Mutates in place."""
        ratio = self.estimate_tokens(messages) / self.max_tokens

        if ratio < 0.70:
            return
        if ratio < 0.85:
            compacted = self._snip_tool_results(messages)
        elif ratio < 0.95:
            compacted = self._summarize_head(messages)
        else:
            compacted = self._collapse_all(messages)

        messages[:] = compacted  # replace contents in place

    def estimate_tokens(self, messages) -> int:
        """~1 token per 4 characters. Rough but sufficient."""
        return sum(len(json.dumps(m)) // 4 for m in messages)

    def _snip_tool_results(self, messages):
        """Layer 1: Replace old tool_result content with placeholder."""
        keep_head, keep_tail = 3, 5
        result = messages[:keep_head]
        for msg in messages[keep_head:-keep_tail]:
            if isinstance(msg.get("content"), list):
                snipped = []
                for block in msg["content"]:
                    if block.get("type") == "tool_result" and len(str(block.get("content", ""))) > 200:
                        snipped.append({**block, "content": f"[snipped: {len(str(block['content']))} chars]"})
                    else:
                        snipped.append(block)
                result.append({**msg, "content": snipped})
            else:
                result.append(msg)
        result.extend(messages[-keep_tail:])
        return result

    def _summarize_head(self, messages):
        """Layer 2: LLM-summarize the first half, keep the rest."""
        mid = len(messages) // 2
        summary = self._summarize(messages[:mid])
        return [{"role": "user", "content": f"[Conversation summary]\n{summary}"}] + messages[mid:]

    def _collapse_all(self, messages):
        """Layer 3: Summarize everything, restart fresh."""
        summary = self._summarize(messages)
        return [{"role": "user", "content": f"[Full conversation summary]\n{summary}"}]

    def _summarize(self, messages):
        """Ask the provider to summarize a message list."""
        response = self.provider.create(
            system="Summarize this conversation concisely. Focus on decisions, code changes, and current state.",
            messages=messages,
            tools=[],
        )
        return response.content[0]["text"]
```

**Integration:** Plugs into the agent loop via `pre_call`:

```python
context_mgr = ContextManager(provider, max_tokens=100_000)
agent = Agent(provider, tools, system_prompt_fn, pre_call=context_mgr.maybe_compact)
```

---

## Layer 6: Safety — Permissions

YAML-based permission rules. Three decisions: `allow` (proceed silently), `deny` (block with error), `ask` (prompt user in the REPL).

### permissions.yaml

```yaml
# Paths under cwd are auto-allowed by PermissionChecker (no need to list them here).
# deny_paths are absolute — they override cwd auto-allow.
# deny_patterns are heuristic speed bumps, not a security boundary.
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

### PermissionChecker

```python
class PermissionChecker:
    def __init__(self, config_path="permissions.yaml", cwd="/", prompt_fn=None):
        with open(config_path) as f:
            self.rules = yaml.safe_load(f)
        self.cwd = cwd           # auto-allow paths under cwd
        self.prompt_fn = prompt_fn or self._default_prompt

    def check(self, tool_name: str, args: dict) -> tuple[bool, str]:
        """Returns (allowed, reason)."""
        rule = self.rules.get("tools", {}).get(tool_name, {"default": "ask"})

        # Deny patterns (bash commands) — heuristic speed bump, not security boundary
        if "deny_patterns" in rule:
            cmd = args.get("command", "")
            for pattern in rule["deny_patterns"]:
                if re.search(pattern, cmd):
                    return False, f"Command matches deny pattern: {pattern}"

        # Path-based rules (read/write/edit)
        path = args.get("file_path", "")
        if path:
            for dp in rule.get("deny_paths", []):
                if path.startswith(dp):
                    return False, f"Path blocked: {dp}"
            # Auto-allow paths under cwd
            if path.startswith(self.cwd):
                return True, "allowed (under cwd)"

        # Default
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

**Integration with ToolRegistry:** Pass the checker to `ToolRegistry` at construction. `execute_all()` calls `check()` before each `execute()`:

```python
class ToolRegistry:
    def __init__(self, permission_checker=None):
        self._tools: dict[str, BaseTool] = {}
        self._permissions = permission_checker

    def execute_all(self, tool_calls) -> list[dict]:
        results = []
        for call in tool_calls:
            tool = self._tools.get(call.name)
            if not tool:
                results.append({"type": "tool_result", "tool_use_id": call.id,
                                "content": f"ERROR: Unknown tool '{call.name}'"})
                continue

            # Permission gate
            if self._permissions:
                allowed, reason = self._permissions.check(call.name, call.input)
                if not allowed:
                    results.append({"type": "tool_result", "tool_use_id": call.id,
                                    "content": f"Permission denied: {reason}"})
                    continue

            try:
                output = tool.execute(**call.input)
            except Exception as e:
                output = f"ERROR: {e}"
            results.append({"type": "tool_result", "tool_use_id": call.id,
                            "content": str(output)})
        return results
```

---

## Layer 7: CLI / REPL

Interactive REPL using `prompt_toolkit` for input (history, multiline) and `rich` for output rendering.

### REPL structure

```python
from prompt_toolkit import PromptSession
from prompt_toolkit.history import FileHistory
from rich.console import Console
from rich.markdown import Markdown

console = Console()

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
    tools = build_tool_registry(permission_checker=permissions)  # see below
    context_mgr = ContextManager(provider, max_tokens=config.max_tokens)
    session_mgr = SessionManager()
    todo_list = []

    # System prompt rebuilds each turn — todos, skills, nag reminders stay current
    def system_prompt():
        return build_system_prompt(cwd=cwd, todos=todo_list)

    agent = Agent(provider, tools, system_prompt,
                  pre_call=context_mgr.maybe_compact)

    prompt_session = PromptSession(history=FileHistory(".tiny-claude-history"))

    console.print("[bold]Tiny Claude Code[/bold] — /help for commands, Ctrl+D to exit\n")

    while True:
        try:
            user_input = prompt_session.prompt(">>> ").strip()
        except (EOFError, KeyboardInterrupt):
            break

        if not user_input:
            continue

        if user_input.startswith("/"):
            handle_slash_command(user_input, agent, session_mgr)
            continue

        agent.messages.append({"role": "user", "content": user_input})

        # Stream response — agent owns messages, chunks are normalized
        for chunk in agent.run_stream():
            render_chunk(chunk)

def build_tool_registry(permission_checker=None):
    registry = ToolRegistry(permission_checker=permission_checker)
    for tool_cls in [ReadTool, WriteTool, EditTool, BashTool, GrepTool, GlobTool, TodoTool]:
        registry.register(tool_cls())
    return registry
```

### Slash commands

| Command | Action |
|---------|--------|
| `/help` | List available commands |
| `/compact` | Force context compression now |
| `/save [name]` | Save session to JSON |
| `/resume [name]` | Load a saved session |
| `/sessions` | List saved sessions |
| `/model [name]` | Switch LiteLLM model (e.g., `/model openai/gpt-4o`) |
| `/todo` | Display current todo list |
| `/clear` | Clear conversation history |
| `/exit` | Quit |

### Output rendering (Rich)

| Content | Rendering |
|---------|-----------|
| Model text | `rich.Markdown` — headers, lists, code blocks |
| Tool call | `rich.Panel` with tool name as title, syntax-highlighted args |
| Tool result | Indented text, truncated if long |
| Diff (from edit) | `rich.Syntax` with diff highlighting |
| Errors | Red `rich.Panel` |
| Tool execution | Spinner (`rich.Status`) while running |

### Streaming behavior

Text tokens render as they arrive (character by character). Tool use blocks accumulate silently during streaming, then execute and display results before the next model call. The user sees:

```
>>> fix the typo in main.py

I'll read the file first to find the typo.

┌─ read ──────────────────────────┐
│ file_path: "main.py"           │
└─────────────────────────────────┘
  1│ def main():
  2│     print("Helo, world!")     ← typo here
  ...

Found it. Let me fix "Helo" → "Hello".

┌─ edit ──────────────────────────┐
│ file_path: "main.py"           │
│ old_string: 'print("Helo, ...' │
│ new_string: 'print("Hello, ...'│
└─────────────────────────────────┘
  --- main.py
  +++ main.py
  -     print("Helo, world!")
  +     print("Hello, world!")

Fixed the typo on line 2.
```

---

## Gotchas & Mitigations

Extracted from field-testing the reference implementations. Each is addressed by a specific layer.

| # | Gotcha | Layer | Mitigation |
|---|--------|-------|------------|
| 1 | Infinite agent loops | 1 | `max_turns` limit (100 main, 50 subagent) |
| 2 | Tool call parsing failures | 3 | Never crash — return errors as `tool_result` strings |
| 3 | Context window overflow | 5 | 3-layer compression at 70/85/95% thresholds |
| 4 | Rate limiting (429) | 2 | LiteLLM built-in retry with exponential backoff |
| 5 | File edit race conditions | 3 | Sequential tool execution for writes |
| 6 | Prompt injection via file content | 1 | Wrap file content in XML tags, system prompt disclaimer |
| 7 | Shell injection | 3+6 | `shell=True` is inherently injectable; deny patterns are a heuristic speed bump (trivially bypassable). Real protection is the `ask` default — patterns just auto-deny common dangerous commands to reduce prompt fatigue |
| 8 | Subagent resource leaks | 1 | `max_turns` on child agents, no subprocess spawning |
| 9 | Multiple-match edit ambiguity | 3 | Error on >1 matches, require unique context |
| 10 | Large tool results | 3 | Truncate all tool output at 8,000 chars |
| 11 | Model stubbornness (same error 3x) | 1 | Inject correction message after 3 identical tool calls |
| 12 | Stream chunk ordering | 2 | Provider accumulates tool call deltas by index, yields complete tool_use blocks after stream ends |
| 13 | Overlong system prompt | 4 | Dynamic assembly — only include active todos and loaded skills |

---

## Development Roadmap

Each phase produces a runnable artifact. Build in order.

| Phase | Deliverable | ~LoC | Cumulative | What works after |
|-------|-------------|------|------------|-----------------|
| **1** | `agent.py` + `provider.py` + `main.py` — bare REPL, `read` + `bash` tools | 300 | 300 | Ask questions about files, run shell commands |
| **2** | `write`, `edit`, `grep`, `glob` tools | 280 | 580 | Read, search, and modify code |
| **3** | `todo_write` tool + nag reminder in system prompt | 80 | 660 | Plan multi-step tasks before executing |
| **4** | Streaming output + Rich rendering | 200 | 860 | Real-time token display, formatted panels |
| **5** | 3-layer `ContextManager` via `pre_call` hook | 120 | 980 | Long conversations without overflow |
| **6** | `PermissionChecker` + `permissions.yaml` | 140 | 1,120 | Dangerous commands blocked, user prompted |
| **7** | Session save/resume + skill loading + slash commands | 200 | 1,320 | Persist and restore work across restarts |
| **8** | `SubagentTool` — child agent with isolated context | 100 | 1,420 | Delegate subtasks without context contamination |

---

## Success Criteria

1. Agent loop calls LLM, dispatches tools, loops — with `max_turns` safety
2. Agent reads files, searches codebase, answers questions about code
3. Agent edits files with unique-match search-replace (error on 0 or >1 matches)
4. Agent plans with `todo_write` before multi-step execution
5. Streaming shows text tokens in real-time, tool calls display after execution
6. Context compression activates automatically at 70/85/95% thresholds
7. Permission system blocks dangerous bash commands, prompts for writes outside workspace
8. Sessions save and resume across restarts
9. Subagent completes isolated tasks without contaminating parent context
10. **End-to-end:** Agent completes "add a function to X.py and write tests for it"
