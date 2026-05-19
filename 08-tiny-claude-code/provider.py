import json
from types import SimpleNamespace

from litellm import completion


class Provider:
    def __init__(self, model="anthropic/claude-sonnet-4-20250514"):
        self.model = model

    def create(self, system, messages, tools):
        response = completion(
            model=self.model,
            messages=[{"role": "system", "content": system}] + self._to_litellm_messages(messages),
            tools=self._to_litellm_tools(tools) if tools else None,
        )
        return self._normalize(response)

    def stream(self, system, messages, tools):
        raw = completion(
            model=self.model,
            messages=[{"role": "system", "content": system}] + self._to_litellm_messages(messages),
            tools=self._to_litellm_tools(tools) if tools else None,
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

    def _to_litellm_messages(self, messages):
        """Translate internal (Anthropic-style) messages to OpenAI format for LiteLLM."""
        out = []
        for msg in messages:
            role = msg["role"]
            content = msg["content"]

            if role == "user" and isinstance(content, str):
                out.append({"role": "user", "content": content})

            elif role == "user" and isinstance(content, list):
                # Tool results: each becomes a separate {"role": "tool"} message
                for block in content:
                    if block.get("type") == "tool_result":
                        out.append({
                            "role": "tool",
                            "tool_call_id": block["tool_use_id"],
                            "content": str(block.get("content", "")),
                        })
                    else:
                        out.append({"role": "user", "content": str(block)})

            elif role == "assistant" and isinstance(content, list):
                # Assistant with content blocks → rebuild OpenAI format
                text_parts = []
                tool_calls = []
                for block in content:
                    if block.get("type") == "text":
                        text_parts.append(block["text"])
                    elif block.get("type") == "tool_use":
                        tool_calls.append({
                            "id": block["id"],
                            "type": "function",
                            "function": {
                                "name": block["name"],
                                "arguments": json.dumps(block["input"]),
                            },
                        })
                assistant_msg = {"role": "assistant"}
                assistant_msg["content"] = "\n".join(text_parts) if text_parts else None
                if tool_calls:
                    assistant_msg["tool_calls"] = tool_calls
                out.append(assistant_msg)

            else:
                out.append(msg)
        return out

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
