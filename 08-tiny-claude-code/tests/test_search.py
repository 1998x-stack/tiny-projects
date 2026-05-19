def test_grep_finds_pattern(tmp_path):
    from tools.search import GrepTool

    (tmp_path / "a.py").write_text("def hello():\n    pass\n")
    (tmp_path / "b.py").write_text("def world():\n    pass\n")
    tool = GrepTool()
    result = tool.execute(pattern="hello", path=str(tmp_path))
    assert "a.py" in result
    assert "hello" in result
    assert "b.py" not in result


def test_grep_no_matches(tmp_path):
    from tools.search import GrepTool

    (tmp_path / "a.txt").write_text("nothing here")
    tool = GrepTool()
    result = tool.execute(pattern="zzzzz", path=str(tmp_path))
    assert "no matches" in result.lower() or result.strip() == ""


def test_glob_finds_files(tmp_path):
    from tools.search import GlobTool

    (tmp_path / "a.py").write_text("")
    (tmp_path / "b.txt").write_text("")
    (tmp_path / "sub").mkdir()
    (tmp_path / "sub" / "c.py").write_text("")
    tool = GlobTool()
    result = tool.execute(pattern=str(tmp_path / "**" / "*.py"))
    assert "a.py" in result
    assert "c.py" in result
    assert "b.txt" not in result
