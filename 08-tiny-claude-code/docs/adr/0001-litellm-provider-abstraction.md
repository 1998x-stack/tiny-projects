# Use LiteLLM as the provider abstraction layer

We use LiteLLM (`litellm.completion()`) as the sole interface to language models, rather than the Anthropic SDK directly or a hand-rolled adapter. This gives us 100+ providers (Anthropic, OpenAI, Ollama, etc.) through a single `completion()` call, at the cost of an extra dependency and one level of indirection. The alternative — using the Anthropic SDK directly — would be simpler for the Anthropic-only case but would require writing our own adapter if we ever want OpenAI or local model support.

## Considered Options

- **Anthropic SDK directly**: Simpler, no translation layer, native tool_use support. But locks us to one provider.
- **OpenAI SDK as universal adapter**: Works with many providers natively, but Anthropic support via OpenAI-compatible endpoints is incomplete (no streaming tool calls, no system prompt as top-level parameter).
- **LiteLLM** (chosen): Handles provider differences automatically, built-in retry, streaming support across providers. Adds ~30MB dependency.
