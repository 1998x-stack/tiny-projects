import yaml


def _write_config(tmp_path, config):
    path = tmp_path / "permissions.yaml"
    path.write_text(yaml.dump(config))
    return str(path)


def test_allow_by_default(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {"tools": {"read": {"default": "allow"}}})
    pc = PermissionChecker(config_path=cfg)
    allowed, reason = pc.check("read", {"file_path": "/any/path"})
    assert allowed is True


def test_deny_by_pattern(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {
        "tools": {"bash": {"default": "ask", "deny_patterns": ["rm -rf", "sudo"]}}
    })
    pc = PermissionChecker(config_path=cfg)
    allowed, reason = pc.check("bash", {"command": "sudo rm -rf /"})
    assert allowed is False
    assert "deny pattern" in reason


def test_deny_by_path(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {
        "tools": {"write": {"default": "ask", "deny_paths": ["/etc/"]}}
    })
    pc = PermissionChecker(config_path=cfg)
    allowed, reason = pc.check("write", {"file_path": "/etc/passwd"})
    assert allowed is False


def test_auto_allow_under_cwd(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {
        "tools": {"write": {"default": "ask"}}
    })
    pc = PermissionChecker(config_path=cfg, cwd="/home/user/project")
    allowed, reason = pc.check("write", {"file_path": "/home/user/project/foo.py"})
    assert allowed is True
    assert "cwd" in reason


def test_ask_calls_prompt_fn(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {
        "tools": {"bash": {"default": "ask"}}
    })
    responses = [True]
    pc = PermissionChecker(
        config_path=cfg,
        prompt_fn=lambda tool, args: responses.pop(0),
    )
    allowed, reason = pc.check("bash", {"command": "ls"})
    assert allowed is True


def test_unknown_tool_defaults_to_ask(tmp_path):
    from permissions import PermissionChecker

    cfg = _write_config(tmp_path, {"tools": {}})
    pc = PermissionChecker(
        config_path=cfg,
        prompt_fn=lambda tool, args: False,
    )
    allowed, reason = pc.check("unknown_tool", {})
    assert allowed is False
