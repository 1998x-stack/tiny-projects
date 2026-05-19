# Use Anthropic-style content blocks as the internal message format

Despite using LiteLLM (which speaks OpenAI format on the wire), our internal message format uses Anthropic-style content blocks: `{"type": "text", "text": "..."}` and `{"type": "tool_use", "id": "...", "name": "...", "input": {...}}`. The Provider layer translates both directions — OpenAI wire format in, Anthropic-style blocks out.

We chose this because Anthropic's format is richer and more explicit: content is a list of typed blocks rather than a flat string + separate tool_calls array. This makes the agent loop cleaner — it can iterate one uniform list instead of checking two fields. The cost is a normalization layer in Provider that wouldn't be needed if we used OpenAI format natively.
