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
