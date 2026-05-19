import pytest


def test_write_creates_file(tmp_path):
    from tools.files import WriteTool

    tool = WriteTool()
    path = str(tmp_path / "new.txt")
    result = tool.execute(file_path=path, content="hello world")
    assert "11" in result
    assert (tmp_path / "new.txt").read_text() == "hello world"


def test_write_creates_parent_dirs(tmp_path):
    from tools.files import WriteTool

    tool = WriteTool()
    path = str(tmp_path / "a" / "b" / "c.txt")
    result = tool.execute(file_path=path, content="deep")
    assert "4" in result
    assert (tmp_path / "a" / "b" / "c.txt").read_text() == "deep"


def test_write_overwrites_existing(tmp_path):
    from tools.files import WriteTool

    f = tmp_path / "exist.txt"
    f.write_text("old")
    tool = WriteTool()
    tool.execute(file_path=str(f), content="new")
    assert f.read_text() == "new"


def test_edit_unique_match(tmp_path):
    from tools.files import EditTool

    f = tmp_path / "code.py"
    f.write_text("def hello():\n    print('Helo')\n")
    tool = EditTool()
    result = tool.execute(
        file_path=str(f),
        old_string="print('Helo')",
        new_string="print('Hello')",
    )
    assert "Hello" in f.read_text()
    assert "---" in result


def test_edit_no_match(tmp_path):
    from tools.files import EditTool

    f = tmp_path / "code.py"
    f.write_text("def hello():\n    pass\n")
    tool = EditTool()
    result = tool.execute(
        file_path=str(f),
        old_string="nonexistent string",
        new_string="replacement",
    )
    assert "ERROR" in result
    assert "not found" in result


def test_edit_multiple_matches(tmp_path):
    from tools.files import EditTool

    f = tmp_path / "code.py"
    f.write_text("x = 1\nx = 2\nx = 3\n")
    tool = EditTool()
    result = tool.execute(
        file_path=str(f),
        old_string="x = ",
        new_string="y = ",
    )
    assert "ERROR" in result
    assert "3 matches" in result
    assert f.read_text() == "x = 1\nx = 2\nx = 3\n"
