def test_save_and_resume(tmp_path):
    from session import SessionManager

    sm = SessionManager(session_dir=str(tmp_path))
    msgs = [{"role": "user", "content": "hi"}]
    todos = [{"content": "task 1", "status": "pending"}]
    sm.save("test-session", msgs, todos)
    loaded_msgs, loaded_todos = sm.resume("test-session")
    assert loaded_msgs == msgs
    assert loaded_todos == todos


def test_list_sessions(tmp_path):
    from session import SessionManager

    sm = SessionManager(session_dir=str(tmp_path))
    sm.save("alpha", [], [])
    sm.save("beta", [], [])
    sessions = sm.list_sessions()
    assert sessions == ["alpha", "beta"]


def test_resume_nonexistent(tmp_path):
    from session import SessionManager

    sm = SessionManager(session_dir=str(tmp_path))
    try:
        sm.resume("nonexistent")
        assert False, "Should have raised"
    except FileNotFoundError:
        pass
