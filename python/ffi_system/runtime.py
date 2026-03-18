import ctypes

from .c_api import _LIB, c_str, FunctionHandle
from .config import CASE_EXT_PATH


def _load_case_ext(filename: str):
    import importlib.util

    spec = importlib.util.spec_from_file_location("case_ext", filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def packed_func_creator(handle):
    handle_value = int(handle)
    return matx_script_api.PackedFuncBase(handle_value, 1)


matx_script_api = _load_case_ext(CASE_EXT_PATH)
matx_script_api.set_packedfunc_creator(packed_func_creator)

ObjectBase = matx_script_api.ObjectBase
Any = matx_script_api.Any
get_attr_names_ = matx_script_api.GetGlobal("runtime.NodeGetAttrNames", True)
get_attr_ = matx_script_api.GetGlobal("runtime.NodeGetAttr", True)


class Object(ObjectBase):
    def astext(self):
        fn = matx_script_api.GetGlobal("ast.AsText", True)
        text = fn(self)
        if isinstance(text, (bytes, bytearray)):
            text = text.decode("utf-8")
        return text

    def __dir__(self):
        class_names = dir(self.__class__)
        attr_names = get_attr_names_(self)
        size = len(attr_names)
        return sorted([attr_names[i] for i in range(size)] + class_names)

    def __getattr__(self, name):
        try:
            return get_attr_(self, name)
        except Exception as exc:
            raise AttributeError(
                f"{type(self)} has no attribute {name}"
            ) from exc

    def __str__(self):
        return self.astext()

    def __repr__(self):
        return self.astext()


_CLASS_OBJECT = None


def _set_class_object(object_class):
    global _CLASS_OBJECT
    _CLASS_OBJECT = object_class

    def _creator(handle=None):
        obj = _CLASS_OBJECT.__new__(_CLASS_OBJECT)
        if handle is not None:
            obj.handle = int(handle)
        return obj

    matx_script_api.set_class_object(_creator)


_set_class_object(Object)


def _register_object(index, cls, callback):
    def _creator():
        return cls.__new__(cls)

    matx_script_api.register_object(index, _creator)
    if callback is not None:
        matx_script_api.register_object_callback(index, callback)


def register_object(type_key=None, callback=None):
    object_name = type_key if isinstance(type_key, str) else type_key.__name__

    def register(cls):
        if hasattr(cls, "_type_index"):
            tindex = cls._type_index
        else:
            tidx = ctypes.c_uint()
            _LIB.GetIndex(c_str(object_name), ctypes.byref(tidx))
            tindex = tidx.value
        _register_object(tindex, cls, callback)
        return cls

    if isinstance(type_key, str):
        return register

    return register(type_key)


@register_object("Tuple")
class Tuple(Object):
    def __init__(self, fs):
        new_tuple_ = matx_script_api.GetGlobal("runtime.Tuple", True)
        self.__init_handle_by_constructor__(new_tuple_, *fs)

    def __getitem__(self, idx):
        tuple_get_ = matx_script_api.GetGlobal("runtime.GetTupleField", True)
        return tuple_get_(self, idx)

    def __len__(self):
        tuple_size_ = matx_script_api.GetGlobal("runtime.GetTupleSize", True)
        return tuple_size_(self)


class Node(Object):
    def astext(self):
        fn = matx_script_api.GetGlobal("ast.AsText", True)
        text = fn(self)
        if isinstance(text, (bytes, bytearray)):
            text = text.decode("utf-8")
        return text


class Type(Node):
    pass


prim_type_ = matx_script_api.GetGlobal("ast.PrimType", True)


@register_object("PrimType")
class PrimType(Type):
    def __init__(self, dtype):
        self.__init_handle_by_constructor__(prim_type_, dtype)


class BaseExpr(Node):
    pass


prim_expr_ = matx_script_api.GetGlobal("ast.PrimExpr", True)


@register_object("PrimExpr")
class PrimExpr(BaseExpr):
    def __init__(self, v):
        self.__init_handle_by_constructor__(prim_expr_, v)


class AstExpr(BaseExpr):
    pass


class Stmt(Node):
    pass


op_add_ = matx_script_api.GetGlobal("ast._OpAdd", True)
op_mul_ = matx_script_api.GetGlobal("ast._OpMul", True)
op_sub_ = matx_script_api.GetGlobal("ast._OpSub", True)
op_div_ = matx_script_api.GetGlobal("ast._OpDiv", True)
op_mod_ = matx_script_api.GetGlobal("ast._OpMod", True)
op_eq_ = matx_script_api.GetGlobal("ast._OpEq", True)
op_ne_ = matx_script_api.GetGlobal("ast._OpNe", True)
op_lt_ = matx_script_api.GetGlobal("ast._OpLt", True)
op_le_ = matx_script_api.GetGlobal("ast._OpLe", True)
op_gt_ = matx_script_api.GetGlobal("ast._OpGt", True)
op_ge_ = matx_script_api.GetGlobal("ast._OpGe", True)
op_and_ = matx_script_api.GetGlobal("ast._OpAnd", True)
op_or_ = matx_script_api.GetGlobal("ast._OpOr", True)
op_not_ = matx_script_api.GetGlobal("ast._OpNot", True)


def add(lhs: BaseExpr, rhs: BaseExpr):
    return op_add_(lhs, rhs)


def mul(lhs: BaseExpr, rhs: BaseExpr):
    return op_mul_(lhs, rhs)


def sub(lhs: BaseExpr, rhs: BaseExpr):
    return op_sub_(lhs, rhs)


def div(lhs: BaseExpr, rhs: BaseExpr):
    return op_div_(lhs, rhs)


def mod(lhs: BaseExpr, rhs: BaseExpr):
    return op_mod_(lhs, rhs)


def eq(lhs: BaseExpr, rhs: BaseExpr):
    return op_eq_(lhs, rhs)


def ne(lhs: BaseExpr, rhs: BaseExpr):
    return op_ne_(lhs, rhs)


def lt(lhs: BaseExpr, rhs: BaseExpr):
    return op_lt_(lhs, rhs)


def le(lhs: BaseExpr, rhs: BaseExpr):
    return op_le_(lhs, rhs)


def gt(lhs: BaseExpr, rhs: BaseExpr):
    return op_gt_(lhs, rhs)


def ge(lhs: BaseExpr, rhs: BaseExpr):
    return op_ge_(lhs, rhs)


def logic_and(lhs: BaseExpr, rhs: BaseExpr):
    return op_and_(lhs, rhs)


def logic_or(lhs: BaseExpr, rhs: BaseExpr):
    return op_or_(lhs, rhs)


def logic_not(value: BaseExpr):
    return op_not_(value)


class ExprOp:
    pass


class PrimExprWithOp(ExprOp, PrimExpr):
    pass


prim_var_ = matx_script_api.GetGlobal("ast.PrimVar", True)
prim_call_ = matx_script_api.GetGlobal("ast.PrimCall", True)
prim_call_by_type_ = matx_script_api.GetGlobal("ast.PrimCallByType", True)


@register_object("PrimVar")
class PrimVar(PrimExprWithOp):
    def __init__(self, name, datatype):
        if isinstance(datatype, str):
            datatype = PrimType(datatype)
        self.__init_handle_by_constructor__(prim_var_, name, datatype)


@register_object("PrimCall")
class PrimCall(PrimExpr):
    def __init__(self, datatype, op, gs):
        args_node = gs if isinstance(gs, Array) else Array(gs)
        if isinstance(datatype, str):
            self.__init_handle_by_constructor__(prim_call_by_type_, datatype, op, args_node)
        else:
            self.__init_handle_by_constructor__(prim_call_, datatype, op, args_node)


bool_imm_ = matx_script_api.GetGlobal("ast.Bool", True)


@register_object("Bool")
class Bool(PrimExpr):
    def __init__(self, value):
        self.__init_handle_by_constructor__(bool_imm_, bool(value))


float_imm_ = matx_script_api.GetGlobal("ast.FloatImm", True)


@register_object("FloatImm")
class FloatImm(PrimExpr):
    def __init__(self, value):
        self.__init_handle_by_constructor__(float_imm_, float(value))


null_imm_ = matx_script_api.GetGlobal("ast.NullImm", True)


@register_object("NullImm")
class NullImm(PrimExpr):
    def __init__(self):
        self.__init_handle_by_constructor__(null_imm_)


str_imm_ = matx_script_api.GetGlobal("ast.StrImm", True)


@register_object("StrImm")
class StrImm(PrimExpr):
    def __init__(self, s):
        str_obj = Str(s) if isinstance(s, str) else s
        self.__init_handle_by_constructor__(str_imm_, str_obj)


class_get_item_ = matx_script_api.GetGlobal("ast.ClassGetItem", True)


@register_object("ClassGetItem")
class ClassGetItem(PrimExpr):
    def __init__(self, obj, attr):
        self.__init_handle_by_constructor__(class_get_item_, obj, attr)


list_literal_ = matx_script_api.GetGlobal("ast.ListLiteral", True)


@register_object("ListLiteral")
class ListLiteral(PrimExpr):
    def __init__(self, elements):
        self.__init_handle_by_constructor__(list_literal_, elements)


dict_literal_ = matx_script_api.GetGlobal("ast.DictLiteral", True)


@register_object("DictLiteral")
class DictLiteral(PrimExpr):
    def __init__(self, keys, values):
        self.__init_handle_by_constructor__(dict_literal_, keys, values)


set_literal_ = matx_script_api.GetGlobal("ast.SetLiteral", True)


@register_object("SetLiteral")
class SetLiteral(PrimExpr):
    def __init__(self, elements):
        self.__init_handle_by_constructor__(set_literal_, elements)


container_get_item_ = matx_script_api.GetGlobal("ast.ContainerGetItem", True)


@register_object("ContainerGetItem")
class ContainerGetItem(PrimExpr):
    def __init__(self, obj, index):
        self.__init_handle_by_constructor__(container_get_item_, obj, index)


container_set_item_ = matx_script_api.GetGlobal("ast.ContainerSetItem", True)


@register_object("ContainerSetItem")
class ContainerSetItem(PrimExpr):
    def __init__(self, obj, index, value):
        self.__init_handle_by_constructor__(container_set_item_, obj, index, value)


container_method_call_ = matx_script_api.GetGlobal("ast.ContainerMethodCall", True)


@register_object("ContainerMethodCall")
class ContainerMethodCall(PrimExpr):
    def __init__(self, obj, method, args):
        method_node = method if isinstance(method, StrImm) else StrImm(method)
        args_node = args if isinstance(args, Array) else Array(args)
        self.__init_handle_by_constructor__(container_method_call_, obj, method_node, args_node)


class_stmt_ = matx_script_api.GetGlobal("ast.ClassStmt", True)


@register_object("ClassStmt")
class ClassStmt(Stmt):
    def __init__(self, name, methods):
        self.__init_handle_by_constructor__(class_stmt_, name, methods)


if_stmt_ = matx_script_api.GetGlobal("ast.IfStmt", True)


@register_object("IfStmt")
class IfStmt(Stmt):
    def __init__(self, cond, then_case, else_case):
        self.__init_handle_by_constructor__(if_stmt_, cond, then_case, else_case)


while_stmt_ = matx_script_api.GetGlobal("ast.WhileStmt", True)


@register_object("WhileStmt")
class WhileStmt(Stmt):
    def __init__(self, cond, body):
        self.__init_handle_by_constructor__(while_stmt_, cond, body)


assign_stmt_ = matx_script_api.GetGlobal("ast.AssignStmt", True)


@register_object("AssignStmt")
class AssignStmt(Stmt):
    def __init__(self, lhs, rhs):
        self.__init_handle_by_constructor__(assign_stmt_, lhs, rhs)


evaluate_stmt_ = matx_script_api.GetGlobal("ast.Evaluate", True)


@register_object("Evaluate")
class Evaluate(Stmt):
    def __init__(self, value):
        self.__init_handle_by_constructor__(evaluate_stmt_, value)


allocavar_stmt_ = matx_script_api.GetGlobal("ast.AllocaVarStmt", True)


@register_object("AllocaVarStmt")
class AllocaVarStmt(Stmt):
    def __init__(self, name_hint, type_annotation, init_value=None):
        self.__init_handle_by_constructor__(allocavar_stmt_, name_hint, type_annotation, init_value)


return_stmt_ = matx_script_api.GetGlobal("ast.ReturnStmt", True)


@register_object("ReturnStmt")
class ReturnStmt(Stmt):
    def __init__(self, value):
        self.__init_handle_by_constructor__(return_stmt_, value)


array_ = matx_script_api.GetGlobal("runtime.Array", True)


@register_object("Array")
class Array(Object):
    def __init__(self, seq=()):
        self.__init_handle_by_constructor__(array_, *seq)


str_ = matx_script_api.GetGlobal("runtime.Str", True)


@register_object("RuntimeStr")
class Str(Object):
    def __init__(self, s):
        self.__init_handle_by_constructor__(str_, s)


seq_stmt_ = matx_script_api.GetGlobal("ast.SeqStmt", True)


@register_object("SeqStmt")
class SeqStmt(Stmt):
    def __init__(self, seq):
        self.__init_handle_by_constructor__(seq_stmt_, seq)


func_cp_ = matx_script_api.GetGlobal("ast.BaseFuncCopy", True)
with_attr_ = matx_script_api.GetGlobal("ast.BaseFuncWithAttr", True)


class BaseFunc(AstExpr):
    def with_attr(self, name, value=None):
        copied = func_cp_(self)
        return with_attr_(copied, Str(name), Str(value))


prim_func_ = matx_script_api.GetGlobal("ast.PrimFunc", True)


@register_object("PrimFunc")
class PrimFunc(BaseFunc):
    def __init__(self, gs, fs, body, rt=None):
        self.__init_handle_by_constructor__(prim_func_, gs, fs, body, rt)
