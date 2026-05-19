def test_build_system_prompt_minimal():
    from prompt import build_system_prompt, BASE_PROMPT

    result = build_system_prompt()
    assert BASE_PROMPT in result


def test_build_system_prompt_with_cwd():
    from prompt import build_system_prompt

    result = build_system_prompt(cwd="/my/project")
    assert "/my/project" in result


def test_build_system_prompt_with_todos():
    from prompt import build_system_prompt

    todos = [
        {"content": "Fix bug", "status": "in_progress"},
        {"content": "Write docs", "status": "pending"},
    ]
    result = build_system_prompt(todos=todos)
    assert "Fix bug" in result
    assert "Write docs" in result


def test_build_system_prompt_with_skills():
    from prompt import build_system_prompt

    skills = [{"name": "git-expert", "content": "You know git well."}]
    result = build_system_prompt(skills=skills)
    assert "git-expert" in result
    assert "You know git well." in result
