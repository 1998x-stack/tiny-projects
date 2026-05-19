def _make_messages(count, content_size=100):
    msgs = []
    for i in range(count):
        if i % 2 == 0:
            msgs.append({"role": "user", "content": f"message {i}"})
        else:
            msgs.append({
                "role": "user",
                "content": [
                    {"type": "tool_result", "tool_use_id": f"t{i}", "content": "x" * content_size}
                ],
            })
    return msgs


def test_no_compaction_under_threshold():
    from context import ContextManager

    class FakeProvider:
        pass

    cm = ContextManager(FakeProvider(), max_tokens=10000)
    msgs = _make_messages(4, content_size=50)
    original = [m.copy() for m in msgs]
    cm.maybe_compact(msgs)
    assert len(msgs) == len(original)


def test_snip_tool_results_at_70_percent():
    from context import ContextManager

    class FakeProvider:
        pass

    # 10 messages with 300-char tool results = ~535 tokens
    # max_tokens=700 → ratio ~0.76 → snip tier
    cm = ContextManager(FakeProvider(), max_tokens=700)
    msgs = _make_messages(10, content_size=300)
    cm.maybe_compact(msgs)
    snipped = False
    for msg in msgs:
        if isinstance(msg.get("content"), list):
            for block in msg["content"]:
                if "snipped" in str(block.get("content", "")):
                    snipped = True
    assert snipped


def test_estimate_tokens():
    from context import ContextManager

    class FakeProvider:
        pass

    cm = ContextManager(FakeProvider(), max_tokens=1000)
    msgs = [{"role": "user", "content": "x" * 400}]
    tokens = cm.estimate_tokens(msgs)
    assert 80 < tokens < 200


def test_compaction_preserves_head_and_tail():
    from context import ContextManager

    class FakeProvider:
        pass

    # 20 messages with 200-char tool results = ~1500 tokens
    # max_tokens=2000 → ratio ~0.75 → snip tier
    cm = ContextManager(FakeProvider(), max_tokens=2000)
    msgs = _make_messages(20, content_size=200)
    first_msg = msgs[0].copy()
    last_msgs = [m.copy() for m in msgs[-5:]]
    cm.maybe_compact(msgs)
    assert msgs[0] == first_msg
    assert msgs[-5:] == last_msgs
