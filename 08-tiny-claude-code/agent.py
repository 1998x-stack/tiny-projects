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
