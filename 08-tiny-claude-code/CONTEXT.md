# Tiny Claude Code

A ~1,500 LoC toy implementation of a Claude Code-style AI coding agent, built for learning and light use.

## Language

**Agent Loop**:
The core while-loop that calls the model, dispatches tools, and repeats until the model returns text.
_Avoid_: Main loop, conversation loop, chat loop

**Turn**:
One iteration of the agent loop: a model call plus any resulting tool executions.
_Avoid_: Step, round, iteration

**Tool**:
A capability exposed to the model via JSON schema. The model requests tools; the harness executes them.
_Avoid_: Function, action, command (except for `bash` tool's `command` parameter)

**Content Block**:
A typed dict in the internal message format: `{"type": "text", ...}` or `{"type": "tool_use", ...}`. Messages contain a list of content blocks.
_Avoid_: Chunk (reserved for streaming), message part

**Chunk**:
A streaming fragment yielded by `Provider.stream()`. Each chunk is a complete content block (text or tool_use), not a raw wire-format delta.
_Avoid_: Event, delta, fragment

**Subagent**:
A child Agent with its own `messages[]`. Shares provider and tools with the parent but isolates conversation state. Parent sees only the final text result.
_Avoid_: Child process, worker, thread

**Compaction**:
Reducing the size of the message list to stay within token limits. Three layers: snip (replace tool results with placeholders), summarize (LLM-condense the first half), collapse (restart from a full summary).
_Avoid_: Compression, truncation (these describe specific layers, not the overall process)

**Permission**:
One of three decisions for a tool call: `allow` (proceed silently), `deny` (block with error), `ask` (prompt user).
_Avoid_: Authorization, access control

**Skill**:
A markdown file injected into the system prompt to give the agent domain-specific instructions.
_Avoid_: Plugin, extension, module

**Session**:
A serialized snapshot of `messages[]` + `todos[]` saved to JSON. Can be resumed across restarts.
_Avoid_: Conversation, history, state

## Relationships

- An **Agent Loop** executes **Turns** until the model stops requesting **Tools**
- Each **Turn** produces one or more **Content Blocks** in the message list
- **Compaction** runs via `pre_call` hook before each **Turn**, mutating messages in place
- A **Subagent** is a full **Agent Loop** with its own messages — not a lightweight coroutine
- **Permissions** gate **Tool** execution inside the **ToolRegistry**, not in the **Agent Loop**
- **Skills** are injected into the system prompt, rebuilt each **Turn** via the callable `system` property
- A **Session** captures the full message list and todo state at a point in time

## Example dialogue

> **Dev:** "When a **Turn** triggers **Compaction**, does the model know its context was snipped?"
> **Domain expert:** "No — the model sees the result after compaction. Snipped tool results just look like short strings. The model doesn't get a notification."

> **Dev:** "Is a **Subagent** a separate process?"
> **Domain expert:** "No — it's a regular **Agent Loop** running synchronously in the same process. It just has its own `messages[]` so it can't see or pollute the parent conversation."

## Flagged ambiguities

- "message" vs "content block" — a message is `{"role": "...", "content": [...]}` where content is a list of content blocks. Early spec drafts used "message" to mean both; resolved: message is the envelope, content block is the payload.
- "streaming chunk" vs "wire delta" — the Provider yields normalized chunks (complete content blocks); raw wire deltas (partial JSON fragments) are internal to the Provider and never leak to the Agent.
