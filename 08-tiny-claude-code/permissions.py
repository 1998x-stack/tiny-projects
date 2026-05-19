import json
import re

import yaml


class PermissionChecker:
    def __init__(self, config_path="permissions.yaml", cwd="/", prompt_fn=None):
        with open(config_path) as f:
            self.rules = yaml.safe_load(f) or {}
        self.cwd = cwd
        self.prompt_fn = prompt_fn or self._default_prompt

    def check(self, tool_name, args):
        rule = self.rules.get("tools", {}).get(tool_name, {"default": "ask"})
        if "deny_patterns" in rule:
            cmd = args.get("command", "")
            for pattern in rule["deny_patterns"]:
                if re.search(pattern, cmd):
                    return False, f"Command matches deny pattern: {pattern}"
        path = args.get("file_path", "")
        if path:
            for dp in rule.get("deny_paths", []):
                if path.startswith(dp):
                    return False, f"Path blocked: {dp}"
            if path.startswith(self.cwd):
                return True, "allowed (under cwd)"
        default = rule.get("default", "ask")
        if default == "allow":
            return True, "allowed"
        if default == "deny":
            return False, "denied by default"
        return self.prompt_fn(tool_name, args), "user decision"

    @staticmethod
    def _default_prompt(tool_name, args):
        display = f"{tool_name}({json.dumps(args, indent=None)[:100]})"
        response = input(f"Allow {display}? [y/N] ").strip().lower()
        return response in ("y", "yes")
