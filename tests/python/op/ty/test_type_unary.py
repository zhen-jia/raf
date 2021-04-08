# pylint: disable=protected-access
import pytest
import mnm
from mnm._ffi.pass_ import AutoDiff, InferType
from mnm._op import sym
from mnm.testing import check_type, randn
from tvm.relay import TensorType, FuncType, TupleType


@pytest.mark.parametrize("op", [
    (sym.relu, True),
    (sym.log, False),
    (sym.cos, False),
    (sym.sin, False),
    (sym.sign, False),
    (sym.round, False),
    (sym.tanh, True),
    (sym.sigmoid, True),
    (sym.copy, False),
    (sym.abs, False),
    (sym.ceil, False),
    (sym.floor, False),
    (sym.exp, False),
    (sym.erf, True),
    (sym.sqrt, True),
    (sym.atan, False),
    (sym.negative, False),
    (sym.logical_not, False),
    (sym.zeros_like, False),
    (sym.ones_like, False),
    (sym.trunc, False),
])
@pytest.mark.parametrize("shape", [
    (),
    (2,),
    (1, 3),
    (3, 7, 9),
])
@pytest.mark.parametrize("dtype", ["float32", "float64"])
def test_unary(op, shape, dtype):
    op, backward = op

    class Unary(mnm.Model):
        def build(self):
            pass

        # pylint: disable=no-self-use
        @mnm.model.trace
        def forward(self, x):
            return op(x)

    model = Unary()
    fwd_ty = TensorType(shape, dtype=dtype)

    # forward
    m_x, _ = randn(shape, dtype=dtype)
    m_x.requires_grad = True
    record = model._internal(m_x)
    m_mod = record.mod
    m_mod = InferType()(m_mod)

    desired_type = FuncType([fwd_ty], fwd_ty)
    check_type(m_mod['main'], desired_type)

    # check backward
    if backward:
        m_mod = AutoDiff(record.requires_grads)(m_mod)
        m_mod = InferType()(m_mod)
        bwd_ty = FuncType([fwd_ty], fwd_ty)
        desired_type = FuncType([fwd_ty], TupleType([fwd_ty, bwd_ty]))
        check_type(m_mod['main'], desired_type)


if __name__ == "__main__":
    pytest.main([__file__])
