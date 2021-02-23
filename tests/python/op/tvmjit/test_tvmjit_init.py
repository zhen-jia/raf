# pylint: disable=protected-access,attribute-defined-outside-init,invalid-name
import numpy as np
import pytest
import mxnet as mx
import mnm
from mnm.testing import get_device_list, randint, check, run_vm_model


@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("ops", [
    (np.zeros, mnm._op.sym.zeros),
    (np.ones, mnm._op.sym.ones),
])
@pytest.mark.parametrize("shape", [(), (1, ), (1, 2), (1, 2, 3), (1, 2, 3, 4)])
@pytest.mark.parametrize("dtype", ["float64", "float32", "int64", "int32", "bool"])
def test_init_ops(ops, shape, dtype, device):
    class InitOpModel(mnm.Model):
        def build(self, op, shape, dtype, device):
            self._op = op
            self._shape = shape
            self._dtype = dtype
            self._device = device

        @mnm.model.trace
        def forward(self):
            return self._op(shape=self._shape, dtype=self._dtype, device=self._device)

    n_op, m_op = ops
    model = InitOpModel(m_op, shape, dtype, device)
    m_y = model()
    v_y = run_vm_model(model, device, [])
    n_y = n_op(shape=shape, dtype=dtype)
    check(m_y, n_y)
    check(v_y, n_y)


@pytest.mark.parametrize("device", get_device_list())
@pytest.mark.parametrize("indices_shape", [(1, ), (1, 2), (1, 2, 3), (1, 2, 3, 4)])
@pytest.mark.parametrize("on_value", [1.0, -1.0])
@pytest.mark.parametrize("off_value", [0.0, 1.0])
@pytest.mark.parametrize("depth", [0, 1, 3])
@pytest.mark.parametrize("dtype", ["float64", "float32", "int64", "int32"])
def test_one_hot(indices_shape, on_value, off_value, depth, dtype, device):
    # pylint: disable=no-member, too-many-arguments, too-many-locals
    class OneHot(mnm.Model):
        def build(self, depth, dtype, device):
            self.depth = depth
            self.dtype = dtype
            self.device = device

        @mnm.model.trace
        def forward(self, shape, on_value, off_value):
            return mnm.one_hot(shape, on_value, off_value, depth=self.depth,
                               dtype=self.dtype, device=self.device)

    model = OneHot(depth, dtype, device)
    m_indices, n_indices = randint(shape=indices_shape, high=10, device=device)
    mx_indices = mx.nd.array(n_indices)
    m_on_value = mnm.array(on_value, device=device)
    m_off_value = mnm.array(off_value, device=device)
    m_y = model(m_indices, m_on_value, m_off_value)
    mx_y = mx.nd.one_hot(mx_indices, depth=depth, on_value=on_value,
                         off_value=off_value, dtype=dtype)
    check(m_y, mx_y.asnumpy())
    v_y = run_vm_model(model, device, [m_indices, m_on_value, m_off_value])
    check(v_y, mx_y.asnumpy())


if __name__ == "__main__":
    pytest.main([__file__])
