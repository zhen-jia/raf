/*!
 * Copyright (c) 2019 by Contributors
 * \file src/impl/value.cc
 * \brief MNM value underlying implementation
 */
#include "tvm/runtime/ndarray.h"
#include "mnm/executor.h"
#include "mnm/ir.h"
#include "mnm/registry.h"
#include "mnm/tensor.h"
#include "mnm/value.h"
#include "../common/shape_utils.h"

namespace mnm {
namespace value {

using common::shape_utils::GetShape;
using common::shape_utils::MakeShape;
using executor::Executor;
using tensor::Tensor;
using namespace mnm::ir;

/*** Constructors ***/
TensorValue TensorValue::make(tensor::Tensor tensor) {
  ObjectPtr<TensorValueObj> n = make_object<TensorValueObj>();
  n->tensor = std::move(tensor);
  return TensorValue(n);
}

TupleValue TupleValue::make(Array<Value> fields) {
  ObjectPtr<TupleValueObj> n = make_object<TupleValueObj>();
  n->fields = std::move(fields);
  return TupleValue(n);
}

ClosureValue ClosureValue::make(Map<Var, Value> env, Function func) {
  ObjectPtr<ClosureValueObj> n = make_object<ClosureValueObj>();
  n->env = std::move(env);
  n->func = std::move(func);
  return ClosureValue(n);
}

RefValue RefValue::make(Value value) {
  ObjectPtr<RefValueObj> n = make_object<RefValueObj>();
  n->value = std::move(value);
  return RefValue(n);
}

OpValue OpValue::make(Op op) {
  ObjectPtr<OpValueObj> n = make_object<OpValueObj>();
  n->op = std::move(op);
  return OpValue(n);
}

IntValue ScalarValue::make(int data) {
  return IntValue::make(data);
}

IntValue ScalarValue::make(int64_t data) {
  return IntValue::make(data);
}

FloatValue ScalarValue::make(double data) {
  return FloatValue::make(data);
}

BoolValue ScalarValue::make(bool data) {
  return BoolValue::make(data);
}

IntValue IntValue::make(int64_t data) {
  ObjectPtr<IntValueObj> n = make_object<IntValueObj>();
  n->data = data;
  return IntValue(n);
}

FloatValue FloatValue::make(double data) {
  ObjectPtr<FloatValueObj> n = make_object<FloatValueObj>();
  n->data = data;
  return FloatValue(n);
}

BoolValue BoolValue::make(bool data) {
  ObjectPtr<BoolValueObj> n = make_object<BoolValueObj>();
  n->data = data;
  return BoolValue(n);
}

StringValue StringValue::make(const std::string& data) {
  ObjectPtr<StringValueObj> n = make_object<StringValueObj>();
  n->data = data;
  return StringValue(n);
}

/*** GetType ***/
Type GetType(const Value& value) {
  if (const auto* tv = value.as<TensorValueObj>()) {
    const DLTensor& dlt = *tv->tensor.operator->();
    auto shape = GetShape<tvm::Integer>(dlt);
    return ir::TensorTypeNode::make({shape.begin(), shape.end()}, tvm::TVMType2Type(dlt.dtype));
  } else if (const auto* tv = value.as<TupleValueObj>()) {
    Array<Type> tuple_type;
    for (const Value& sub_value : tv->fields) {
      tuple_type.push_back(GetType(sub_value));
    }
    return ir::TupleTypeNode::make(tuple_type);
  }
  LOG(FATAL) << "NotImplementedError: " << value->GetTypeKey();
  throw;
}

/*** Value ***/
Value::operator DLTensor*() const {
  if (const auto* tensor_value = this->as<TensorValueObj>()) {
    const DLTensor* dl_tensor_ref = tensor_value->tensor.operator->();
    return const_cast<DLTensor*>(dl_tensor_ref);
  }
  LOG(FATAL) << "InternalError: cannot convert to TensorValue";
  throw;
}

Value::operator tensor::Tensor&() const {
  if (const auto* tensor_value = this->as<TensorValueObj>()) {
    return tensor_value->tensor;
  }
  LOG(FATAL) << "InternalError: cannot convert to TensorValue";
  throw;
}

/*** TensorValue ***/
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

TensorValue FromTVM(tvm::runtime::NDArray array) {
  return TensorValue::make(Tensor::FromDLPack(array.ToDLPack()));
}

/*** External symbols ***/
tvm::runtime::NDArray ToTVM(TensorValue value) {
  DLManagedTensor* tensor = value->tensor.ToDLPack();
  if (tensor->dl_tensor.strides != nullptr) {
    tensor->deleter(tensor);
    LOG(FATAL) << "NotImplementedError: strided tensor not supported";
    throw;
  }
  return tvm::runtime::NDArray::FromDLPack(tensor);
}

ObjectRef DeTuple(Value value) {
  if (value->IsInstance<TensorValueObj>()) {
    return std::move(value);
  }
  if (const auto* tuple = value.as<TupleValueObj>()) {
    Array<ObjectRef> result;
    for (Value sub_value : tuple->fields) {
      if (sub_value->op_env == nullptr) {
        sub_value->op_env = tuple->op_env;
      }
      result.push_back(DeTuple(sub_value));
    }
    return std::move(result);
  }
  LOG(FATAL) << "ValueError: cannot de-tuple " << value->GetTypeKey();
  throw;
}

MNM_REGISTER_GLOBAL("mnm.value.AssembleTensorValue").set_body_typed(AssembleTensorValue);
MNM_REGISTER_GLOBAL("mnm.value.DeTuple").set_body_typed(DeTuple);
MNM_REGISTER_GLOBAL("mnm.value.FromTVM").set_body_typed(FromTVM);
MNM_REGISTER_GLOBAL("mnm.value.ToTVM").set_body_typed(ToTVM);
MNM_REGISTER_GLOBAL("mnm.value._make.TupleValue").set_body_typed(TupleValue::make);
MNM_REGISTER_GLOBAL("mnm.value._make.IntValue").set_body_typed(IntValue::make);
MNM_REGISTER_GLOBAL("mnm.value._make.FloatValue").set_body_typed(FloatValue::make);
MNM_REGISTER_GLOBAL("mnm.value._make.BoolValue").set_body_typed(BoolValue::make);
MNM_REGISTER_GLOBAL("mnm.value._make.StringValue").set_body_typed(StringValue::make);
MNM_REGISTER_OBJECT_NO_REFLECT(ValueObj);
MNM_REGISTER_OBJECT_NO_REFLECT(ScalarValueObj);
MNM_REGISTER_OBJECT_NO_REFLECT(OpaqueValueObj);
MNM_REGISTER_OBJECT_REFLECT(TensorValueObj);
MNM_REGISTER_OBJECT_REFLECT(TupleValueObj);
MNM_REGISTER_OBJECT_REFLECT(ClosureValueObj);
MNM_REGISTER_OBJECT_REFLECT(RefValueObj);
MNM_REGISTER_OBJECT_REFLECT(OpValueObj);
MNM_REGISTER_OBJECT_REFLECT(IntValueObj);
MNM_REGISTER_OBJECT_REFLECT(FloatValueObj);
MNM_REGISTER_OBJECT_REFLECT(BoolValueObj);
MNM_REGISTER_OBJECT_REFLECT(StringValueObj);
}  // namespace value
}  // namespace mnm

namespace mnm {
namespace value {

class BindingEntry {
 public:
  Expr expr{nullptr};
  Value value{nullptr};

  BindingEntry() = default;
  BindingEntry(const Expr& expr, const Value& value) : expr(expr), value(value) {
  }
};

class BindingMgr {
 public:
  std::mutex mu;
  std::unordered_map<const VarNode*, std::unique_ptr<BindingEntry> > bindings;

  static BindingMgr* Get() {
    static BindingMgr* instance = new BindingMgr();
    return instance;
  }
};

class BoundVarObj : public VarNode {
  // This is basically relay::VarNode, but with a customized callback that
  // deletes the weak reference inside BindingMgr
 public:
  ~BoundVarObj() {
    static BindingMgr* mgr = BindingMgr::Get();
    std::unique_ptr<BindingEntry> entry{nullptr};
    {
      std::lock_guard<std::mutex> lock(mgr->mu);
      auto iter = mgr->bindings.find(this);
      CHECK(iter != mgr->bindings.end());
      entry.swap(iter->second);
      mgr->bindings.erase(iter);
    }
    // "entry" is destroyed here, to avoid potential recursive lock
  }
  static Var make(const std::string& name_hint) {
    ObjectPtr<BoundVarObj> n = make_object<BoundVarObj>();
    ObjectPtr<IdNode> id_ptr = make_object<IdNode>();
    id_ptr->name_hint = name_hint;
    n->vid = Id(id_ptr);
    return Var(n);
  }
};

Var BindNothing(const std::string& name_hint) {
  const Expr& expr = NullValue<Expr>();
  const Value& value = NullValue<Value>();
  return BindExprValue(expr, value, name_hint);
}

Var BindValue(const Value& value, const std::string& name_hint) {
  const Expr& expr = MakeConstant(value);
  return BindExprValue(expr, value, name_hint);
}

Var BindExprValue(const Expr& expr, const Value& value, const std::string& name_hint) {
  static BindingMgr* mgr = BindingMgr::Get();
  Var var = BoundVarObj::make(name_hint);
  const VarNode* var_ptr = var.operator->();
  {
    std::lock_guard<std::mutex> lock(mgr->mu);
    mgr->bindings.emplace(var_ptr, std::make_unique<BindingEntry>(expr, value));
  }
  return var;
}

Expr LookupBoundExpr(const Var& var) {
  static BindingMgr* mgr = BindingMgr::Get();
  {
    std::lock_guard<std::mutex> lock(mgr->mu);
    auto iter = mgr->bindings.find(var.operator->());
    if (iter == mgr->bindings.end()) {
      return NullValue<Expr>();
    }
    return iter->second->expr;
  }
}

Value LookupBoundValue(const ir::Var& var) {
  static BindingMgr* mgr = BindingMgr::Get();
  {
    std::lock_guard<std::mutex> lock(mgr->mu);
    auto iter = mgr->bindings.find(var.operator->());
    if (iter == mgr->bindings.end()) {
      return NullValue<Value>();
    }
    return iter->second->value;
  }
}

MNM_REGISTER_GLOBAL("mnm.value.BindNothing").set_body_typed(BindNothing);
MNM_REGISTER_GLOBAL("mnm.value.BindValue").set_body_typed(BindValue);
MNM_REGISTER_GLOBAL("mnm.value.BindExprValue").set_body_typed(BindExprValue);
MNM_REGISTER_GLOBAL("mnm.value.LookupBoundExpr").set_body_typed(LookupBoundExpr);
MNM_REGISTER_GLOBAL("mnm.value.LookupBoundValue").set_body_typed(LookupBoundValue);
}  // namespace value
}  // namespace mnm
