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
