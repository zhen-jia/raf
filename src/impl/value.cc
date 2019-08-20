#include <mnm/executor.h>
#include <mnm/ir.h>
#include <mnm/registry.h>
#include <mnm/tensor.h>
#include <mnm/value.h>

#include "../common/shape_utils.h"

namespace mnm {
namespace value {

using common::shape_utils::MakeShape;
using executor::Executor;
using ir::Array;
using ir::Downcast;
using ir::Expr;
using ir::Function;
using ir::Integer;
using ir::make_node;
using ir::Map;
using ir::NodePtr;
using ir::NodeRef;
using ir::Op;
using ir::Var;
using tensor::Tensor;

ValueNode::~ValueNode() {
  if (executor != nullptr) {
    executor->OnDestruct(this);
  }
}

TensorValue TensorValue::make(tensor::Tensor tensor) {
  NodePtr<TensorValueNode> n = make_node<TensorValueNode>();
  n->tensor = std::move(tensor);
  return TensorValue(n);
}

TupleValue TupleValue::make(Array<Value> fields) {
  NodePtr<TupleValueNode> n = make_node<TupleValueNode>();
  n->fields = std::move(fields);
  return TupleValue(n);
}

ClosureValue ClosureValue::make(Map<Var, Value> env, Function func) {
  NodePtr<ClosureValueNode> n = make_node<ClosureValueNode>();
  n->env = std::move(env);
  n->func = std::move(func);
  return ClosureValue(n);
}

RefValue RefValue::make(Value value) {
  NodePtr<RefValueNode> n = make_node<RefValueNode>();
  n->value = std::move(value);
  return RefValue(n);
}

OpValue OpValue::make(Op op) {
  NodePtr<OpValueNode> n = make_node<OpValueNode>();
  n->op = std::move(op);
  return OpValue(n);
}

IntValue IntValue::make(int64_t data) {
  NodePtr<IntValueNode> n = make_node<IntValueNode>();
  n->data = std::move(data);
  return IntValue(n);
}

FloatValue FloatValue::make(double data) {
  NodePtr<FloatValueNode> n = make_node<FloatValueNode>();
  n->data = std::move(data);
  return FloatValue(n);
}

BoundExpr BoundExpr::make(Expr expr, Value value) {
  NodePtr<BoundExprNode> n = make_node<BoundExprNode>();
  n->expr = std::move(expr);
  n->value = std::move(value);
  return BoundExpr(n);
}

BoundExprNode::~BoundExprNode() {
  if (executor != nullptr) {
    executor->OnDestruct(this);
  }
}

void BoundExprNode::BindExecutor(Executor* executor) {
  CHECK(this->executor == nullptr);
  this->executor = executor;
  executor->OnBind(this);
}

Value::operator const DLTensor*() const {
  if (const auto* tensor_value = this->as<TensorValueNode>()) {
    const DLTensor* dl_tensor_ref = tensor_value->tensor.operator->();
    return dl_tensor_ref;
  }
  LOG(FATAL) << "InternalError: cannot convert to TensorValue";
  throw;
}

Value::operator const tensor::Tensor&() const {
  if (const auto* tensor_value = this->as<TensorValueNode>()) {
    return tensor_value->tensor;
  }
  LOG(FATAL) << "InternalError: cannot convert to TensorValue";
  throw;
}

TensorValue TensorValue::Assemble(const Context& ctx, const DType& dtype,
                                  const std::vector<int64_t>& shape,
                                  const std::vector<int64_t>& strides, void* const data) {
  return TensorValue::make(Tensor::make(ctx, dtype, shape, strides, data));
}

TensorValue AssembleTensorValue(DLContext ctx, DLDataType dtype, Array<Integer> shape,
                                Array<Integer> strides, void* data) {
  return TensorValue::make(
      Tensor::make(ctx, dtype, MakeShape<int64_t>(shape), MakeShape<int64_t>(strides), data));
}

NodeRef DeTuple(Value value) {
  if (const auto* _ = value.as<TensorValueNode>()) {
    return std::move(value);
  }
  if (const auto* tuple = value.as<TupleValueNode>()) {
    Array<NodeRef> result;
    for (Value sub_value : tuple->fields) {
      result.push_back(DeTuple(sub_value));
    }
    return std::move(result);
  }
  LOG(FATAL) << "ValueError: cannot de-tuple " << value->type_key();
  throw;
}

MNM_REGISTER_GLOBAL("mnm.value.AssembleTensorValue").set_body_typed(AssembleTensorValue);

MNM_REGISTER_GLOBAL("mnm.value.DeTuple").set_body_typed(DeTuple);

MNM_REGISTER_GLOBAL("mnm.value._make.TupleValue").set_body_typed(TupleValue::make);

MNM_REGISTER_GLOBAL("mnm.value._make.IntValue").set_body_typed(IntValue::make);

MNM_REGISTER_GLOBAL("mnm.value._make.FloatValue").set_body_typed(FloatValue::make);

MNM_REGISTER_GLOBAL("mnm.value._make.BoundExpr").set_body_typed(BoundExpr::make);

}  // namespace value
}  // namespace mnm
