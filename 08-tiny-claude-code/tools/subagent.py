from tools import BaseTool


class SubagentTool(BaseTool):
    name = "subagent"
    description = "Spawn a child agent with isolated context to handle a subtask. The child cannot see the parent conversation."
    parameters = {
        "prompt": {"type": "string", "description": "Task description for the child agent"},
    }

    def __init__(self, parent_agent):
        self.parent = parent_agent

    def execute(self, prompt) -> str:
        return self.parent.spawn_subagent(prompt)
