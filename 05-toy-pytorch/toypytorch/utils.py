from toypytorch.engine import Value


def gradient_check(f, h=1e-6, rtol=1e-4, atol=1e-8):
    out = f()
    out.backward()

    leaves = _collect_leaves(out)
    analytical_grads = {id(v): v.grad for v in leaves}

    for leaf in leaves:
        original_data = leaf.data

        leaf.data = original_data + h
        out_plus = f()
        plus_val = out_plus.data

        leaf.data = original_data - h
        out_minus = f()
        minus_val = out_minus.data

        leaf.data = original_data

        numerical = (plus_val - minus_val) / (2 * h)
        analytical = analytical_grads[id(leaf)]

        denom = max(abs(numerical), abs(analytical), 1.0)
        if abs(numerical - analytical) / denom > rtol:
            if abs(numerical - analytical) > atol:
                return False

    return True


def _collect_leaves(root):
    leaves = []
    visited = set()

    def dfs(v):
        if v in visited:
            return
        visited.add(v)
        if not v._prev and not v._op:
            leaves.append(v)
        for child in v._prev:
            dfs(child)

    dfs(root)
    return leaves


def draw_dot(root):
    try:
        import graphviz
    except ImportError:
        raise ImportError("graphviz package required. Install: pip install graphviz")

    dot = graphviz.Digraph(format='svg', graph_attr={'rankdir': 'LR'})

    nodes = set()
    edges = set()

    def trace(v):
        if v not in nodes:
            nodes.add(v)
            for child in v._prev:
                edges.add((child, v))
                trace(child)

    trace(root)

    for n in nodes:
        uid = str(id(n))
        label = f"{{ data {n.data:.4f} | grad {n.grad:.4f} }}"
        dot.node(uid, label=label, shape='record')
        if n._op:
            op_uid = uid + n._op
            dot.node(op_uid, label=n._op)
            dot.edge(op_uid, uid)

    for n1, n2 in edges:
        dot.edge(str(id(n1)), str(id(n2)) + n2._op)

    return dot
