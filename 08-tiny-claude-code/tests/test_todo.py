def test_todo_write_creates_list():
    from tools.todo import TodoTool

    tool = TodoTool()
    result = tool.execute(todos=[
        {"content": "Read the code", "status": "completed"},
        {"content": "Fix the bug", "status": "in_progress"},
        {"content": "Write tests", "status": "pending"},
    ])
    assert "Read the code" in result
    assert "Fix the bug" in result
    assert "Write tests" in result


def test_todo_write_replaces_list():
    from tools.todo import TodoTool

    tool = TodoTool()
    tool.execute(todos=[{"content": "old task", "status": "pending"}])
    assert len(tool.todos) == 1
    tool.execute(todos=[
        {"content": "new task 1", "status": "pending"},
        {"content": "new task 2", "status": "pending"},
    ])
    assert len(tool.todos) == 2
    assert tool.todos[0]["content"] == "new task 1"


def test_todo_get_todos():
    from tools.todo import TodoTool

    tool = TodoTool()
    tool.execute(todos=[{"content": "task", "status": "pending"}])
    assert tool.todos == [{"content": "task", "status": "pending"}]
