# Tiny Claude Code — Specification

> Based on: claude-code-from-scratch, CoreCoder, learn-claude-code

## References

| Project | Stars | Language | Description |
|---------|-------|----------|-------------|
| [claude-code-from-scratch](https://github.com/FareedKhan-dev/claude-code-from-scratch) | — | Python | 23-session progressive reverse-engineering of Claude Code |
| [CoreCoder](https://github.com/he-yufeng/CoreCoder) | — | Python | ~1,400 LoC distilled Claude Code blueprint |
| [learn-claude-code](https://github.com/Humbertzhang/learn-claude-code) | — | Python | 12-session progressive build from bash loop |
| [Aider](https://github.com/paul-gauthier/aider) | — | Python | Production AI coding agent (50K+ LoC reference) |

## Architecture: The Harness

**Core philosophy:**
> "The model is the only source of decisions — the harness never branches on model output, it only executes what the model requests. Tools are the only interface between the model and the world."

```
┌─────────────────────────────────────────────────────────────┐
│                     THE AGENT LOOP                           │
│                                                              │
│   ┌──────────┐    ┌──────────┐    ┌──────────┐             │
│   │  User    │───►│ messages │───►│   LLM    │             │
│   │  Input   │    │   []     │    │  (API)   │             │
│   └──────────┘    └──────────┘    └────┬─────┘             │
│                                        │                    │
│                          stop_reason == "tool_use"?         │
│                              /            \                 │
│                            YES             NO               │
│                             │               │               │
│                             ▼               ▼               │
│                    ┌──────────────┐   ┌──────────┐         │
│                    │  EXECUTE     │   │  RETURN  │         │
│                    │  TOOLS       │   │  TEXT    │         │
│                    │  (dispatch)  │   │  (done)  │         │
│                    └──────┬───────┘   └──────────┘         │
│                           │                                 │
│                    Append tool_results                      │
│                    to messages[]                            │
│                           │                                 │
│                           └──────► LOOP BACK                │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Feature Specification

### 1. The Core Agent Loop (Session 1)

**The minimal loop that never changes:**

```python
def agent_loop(messages, tools, system_prompt):
    """The core loop — everything else layers on top of this."""
    while True:
        response = client.messages.create(
            model=MODEL,
            system=system_prompt,
            messages=messages,
            tools=tools,
            max_tokens=4096,
        )
        messages.append({"role": "assistant", "content": response.content})

        if response.stop_reason != "tool_use":
            return response  # text response → done

        # Execute tool calls
        tool_results = []
        for block in response.content:
            if block.type == "tool_use":
                handler = TOOL_HANDLERS.get(block.name)
                output = handler(block.input) if handler else f"Unknown tool: {block.name}"
                tool_results.append({
                    "type": "tool_result",
                    "tool_use_id": block.id,
                    "content": output,
                })
        messages.append({"role": "user", "content": tool_results})
```

**Key insight:** This loop structure is universal. Adding features only adds tools and context management — the loop itself stays the same.

### 2. Tool Dispatch System (Session 2)

**Each tool is a typed, schema-validated function:**

```python
TOOL_REGISTRY = {}

def register_tool(name, description, parameters, handler):
    TOOL_REGISTRY[name] = {
        "definition": {
            "name": name,
            "description": description,
            "input_schema": {
                "type": "object",
                "properties": parameters,
                "required": list(parameters.keys()),
            }
        },
        "handler": handler
    }

# Example tools:
register_tool(
    name="read",
    description="Read a file from the local filesystem",
    parameters={
        "file_path": {"type": "string", "description": "Absolute path to file"}
    },
    handler=lambda args: read_file(args["file_path"])
)

register_tool(
    name="bash",
    description="Execute a shell command",
    parameters={
        "command": {"type": "string", "description": "Command to execute"}
    },
    handler=lambda args: run_bash(args["command"])
)
```

**Tool set (minimum viable):**
| Tool | Description |
|------|-------------|
| `read` | Read file with line numbers |
| `write` | Write/overwrite file |
| `edit` | Search-replace in file |
| `bash` | Execute shell command |
| `grep` | Regex content search |
| `glob` | File pattern matching |
| `todo_write` | Plan tasks before execution |

### 3. TodoWrite — Plan Before Execution (Session 3)

```python
TODO_LIST = []

def todo_write(todos):
    """Update the todo list and enforce plan-before-execute discipline."""
    global TODO_LIST
    TODO_LIST = todos
    return format_todo_list(TODO_LIST)

def format_todo_list(todos):
    lines = []
    for i, todo in enumerate(todos):
        status = {"pending": "⬜", "in_progress": "🔄", "completed": "✅", "cancelled": "❌"}
        lines.append(f"{status.get(todo['status'], '?')} {todo['content']}")
    return "\n".join(lines)
```

**Nag reminder:** If more than N turns pass without changing todo status, the system prompt reminds the agent.

### 4. Subagents — Isolated Context (Session 4)

```python
def dispatch_subagent(prompt, tools=None, context=None):
    """Spawn child agent with fresh messages[] and isolated context."""
    child_messages = [
        {"role": "user", "content": prompt}
    ]
    if context:
        child_messages.insert(0, {"role": "user", "content": f"Context:\n{context}"})

    child_tools = tools or TOOLS  # inherit all tools or subset

    result = agent_loop(child_messages, child_tools, SYSTEM_PROMPT)

    return {
        "type": "tool_result",
        "content": result.content[0].text
    }
```

**Key:** Subagent has its own `messages[]` — it does NOT see parent conversation. Parent only gets the final result.

### 5. Skills — Dynamic System Prompt Injection (Session 5)

```python
def load_skill(skill_name):
    """Load SKILL.md from file system and inject into system prompt."""
    skill_path = find_skill_file(skill_name)
    if not skill_path:
        return f"Skill '{skill_name}' not found"

    skill_content = read_file(skill_path)
    # Inject into system prompt for this turn
    return f"Skill loaded: {skill_name}\n\n{skill_content}"
```

### 6. Context Compression — 3 Layer System (Session 6)

**Problem:** Conversation grows unboundedly → exceeds context window.

**Layer 1: HISTORY_SNIP (middle truncation)**
```
Remove old tool_results from the middle of conversation.
Keep the first few messages (context) and last few messages (recent).
```

**Layer 2: Microcompact (summarization)**
```
Ask LLM to summarize intermediate conversation turns.
Replace detailed messages with concise summary.
```

**Layer 3: CONTEXT_COLLAPSE (full regeneration)**
```
Summarize ENTIRE conversation history.
Restart conversation with summary as first message.
```

```python
def compact_context(messages, max_tokens=100000):
    """3-layer compression pipeline."""
    estimated = estimate_tokens(messages)

    if estimated < max_tokens * 0.7:
        return messages  # no compact needed

    # Layer 1: Middle truncation
    if estimated < max_tokens * 0.9:
        return snip_middle(messages)

    # Layer 2: Summarize old turns
    summary = summarize_conversation(messages[:len(messages)//2])
    return [messages[0], summary] + messages[len(messages)//2:]

    # Layer 3: Full collapse
    return collapse_and_restart(messages)
```

### 7. Streaming Output (Session 7)

```python
def agent_loop_streaming(messages, tools, system_prompt):
    """Stream tokens as they arrive."""
    while True:
        with client.messages.stream(
            model=MODEL,
            system=system_prompt,
            messages=messages,
            tools=tools,
        ) as stream:
            content_blocks = []
            for event in stream:
                if event.type == "text":
                    print(event.text, end="", flush=True)  # real-time output
                    content_blocks.append(event)
                elif event.type == "tool_use":
                    content_blocks.append(event)

            response = stream.get_final_message()

        # ... same tool dispatch logic as non-streaming loop ...
```

### 8. Permission System (Session 8+)

**3-tier YAML-based permission governance:**

```yaml
# permissions.yaml
tools:
  read:
    default: allow
  write:
    default: ask          # prompt user before write
    paths:
      /workspace/:
        default: allow    # auto-allow in workspace
      /etc/:
        default: deny     # block system file access

  bash:
    default: ask
    dangerous_commands:   # always ask, even if default=allow
      - rm -rf
      - sudo
      - chmod 777
      - curl.*|.*sh
```

```python
def check_permission(tool_name, args):
    rule = PERMISSIONS.get(tool_name, {}).get("default", "ask")
    if rule == "allow":
        return True
    elif rule == "deny":
        return False
    elif rule == "ask":
        return prompt_user(f"Allow {tool_name}({args})?")
```

### 9. Session Persistence (Session 8+)

```python
def save_session(messages, todo_list, session_id=None):
    session_id = session_id or generate_id()
    with open(f"sessions/{session_id}.json", "w") as f:
        json.dump({"messages": messages, "todos": todo_list}, f)
    return session_id

def resume_session(session_id):
    with open(f"sessions/{session_id}.json", "r") as f:
        data = json.load(f)
    return data["messages"], data.get("todos", [])
```

### 10. File Edit Tool — Search-Replace Pattern

**The Claude Code edit pattern:**

```python
def edit_file(file_path, old_string, new_string):
    content = read_file(file_path)
    count = content.count(old_string)

    if count == 0:
        return f"ERROR: old_string not found in {file_path}"
    if count > 1:
        return f"ERROR: Found {count} matches. Provide more context to make unique."

    content = content.replace(old_string, new_string)
    write_file(file_path, content)

    # Show diff
    return generate_diff(old_string, new_string)
```

**Key insight:** Requiring unique match prevents accidental multi-edit. Model must provide enough surrounding context to make the match unique.

## Project Structure

```
tiny-claude-code/
├── core.py                  # Agent loop + tool registry + permissions
├── llm.py                   # Streaming client + retry logic
├── context.py               # 3-layer compression
├── session.py               # Save/resume persistence
├── prompt.py                # Dynamic system prompt builder
├── tools/
│   ├── bash.py              # Shell execution + dangerous command detection
│   ├── edit.py              # Search-replace file editing
│   ├── read.py              # File reading with line numbers
│   ├── write.py             # File creation/overwrite
│   ├── grep.py              # Content regex search
│   ├── glob_tool.py         # File pattern search
│   ├── todo.py              # Todo list management
│   └── subagent.py          # Child agent spawning
└── permissions.yaml         # Tool permission rules
```

## Development Roadmap

### Phase 1: Minimal Loop (Session 1-2)
- Basic `agent_loop()` with `while True`
- `read` + `bash` tools only
- Single-file CLI

### Phase 2: Core Tools (Session 3-5)
- `write`, `edit`, `grep`, `glob` tools
- `todo_write` + planning discipline
- Subagent spawning

### Phase 3: Polish (Session 6-8)
- Streaming output
- Context compression
- Skill loading

### Phase 4: Production Features (Session 9+)
- Permission governance (YAML)
- Session persistence (save/resume/fork)
- Hook/event system
- MCP tool integration
- Parallel tool execution

## Success Criteria

1. Agent loop calls LLM, parses tool_use, executes tools, loops successfully
2. Agent can read files, search codebase, and answer questions about it
3. Agent can edit files with unique-match search-replace
4. Agent plans multi-step tasks with todo_write before execution
5. Subagent completes isolated tasks without contaminating parent context
6. Streaming output shows tokens in real-time
7. Context compression keeps conversation within token limits
8. Permission system blocks dangerous bash commands
9. Sessions can be saved and resumed across restarts
10. Agent completes a non-trivial coding task (e.g., "add a function and tests")
