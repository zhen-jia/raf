import utils
import os

# TODO(@were): Auto-generate test cases.

class Scope(object):
    reserved_keys = ['callee', 'attrs_t', 'size_t', 'res', 'dlts', 'order', 'shapes']
    default = {
        'cudnnDataType_t'       : 'CUDNNDType(dtype)',
        'cudnnNanPropagation_t' : 'CUDNN_PROPAGATE_NAN',
        'cudnnHandle_t'         : 'CUDNNThreadEntry::ThreadLocal()->handle',
        'cudnnTensorFormat_t'   : 'CUDNN_TENSOR_NCHW'
    }


    def __init__(self, op):
        self.scope = [{}]
        # TODO(@were): Support grad initialization.
        self.add_global('alpha', f'CUDNNDType(dtype).const_addr<1>()')
        self.add_global('beta', 'CUDNNDType(dtype).const_addr<0>()')
    

    def find(self, arg):
        for i in self.scope[::-1]:
            if arg.name in i.keys():
                return i[arg.name]
        if arg.type in Scope.default.keys():
            return Scope.default[arg.type]
        return None


    def enter(self, subscope):
        self.scope.append({})
        for k, v in subscope.items():
            if k not in Scope.reserved_keys and isinstance(v, (str, tuple)):
                self.scope[-1][k] = v


    def add(self, k, v):
        self.scope[-1][k] = v


    def add_global(self, k, v):
        self.scope[0][k] = v


    def exit(self):
        if len(self.scope) > 1:
            self.scope.pop()


class Status(object):

    def __init__(self, op_name, func, order, shapes):
        self.current_tensor = 0
        self.op = op_name
        self.exec_func = func
        self.order = order
        self.shapes = shapes
        self.last_desc_name = None

    def current_arg_idx(self):
        return self.order[self.current_tensor]

    def current_shape(self):
        idx = self.current_arg_idx()
        shape = 'shape_%d' % idx
        if self.shapes is not None and self.shapes.get(self.current_arg_idx(), None):
            return 'std::vector<int> %s{%s};' % (shape, self.shapes.get(idx, None))
        return 'FORM_SHAPE(%s, dlts[%d]);' % (shape, idx)

    def last_arg_idx(self):
        assert self.current_tensor >= 1
        return self.order[self.current_tensor - 1]
    
    def next_arg(self, desc_name):
        self.current_tensor += 1
        self.last_desc_name = desc_name
    
    def match_last_tensor(self, ptr_name):
        if self.last_desc_name is None:
            return False
        if self.last_desc_name.lower().startswith(ptr_name.lower()):
            return True


class Emitter(object):

    namespaces = ['mnm', 'op', 'backend', 'cudnn', 'generated']

    headers = [
            '#include <cudnn.h>\n',
            '#include <mnm/op.h>\n',
            '#include "./util.h"',
            '#include "../../../common/arg_utils.h"',
            '#include "../../../common/shape_utils.h"',
            '#include "../../../common/cuda.h"'
    ]

    usings = [
            'common::arg_utils::AsVector',
            'common::arg_utils::DeduceDLType',
            'common::arg_utils::DeduceCtx',
            'common::shape_utils::MakeShape',
            'common::shape_utils::BytesCompactTensor',
            'dmlc::BeginPtr',
            'mnm::op::OpEnv',
            'ir::Array',
            'ir::Attrs',
            'ir::Downcast',
            'ir::make_node',
            'value::Value',
            'value::TensorValue',
            'value::TupleValue',
            'value::OpaqueValue',
    ]

    finder_fmt = """%s %s(const std::vector<int64_t> &key, %s) {
  if (%s.has(key)) {
    return %s.get(key);
  }
  int cnt;
  %s res;
  %s
  if (res.status != CUDNN_STATUS_SUCCESS) {
    LOG(FATAL) << "ValueError: Cannot find a proper algorithm!\\n";
    throw;
  }
  %s.set(key, res.algo);
  return res.algo;
}
"""
    size_fmt = """int64_t %s(%s) {
  size_t sizeInBytes;
  %s\n
  return sizeInBytes;\n}
"""

    args_attrs = 'Array<Value> args, const OpInfo &info, Attrs attrs'
    prfx = 'cudnn'
    desc_t = 'Descriptor_t'
    tensor_desc_t = '%sTensor%s' % (prfx, desc_t)
    filter_desc_t = '%sFilter%s' % (prfx, desc_t)

    def __init__(self, prefix, parser_info):
        self._cc = open('%s.cc' % prefix, 'w')
        self.indent_ = 0

        self.func_used = set()
        self.cudnn_api = parser_info.parsed_funcs
        self.cudnn_enum = parser_info.parsed_enums

        self.needed_enum = set()
        self.extra_fields = []
        self.extra_fields_by_key = {}
        self.extra_wrappers = []

        self.default = {
            'cudnnDataType_t'       : 'CUDNNDType(dtype)',
            'cudnnNanPropagation_t' : 'CUDNN_PROPAGATE_NAN',
            'cudnnHandle_t'         : 'CUDNNThreadEntry::ThreadLocal()->handle',
        }

        self.write('\n'.join(Emitter.headers) + '\n')
        for fs in os.listdir('../../src/op/attrs'):
            if fs.endswith('.h'):
                self.write('#include "../../attrs/%s"' % fs)

        for ns in Emitter.namespaces:
            self.write('namespace %s {' % ns)
        self.write('\n')

        for i in Emitter.usings:
            self.write('using %s;' % i)
        self.write('\n')

        self.status = None


    def __del__(self):
        self.write('\n')
        for ns in Emitter.namespaces[::-1]:
            self.write('} // namespaces %s' % ns)
    
    def get_arg_list(self, name):
        self.func_used.add(name)
        return self.cudnn_api[name]


    def write(self, s):
        if s == '\n':
            self._cc.write('\n')
            return
        self._cc.write((self.indent_ * ' ' + '%s\n') % s)


    def indent(self, delta):
        self.indent_ += delta
        assert self.indent_ >= 0


    def emit_enum(self):
        class_fmt = 'class %s : public EnumBase<%s, %d, int32_t, %s> {\n public:'
        enum_def_fmt = 'ENUM_DEF_ENTRY_WITH_NAME(%s, %d, %s, %s, "%s");'

        # Skip this for now
        self.cudnn_enum.pop('cudnnFusedOpsVariantParamLabel_t')
        # This is handled by special rule
        self.cudnn_enum.pop('cudnnDataType_t')

        for name in self.needed_enum:
            elems = self.cudnn_enum[name]
            wrapper_name = '%sEnum' % name[5:-2]
            self.write(class_fmt % (wrapper_name, wrapper_name, len(elems), name))
            self.indent(2)
            self.write('ENUM_DEF_HEADER(%s, 0, plain);' % (wrapper_name))
            for elem in elems:
                s, v = elem
                self.write(enum_def_fmt % (wrapper_name, v, utils.format_enum_name(s), s, s))
            self.indent(-2)
            self.write('};\n')
    
    
    @staticmethod
    def strip(s, prfx, suff):
        return s[len(prfx): -len(suff)]


    def _dispatch_openv_emission(self, op, rule, is_first, idx, super_cls='OpEnv'):
        if is_first:
            self.extra_fields = []
        else:
            self.extra_fields = self.extra_fields_by_key[(op, idx)]
        self._emit_openv(op, rule.copy())
        if is_first:
            self.extra_fields_by_key[(op, idx)] = self.extra_fields
    
    def dispatch_openv_emission(self, op, rule, is_first):
        if isinstance(rule, dict):
            self._dispatch_openv_emission(op, rule, is_first, 0)
            reg_class = self._class_name()
        elif isinstance(rule, tuple):
            cond, r0, r1 = rule
            super_name = ('Unified_%s' % op).replace('.', '_')
            self.write('class %s;' % super_name)
            self._dispatch_openv_emission(op, r0, is_first, 0, super_cls=super_name)
            name0 = self._class_name()
            self._dispatch_openv_emission(op, r1, is_first, 1, super_cls=super_name)
            name1 = self._class_name()
            self.write('class %s : OpEnv {\n public:\n' % super_name)
            self.indent(2)
            self.write('static OpEnv *make(%s) {\n' % Emitter.args_attrs)
            self.indent(2)
            self.write('auto casted_ptr = attrs.as<attrs::%s>();' % cond['attrs_t'])
            self.write('(void) casted_ptr;')
            self.write('if (%s) {' % cond['cond'])
            self.write('auto res = std::make_unique<%s>(args, info, attrs);' % name0)
            self.write('return res.release();')
            self.write('} else {')
            self.write('auto res = std::make_unique<%s>(args, info, attrs);' % name1)
            self.write('return res.release();')
            self.write('}')
            self.indent(-2)
            self.write('}\n')
            self.indent(-2)
            self.write('};\n')
            reg_class = super_name
        else:
            assert False

        reg_fmt = 'MNM_REGISTER_OP_DISPATCH("mnm.op.%s", DevType::kCUDA(), "generated_cudnn", %s::make);'
        self.write(reg_fmt % (op, reg_class))
        self.write('\n\n')



    def emit_openv(self, rules):

        """ Dump it twice. The first pass is to figure out which wrappers to dump. """
        old = self._cc
        self._cc = open('/dev/null', 'w')
        for op, rule in rules:
            self.dispatch_openv_emission(op, rule, True)

        self._cc = old
        self.emit_enum()
        self.write('\n'.join(self.extra_wrappers))
        for op, rule in rules:
            self.dispatch_openv_emission(op, rule, False)

    
    def _class_name(self):
        exec_func = self.status.exec_func
        op_name = self.status.op
        return ('%s_for_op_%s' % (exec_func[5:], op_name)).replace('.', '_')

    def _handle_enum_literal(self, arg, s):
        if arg.type == 'cudnnDataType_t':
            return None
        if arg.type not in self.cudnn_enum.keys():
            return None
        ripped = arg.type[5:-2]
        res = None
        if isinstance(s, str) and s.startswith('CUDNN_'):
            s = utils.format_enum_name(s)
            res = '%sEnum(%sEnum::%s())' % (ripped, ripped, s)
        elif isinstance(s, tuple):
            c, s0, s1 = s
            assert s0.startswith('CUDNN_') and s1.startswith('CUDNN_')
            s0 = utils.format_enum_name(s0)
            s0 = '%sEnum(%sEnum::%s())' % (ripped, ripped, s0)
            s1 = utils.format_enum_name(s1)
            s1 = '%sEnum(%sEnum::%s())' % (ripped, ripped, s1)
            res = '%s ? %s : %s' % (c, s0, s1)
        self.needed_enum.add(arg.type)
        return res
    
    def emit_algorithm_wrapper(self, arg, rule):
        tensor_cnt = 0
        for i in self.get_arg_list(self.status.exec_func):
            if i.type in [Emitter.tensor_desc_t, Emitter.filter_desc_t]:
                # TODO(@were): This is not good probably a should provide someway
                #              to access the root rule.
                self.initialize_arg_if_not_found(i, {})
                tensor_cnt += 1
        self.write('std::vector<int64_t> key;')
        for i in range(tensor_cnt):
            self.write('VecAppend(key, shape_%d);' % i)
        for elem in rule.get('extrakeys', []):
            self.write('VecAppend(key, %s);' % elem)
        callee = rule.get('callee')
        if callee is None:
            callee = 'cudnnFind%sAlgorithm' % self.status.exec_func[5:]
        args = self.get_arg_list(callee)
        dest = rule.get('dest', arg.name)
        arg_list, param_list = self._form_wrapper_arglist(callee, rule, {args[-3].name : '1',
                                                                         args[-2].name : '&cnt',
                                                                         args[-1].name : '&res'})
        wrapped_call = self._call_wrapped_func(callee, {args[-3].name : '1',
                                                        args[-2].name : '&cnt',
                                                        args[-1].name : '&res'})
        cache_name = '_cache_%s' % arg.type
        perf_t = args[-1].type
        cache_decl = 'AlgorithmCache<%s> %s;' % (arg.type, cache_name)
        finder_decl = Emitter.finder_fmt % (arg.type, callee[5:], arg_list,
                                            cache_name, cache_name, perf_t,
                                            wrapped_call, cache_name)
        self.extra_wrappers.append(cache_decl)
        self.extra_wrappers.append(finder_decl)
        self.write('%s = %s(key, %s);' % (dest, callee[5:], param_list))
        self.scope.add_global(dest, dest)

    def _form_wrapper_arglist(self, func, rule, prefill):
        args = self.get_arg_list(func)
        f = lambda i: i.type not in Scope.default.keys() and i.name not in prefill.keys()
        a = [i.__str__() for i in args if f(i)]
        b = [self.initialize_arg_if_not_found(i, rule) for i in args if f(i)]
        return ', '.join(a), ', '.join(b)

    def _call_wrapped_func(self, func, prefill):
        args = self.get_arg_list(func)
        res = [self.scope.find(i) if i.type in Scope.default.keys() else prefill.get(i.name, i.name) for i in args]
        params = ', '.join(res)
        return 'CUDNN_CALL(%s(%s));' % (func, params)
    
    def emit_space_wrapper(self, arg, rule):
        is_workspace = 'workspace' in rule.keys()
        callee = rule[['reserve', 'workspace'][is_workspace]]
        requester = 'Request%s' % ['Memory', 'Workspace'][is_workspace]
        if is_workspace:
            self.extra_fields.append('BufferValue workspace;')
        dest = 'workspace' if is_workspace else '%s->data' % rule['res']
        self.write('%s = BufferValue(make_node<BufferNode>());' % dest)
        dest = 'workspace' if is_workspace else 'Downcast<BufferValue>(%s->data)' % rule['res']
        if callee is None:
            callee = 'cudnnGet%sWorkspaceSize' % self.status.exec_func[5:]
        # Get all the parameters ready without emitting the actual callsite
        arg_list, param_list = self._form_wrapper_arglist(callee, rule, {'sizeInBytes': '&sizeInBytes'})
        wrapped_call = self._call_wrapped_func(callee, {'sizeInBytes': '&sizeInBytes'})
        self.extra_wrappers.append(Emitter.size_fmt % (callee[5:], arg_list, wrapped_call))
        self.write('%s->size_in_bytes = %s(%s);' % (dest, callee[5:], param_list))
        self.write('%s(const_cast<void**>(&%s->data), ctx, %s->size_in_bytes);' % (requester, dest, dest))
        self.scope.add_global(arg.name, '%s->data' % dest)
        size = rule.get('size_t', '%sSizeInBytes' % arg.name)
        self.scope.add_global(size, '%s->size_in_bytes' % dest)


    def emit_init_func(self, arg, rule):
        dest = rule.get('dest', arg.name)
        if dest[0] == '$':
            dest = dest[1:]
        if not self.scope.find(arg):
            creator = 'cudnnCreate%s' % arg.type[5:-2]
            self.scope.add(dest, '&%s' % arg.name)
            self.emit_func_call(creator, rule)
        self.scope.add(dest, arg.name)
        self.emit_func_call(rule['callee'], rule)
        self.scope.add_global(arg.name, arg.name)


    def dispatch_dict_rule(self, arg, rules):
        if not isinstance(rules, list):
            rules = [rules]

        for rule in rules:
            self.scope.enter(rule)
            if arg.type.endswith('Algo_t'):
                self.emit_algorithm_wrapper(arg, rule)
            elif arg.type.endswith(Emitter.desc_t):
                self.emit_init_func(arg, rule)
            else:
                self.emit_space_wrapper(arg, rule)
            self.scope.exit()


    def _initialize_arg(self, arg, rule):

        if rule is None:
            if arg.type in [Emitter.filter_desc_t, Emitter.tensor_desc_t]:
                mid = Emitter.strip(arg.type, 'cudnn', Emitter.desc_t)
                idx = self.status.current_arg_idx()
                shape = 'shape_%d' % idx
                stride = 'stride_%d' % idx
                self.write(self.status.current_shape())
                self.write('FORM_STRIDE(%s, %s);' % (stride, shape))
                setter = 'cudnnSet%sNd%s' % (mid, Emitter.desc_t[:-2])
                dest = self.get_arg_list(setter)[0].name
                rule = {'callee'     : setter,
                        'dest'       : '$%s' % dest,
                        dest         : arg.name,
                        'nbDims'     : '%s.size()' % shape,
                        'filterDimA' : 'BeginPtr(%s)' % shape,
                        'dimA'       : 'BeginPtr(%s)' % shape,
                        'strideA'    : 'BeginPtr(%s)' % stride}
            elif self.status.match_last_tensor(arg.name):
                idx = self.status.last_arg_idx()
                tensor = 'dlts[%d]' % idx
                if arg.__str__().startswith('void *'):
                    ptr = 'const_cast<void**>(%s)' % ('&%s->data' % tensor)
                    self.write('RequestMemory(%s, ctx, BytesCompactTensor(*%s));' % (ptr, tensor))
                self.scope.add_global(arg.name, '%s->data' % tensor)
                return self.scope.find(arg)

        if arg.type in [Emitter.filter_desc_t, Emitter.tensor_desc_t]:
            self.status.next_arg(arg.name)

        # This is to handle cuDNN's inconsistency.
        # Assuming all the data buffer pointers and descritptors are in pairs is too strong to be true.
        # For example, in TensorAdd, the arg list is like (xDesc, *x, yDesc, *y), even if x and y are
        # required to be the same shape. However, in divisive normalization, the workspace even borrows
        # tensor descriptor, instead of calling GetSpace APIs.
        # This is to use an ad-hoc solution to solve ad-hoc problem.
        if isinstance(rule, tuple):
            if arg.__str__().startswith('void *'):
                assert rule[0] == 'workspace'
                self.extra_fields.append('void *%s;' % arg.name)
                self.write('RequestWorkspace(&%s, ctx, %s);' % (arg.name, rule[1]))
                self.scope.add_global(arg.name, arg.name)
                return arg.name
            elif arg.type in self.cudnn_enum.keys():
                return rule

        if isinstance(rule, (dict, list)):
            self.dispatch_dict_rule(arg, rule)
            return self.scope.find(arg)


        if isinstance(rule, str):
            self.scope.add_global(arg.name, rule)
            return rule

        assert False, "Failed to init %s..." % arg.__str__()
    
    def initialize_arg_if_not_found(self, arg, rule):
        s = self.scope.find(arg)
        if s is None:
            s = self._initialize_arg(arg, rule.get(arg.name, None))
        enum_literal = self._handle_enum_literal(arg, s)
        if enum_literal:
            self.scope.add_global(arg.name, enum_literal)
            s = enum_literal
        assert s is not None
        return s

    def _emit_func_call(self, func, rule):
        indent = self.indent_ + len(func) + 2 + 10
        res = []
        args = self.get_arg_list(func)

        for arg in args:
            res.append(self.initialize_arg_if_not_found(arg, rule))
        
        return 'CUDNN_CALL(' + func + '(' + (',\n' + indent * ' ').join(res) + '));'


    def emit_func_call(self, func, op_rule):
        self.write(self._emit_func_call(func, op_rule))


    """ Emit the constructor of the given op. """
    def _emit_constructor(self, op_name, op_rule):

        # Emit constructor
        self.write('%s(%s) {' % (self._class_name(), Emitter.args_attrs))
        self.indent(2)

        self.write('args.push_back(info->output);')
        self.write(op_rule.get('dlts', 'std::vector<const DLTensor*> dlts = AsVector(args);'))
        self.write('dtype = DeduceDLType(dlts);')
        self.write('Context ctx = info->ctx;')
        self.write('(void) ctx;')
        if 'attrs_t' in op_rule.keys():
            self.write('auto casted_ptr = attrs.as<attrs::%s>();' % op_rule['attrs_t'])
            self.write('(void) casted_ptr;')
        self.write(op_rule.get('init_extra', ''))

        self.scope = Scope(self.status.op)
        args = self.get_arg_list(self.status.exec_func)
        for arg in args:
            s = self.initialize_arg_if_not_found(arg, op_rule)
            if arg.is_attr_field() and s != arg.name:
                self.write('%s = %s;' % (arg.name, s))
                self.scope.add_global(arg.name, arg.name)

        self.indent(-2)
        self.write('}\n')


    def _emit_openv(self, op_name, op_rule, super_cls='OpEnv'):

        func = op_rule['callee']
        args = self.get_arg_list(func)
        order = op_rule.get('order', list(range(len(args))))
        shapes = op_rule.get('shapes', None)

        self.status = Status(op_name, func, order, shapes)

        self.write('// Frontend operator "mnm.op.%s" dispatch to' % op_name)
        self.emit_func_comments(op_rule['callee'])

        class_name = self._class_name() #('%s_for_op_%s' % (op_rule['callee'][5:], op_name)).replace('.', '_')
        self.write('class %s : public %s {\n public:\n' % (class_name, super_cls))


        self.indent(2)
        # Emit op fileds
        self.write('DType dtype;')
        for arg in args:
            if arg.is_attr_field():
                self.write('%s %s%s;' % (arg.type, arg.is_ptr, arg.name))
        for arg in self.extra_fields:
            self.write(arg)
        self.write('\n')

        # Emit constructor
        self._emit_constructor(op_name, op_rule)

        # Emit destructor
        self.write('~%s() {' % class_name)
        self.indent(2)
        for arg in args:
            if arg.type.endswith(Emitter.desc_t):
                func = 'cudnnDestroy%sDescriptor' % arg.type[len(Emitter.prfx):-len(Emitter.desc_t)]
                assert func in self.cudnn_api.keys()
                self.get_arg_list(func)
                self.write('CUDNN_CALL(%s(%s));' % (func, arg.name))
        self.indent(-2)
        self.write('}\n')

        # Emit execute
        self.write('void Execute(%s) override final {' % Emitter.args_attrs)
        self.indent(2)
        self.write('args.push_back(info->output);')
        self.write(op_rule.get('dlts', 'std::vector<const DLTensor*> dlts = AsVector(args);'))
        if 'attrs_t' in op_rule.keys():
            self.write('auto casted_ptr = attrs.as<attrs::%s>();' % op_rule['attrs_t'])
            self.write('(void) casted_ptr;')
        self.emit_func_call(op_rule['callee'], op_rule)
        # TODO(@were): remove this after stream is done.
        self.write('CUDA_CALL(cudaDeviceSynchronize());')
        self.indent(-2)
        self.write('}\n')

        # Emit maker
        self.write('static OpEnv *make(%s) {' % Emitter.args_attrs)
        self.indent(2)
        self.write('auto res = std::make_unique<%s>(args, info, attrs);' % class_name)
        self.write('return res.release();')
        self.indent(-2)
        self.write('}\n')

        self.indent(-2)
        self.write('};\n')

    def emit_func_comments(self, func):
        indent = self.indent_ + len(func) + 1
        args = [i.__str__() for i in self.get_arg_list(func)]
        self.write('// ' + func + '(' + (',\n// ' + indent * ' ').join(args) + ')')
