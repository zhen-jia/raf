from mnm._lib import _APIS

# pylint: disable=invalid-name,redefined-builtin
# Defined in ./src/pass/gradient.cc
AutoDiff = _APIS.get("mnm.pass_.AutoDiff", None)
# Defined in ./src/pass/fold_const.cc
BindParam = _APIS.get("mnm.pass_.BindParam", None)
# Defined in ./src/pass/extract_binding.cc
ExtractBinding = _APIS.get("mnm.pass_.ExtractBinding", None)
# Defined in ./src/pass/fold_const.cc
FoldConstant = _APIS.get("mnm.pass_.FoldConstant", None)
# Defined in ./src/pass/rename_vars.cc
RenameVars = _APIS.get("mnm.pass_.RenameVars", None)
# Defined in ./src/pass/fold_const.cc
is_constant = _APIS.get("mnm.pass_.is_constant", None)
