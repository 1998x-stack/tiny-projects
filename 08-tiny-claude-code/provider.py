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
