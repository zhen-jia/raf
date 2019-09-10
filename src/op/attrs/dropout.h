#pragma once

#include <mnm/base.h>
#include <mnm/ir.h>
#include <mnm/value.h>

namespace mnm {
namespace op {
namespace attrs {

class DropoutAttrs : public ir::AttrsNode<DropoutAttrs> {
 public:
  double dropout;
  int64_t seed;

  MNM_DECLARE_ATTRS(DropoutAttrs, "mnm.attrs.DropoutAttrs") {
    MNM_ATTR_FIELD(dropout);
    MNM_ATTR_FIELD(seed);
  }
};

}  // namespace attrs
}  // namespace op
}  // namespace mnm
