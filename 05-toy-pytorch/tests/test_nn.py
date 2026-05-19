from toypytorch.nn import Module, Neuron, Layer, MLP
from toypytorch.engine import Value


def test_module_parameters_default_empty():
    m = Module()
    assert m.parameters() == []


def test_module_zero_grad():
    m = Module()
    m.zero_grad()


def test_neuron_parameters_count():
    n = Neuron(3)
    params = n.parameters()
    assert len(params) == 4


def test_neuron_forward():
    n = Neuron(2, nonlin=False)
    n.w[0].data = 1.0
    n.w[1].data = 2.0
    n.b.data = 0.5
    x = [Value(3.0), Value(4.0)]
    out = n(x)
    assert abs(out.data - 11.5) < 1e-6


def test_neuron_relu():
    n = Neuron(2, nonlin=True)
    n.w[0].data = -1.0
    n.w[1].data = -1.0
    n.b.data = 0.0
    x = [Value(1.0), Value(1.0)]
    out = n(x)
    assert out.data == 0.0


def test_layer_output_shape():
    layer = Layer(3, 5)
    x = [Value(1.0), Value(2.0), Value(3.0)]
    out = layer(x)
    assert len(out) == 5
    assert all(isinstance(v, Value) for v in out)


def test_layer_parameters_count():
    layer = Layer(3, 5)
    assert len(layer.parameters()) == 20


def test_mlp_output_shape_single():
    mlp = MLP(2, [16, 16, 1])
    x = [Value(1.0), Value(2.0)]
    out = mlp(x)
    assert isinstance(out, Value)


def test_mlp_output_shape_multi():
    mlp = MLP(2, [16, 5])
    x = [Value(1.0), Value(2.0)]
    out = mlp(x)
    assert len(out) == 5


def test_mlp_parameters_count():
    mlp = MLP(2, [16, 16, 1])
    assert len(mlp.parameters()) == 337


def test_mlp_final_layer_no_activation():
    mlp = MLP(1, [1])
    for p in mlp.parameters():
        p.data = -1.0
    out = mlp([Value(1.0)])
    assert abs(out.data - (-2.0)) < 1e-6
