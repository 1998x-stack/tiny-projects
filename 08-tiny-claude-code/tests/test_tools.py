import pytest
from types import SimpleNamespace


def test_tool_definition_schema():
    from tools import BaseTool

    class DummyTool(BaseTool):
        name = "dummy"
        description = "A test tool"
        parameters = {"x": {"type": "string", "description": "input"}}

        def execute(self, x):
            return f"got {x}"

    tool = DummyTool()
    defn = tool.definition()
    assert defn["name"] == "dummy"
    assert defn["input_schema"]["properties"]["x"]["type"] == "string"
    assert defn["input_schema"]["required"] == ["x"]


def test_registry_definitions():
    from tools import BaseTool, ToolRegistry

    class FooTool(BaseTool):
        name = "foo"
        description = "foo tool"
        parameters = {"a": {"type": "string", "description": "a"}}
        def execute(self, a):
            return a

    reg = ToolRegistry()
    reg.register(FooTool())
    defs = reg.definitions()
    assert len(defs) == 1
    assert defs[0]["name"] == "foo"


def test_registry_execute_all_success():
    from tools import BaseTool, ToolRegistry

    class EchoTool(BaseTool):
        name = "echo"
        description = "echo"
        parameters = {"msg": {"type": "string", "description": "msg"}}
        def execute(self, msg):
            return msg

    reg = ToolRegistry()
    reg.register(EchoTool())
    calls = [SimpleNamespace(name="echo", id="c1", input={"msg": "hi"})]
    results = reg.execute_all(calls)
    assert len(results) == 1
    assert results[0]["content"] == "hi"
    assert results[0]["tool_use_id"] == "c1"


def test_registry_execute_unknown_tool():
    from tools import ToolRegistry

    reg = ToolRegistry()
    calls = [SimpleNamespace(name="nope", id="c2", input={})]
    results = reg.execute_all(calls)
    assert "ERROR" in results[0]["content"]
    assert "nope" in results[0]["content"]


def test_registry_execute_catches_exceptions():
    from tools import BaseTool, ToolRegistry

    class BrokenTool(BaseTool):
        name = "broken"
        description = "always fails"
        parameters = {}
        def execute(self):
            raise ValueError("boom")

    reg = ToolRegistry()
    reg.register(BrokenTool())
    calls = [SimpleNamespace(name="broken", id="c3", input={})]
    results = reg.execute_all(calls)
    assert "ERROR" in results[0]["content"]
    assert "boom" in results[0]["content"]


def test_read_tool_returns_numbered_lines(tmp_path):
    from tools.files import ReadTool

    f = tmp_path / "hello.py"
    f.write_text("line one\nline two\nline three\n")
    tool = ReadTool()
    result = tool.execute(file_path=str(f))
    assert "1\t" in result
    assert "line one" in result
    assert "3\t" in result


def test_read_tool_truncates_large_files(tmp_path):
    from tools.files import ReadTool

    f = tmp_path / "big.txt"
    f.write_text("x" * 10000)
    tool = ReadTool()
    result = tool.execute(file_path=str(f))
    assert len(result) <= 8200
    assert "[truncated]" in result


def test_read_tool_missing_file():
    from tools.files import ReadTool

    tool = ReadTool()
    result = tool.execute(file_path="/nonexistent/path/xyz.py")
    assert "ERROR" in result


def test_bash_tool_captures_stdout():
    from tools.bash import BashTool

    tool = BashTool()
    result = tool.execute(command="echo hello")
    assert "hello" in result


def test_bash_tool_captures_stderr():
    from tools.bash import BashTool

    tool = BashTool()
    result = tool.execute(command="echo oops >&2")
    assert "oops" in result


def test_bash_tool_truncates_output():
    from tools.bash import BashTool

    tool = BashTool()
    result = tool.execute(command="python3 -c \"print('x' * 10000)\"")
    assert len(result) <= 8200
    assert "[truncated]" in result
