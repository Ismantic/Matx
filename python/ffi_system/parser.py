import ast

from .runtime import (
    add,
    mul,
    sub,
    div,
    mod,
    eq,
    ge,
    gt,
    le,
    lt,
    ne,
    logic_and,
    logic_or,
    logic_not,
    AllocaVarStmt,
    Array,
    AssignStmt,
    Bool,
    ContainerGetItem,
    ContainerMethodCall,
    ContainerSetItem,
    DictLiteral,
    Evaluate,
    FloatImm,
    IfStmt,
    ListLiteral,
    WhileStmt,
    NullImm,
    PrimExpr,
    PrimFunc,
    PrimType,
    PrimVar,
    ReturnStmt,
    SetLiteral,
    SeqStmt,
    StrImm,
)
from .scope_context import ScopeContext


class SimpleParser(ast.NodeVisitor):
    _op_maker = {
        ast.Add: lambda lhs, rhs: add(lhs, rhs),
        ast.Mult: lambda lhs, rhs: mul(lhs, rhs),
        ast.Sub: lambda lhs, rhs: sub(lhs, rhs),
        ast.Div: lambda lhs, rhs: div(lhs, rhs),
        ast.Mod: lambda lhs, rhs: mod(lhs, rhs),
    }
    _ty_maker = {
        "int": lambda: PrimType("int64"),
        "float": lambda: PrimType("float64"),
        "bool": lambda: PrimType("bool"),
        "str": lambda: PrimType("handle"),
        "handle": lambda: PrimType("handle"),
        "None": lambda: PrimType("handle"),
    }
    _cmp_maker = {
        ast.Eq: lambda lhs, rhs: eq(lhs, rhs),
        ast.NotEq: lambda lhs, rhs: ne(lhs, rhs),
        ast.Lt: lambda lhs, rhs: lt(lhs, rhs),
        ast.LtE: lambda lhs, rhs: le(lhs, rhs),
        ast.Gt: lambda lhs, rhs: gt(lhs, rhs),
        ast.GtE: lambda lhs, rhs: ge(lhs, rhs),
    }

    def __init__(self, entry_func_name=None):
        self.context = None
        self.entry_func_name = entry_func_name
        self.functions = {}
        self.function_order = []

        self.class_defs = {}
        self.class_methods = {}
        self.class_attr_types = {}
        self.class_method_sigs = {}
        self.current_var_class_types = {}
        self._tmp_id = 0

    def init_function_parsing_env(self):
        self.context = ScopeContext()

    def resolve_annotation(self, node, allow_none_default=False):
        if node is None:
            if allow_none_default:
                return self._ty_maker["None"]()
            raise RuntimeError("Missing type annotation")
        if isinstance(node, ast.Name):
            return self._ty_maker[node.id]()
        if isinstance(node, ast.Constant) and node.value is None:
            return self._ty_maker["None"]()
        raise RuntimeError("Unsupported type annotation")

    def visit(self, node: ast.AST):
        method = "visit_" + node.__class__.__name__
        visitor = getattr(self, method, self.generic_visit)
        return visitor(node)

    def generic_visit(self, node):
        raise RuntimeError(f"This node is not supported now: {node}")

    def _record_function(self, name, func_ir):
        self.functions[name] = func_ir
        if name not in self.function_order:
            self.function_order.append(name)

    def _new_temp_name(self, prefix):
        while True:
            name = f"{prefix}_{self._tmp_id}"
            self._tmp_id += 1
            if self.context.lookup_symbol(name) is None:
                return name

    def _empty_seq(self):
        return SeqStmt(Array([]))

    def _coerce_bool(self, expr):
        # Normalize to language-level bool (0/1), matching current and/or semantics.
        return logic_not(logic_not(expr))

    def _default_init_for_type(self, ty):
        if ty == "bool":
            return Bool(False)
        if ty == "float64":
            return FloatImm(0.0)
        if ty == "handle":
            return NullImm()
        return PrimExpr(0)

    def _merge_value_type(self, left, right):
        if left == right:
            return left
        if left == "handle" or right == "handle":
            return "handle"
        numeric = {"int64", "float64", "bool"}
        if left in numeric and right in numeric:
            return self._merge_numeric(left, right)
        return "handle"

    def visit_Expr(self, node: ast.Expr):
        call_info = self._match_class_method_call(node.value)
        if call_info is not None:
            class_name, method_name, obj_symbol, arg_nodes = call_info
            stmts, _ = self._expand_class_method(class_name, method_name, obj_symbol, arg_nodes)
            return SeqStmt(Array(stmts))

        value = self.visit(node.value)
        return Evaluate(value)

    def parse_body(self):
        body = []
        while self.context.node_stack[-1]:
            res = self.visit(self.context.node_stack[-1].pop())
            if res is not None:
                body.append(res)
        return SeqStmt(Array(body))

    def visit_Module(self, node):
        self.functions = {}
        self.function_order = []
        self.class_defs = {}
        self.class_methods = {}
        self.class_attr_types = {}
        self.class_method_sigs = {}

        for item in node.body:
            if isinstance(item, ast.ClassDef):
                self.visit(item)

        for item in node.body:
            if isinstance(item, ast.FunctionDef):
                self.visit(item)

        if self.entry_func_name is not None:
            if self.entry_func_name not in self.functions:
                raise RuntimeError(f"Entry function not found: {self.entry_func_name}")
            return self.functions[self.entry_func_name]

        if len(self.functions) == 1:
            only_name = next(iter(self.functions))
            return self.functions[only_name]

        raise RuntimeError("Module must contain exactly one function, or set entry_func_name")

    def visit_ClassDef(self, node: ast.ClassDef):
        methods = {}
        for item in node.body:
            if isinstance(item, ast.FunctionDef):
                methods[item.name] = item
        self.class_defs[node.name] = node
        self.class_methods[node.name] = methods
        self.class_attr_types[node.name] = self._infer_class_attr_types(node, methods)
        self.class_method_sigs[node.name] = self._infer_class_method_sigs(methods)
        return None

    def _annotation_type_name(self, ann, default="handle", allow_none=False):
        if ann is None:
            return default
        if isinstance(ann, ast.Name):
            if ann.id == "int":
                return "int64"
            if ann.id == "float":
                return "float64"
            if ann.id == "bool":
                return "bool"
            if ann.id in ("str", "handle"):
                return "handle"
            return "handle"
        if allow_none and isinstance(ann, ast.Constant) and ann.value is None:
            return "handle"
        return "handle"

    def _infer_class_method_sigs(self, methods):
        out = {}
        for method_name, method in methods.items():
            if not method.args.args or method.args.args[0].arg != "self":
                continue
            params = []
            for arg in method.args.args[1:]:
                params.append(
                    self._annotation_type_name(
                        arg.annotation,
                        default="handle",
                        allow_none=True,
                    )
                )
            ret = self._annotation_type_name(
                method.returns,
                default="handle",
                allow_none=True,
            )
            out[method_name] = {"params": params, "ret": ret}
        return out

    def _infer_class_attr_types(self, _class_node, methods):
        attrs = {}
        init_method = methods.get("__init__")
        if init_method is None:
            return attrs

        param_types = {}
        for idx, arg in enumerate(init_method.args.args):
            if idx == 0 and arg.arg == "self":
                continue
            param_types[arg.arg] = self._annotation_type_name(
                arg.annotation,
                default="handle",
                allow_none=True,
            )

        for stmt in init_method.body:
            if not isinstance(stmt, ast.Assign) or len(stmt.targets) != 1:
                continue
            tgt = stmt.targets[0]
            if not isinstance(tgt, ast.Attribute):
                continue
            if not isinstance(tgt.value, ast.Name) or tgt.value.id != "self":
                continue

            v = stmt.value
            inferred = "handle"
            if isinstance(v, ast.Name):
                inferred = param_types.get(v.id, "handle")
            elif isinstance(v, ast.Constant):
                if isinstance(v.value, bool):
                    inferred = "bool"
                elif isinstance(v.value, int):
                    inferred = "int64"
                elif isinstance(v.value, float):
                    inferred = "float64"
                else:
                    inferred = "handle"
            attrs[tgt.attr] = inferred
        return attrs

    def visit_FunctionDef(self, node: ast.FunctionDef):
        prev_var_classes = self.current_var_class_types
        prev_tmp_id = self._tmp_id

        self.init_function_parsing_env()
        self.context.new_scope(nodes=node.body)
        self.current_var_class_types = {}
        self._tmp_id = 0

        for arg in node.args.args:
            var_type = self.resolve_annotation(arg.annotation)
            arg_var = PrimVar(arg.arg, var_type)
            self.context.update_symbol(arg.arg, arg_var, str(var_type))
            self.context.func_params.append(arg_var)

        ret_type = self.resolve_annotation(node.returns)
        self.context.func_ret_type = ret_type

        func = PrimFunc(
            Array(self.context.func_params),
            Array([]),
            self.parse_body(),
            ret_type,
        )
        func = func.with_attr("GlobalSymbol", node.name)
        self._record_function(node.name, func)

        self.context.pop_scope()
        self.current_var_class_types = prev_var_classes
        self._tmp_id = prev_tmp_id
        return func

    def _match_class_method_call(self, node):
        if not isinstance(node, ast.Call):
            return None
        if not isinstance(node.func, ast.Attribute):
            return None
        if not isinstance(node.func.value, ast.Name):
            return None

        obj_name = node.func.value.id
        class_name = self.current_var_class_types.get(obj_name)
        if class_name is None:
            return None

        obj_symbol = self.context.lookup_symbol(obj_name)
        if obj_symbol is None:
            return None
        return class_name, node.func.attr, obj_symbol, node.args

    def _visit_expr_lowered(self, node):
        call_info = self._match_class_method_call(node)
        if call_info is not None:
            class_name, method_name, obj_symbol, arg_nodes = call_info
            return self._expand_class_method(class_name, method_name, obj_symbol, arg_nodes)

        if isinstance(node, ast.BinOp):
            l_stmts, lhs = self._visit_expr_lowered(node.left)
            r_stmts, rhs = self._visit_expr_lowered(node.right)
            op = self._op_maker[type(node.op)]
            return l_stmts + r_stmts, op(lhs, rhs)

        if isinstance(node, ast.Compare):
            if len(node.ops) != len(node.comparators):
                raise RuntimeError("Compare ops and comparators mismatch")
            stmts, left = self._visit_expr_lowered(node.left)
            parts = []
            for op_node, comp in zip(node.ops, node.comparators):
                c_stmts, right = self._visit_expr_lowered(comp)
                stmts.extend(c_stmts)
                op = self._cmp_maker[type(op_node)]
                parts.append(op(left, right))
                left = right
            expr = parts[0]
            for part in parts[1:]:
                expr = logic_and(expr, part)
            return stmts, expr

        if isinstance(node, ast.BoolOp):
            if len(node.values) < 2:
                raise RuntimeError("BoolOp requires at least two operands")

            first_stmts, first_expr = self._visit_expr_lowered(node.values[0])
            tmp_name = self._new_temp_name("__boolop")
            tmp_init = AllocaVarStmt(tmp_name, "bool", self._coerce_bool(first_expr))
            self.context.update_symbol(tmp_name, tmp_init.var, "bool")
            tmp_var = tmp_init.var

            stmts = list(first_stmts)
            stmts.append(tmp_init)

            if isinstance(node.op, ast.And):
                for v in node.values[1:]:
                    v_stmts, v_expr = self._visit_expr_lowered(v)
                    then_seq = SeqStmt(Array(v_stmts + [AssignStmt(tmp_var, self._coerce_bool(v_expr))]))
                    stmts.append(IfStmt(tmp_var, then_seq, self._empty_seq()))
                return stmts, tmp_var

            if isinstance(node.op, ast.Or):
                for v in node.values[1:]:
                    v_stmts, v_expr = self._visit_expr_lowered(v)
                    else_seq = SeqStmt(Array(v_stmts + [AssignStmt(tmp_var, self._coerce_bool(v_expr))]))
                    stmts.append(IfStmt(tmp_var, self._empty_seq(), else_seq))
                return stmts, tmp_var
            raise RuntimeError("Unsupported BoolOp")

        if isinstance(node, ast.IfExp):
            cond_stmts, cond_expr = self._visit_expr_lowered(node.test)
            then_stmts, then_expr = self._visit_expr_lowered(node.body)
            else_stmts, else_expr = self._visit_expr_lowered(node.orelse)

            then_ty = self.infer_type(then_expr)
            else_ty = self.infer_type(else_expr)
            out_ty = self._merge_value_type(then_ty, else_ty)

            tmp_name = self._new_temp_name("__ifexp")
            tmp_init = AllocaVarStmt(tmp_name, out_ty, self._default_init_for_type(out_ty))
            self.context.update_symbol(tmp_name, tmp_init.var, out_ty)

            then_seq = SeqStmt(Array(then_stmts + [AssignStmt(tmp_init.var, then_expr)]))
            else_seq = SeqStmt(Array(else_stmts + [AssignStmt(tmp_init.var, else_expr)]))
            if_stmt = IfStmt(cond_expr, then_seq, else_seq)
            return cond_stmts + [tmp_init, if_stmt], tmp_init.var

        if isinstance(node, ast.UnaryOp):
            s_stmts, operand_expr = self._visit_expr_lowered(node.operand)
            if isinstance(node.op, ast.Not):
                return s_stmts, logic_not(operand_expr)
            if isinstance(node.op, ast.UAdd):
                return s_stmts, operand_expr
            if isinstance(node.op, ast.USub):
                return s_stmts, sub(PrimExpr(0), operand_expr)
            raise RuntimeError("Unsupported UnaryOp")

        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute):
            obj_stmts, obj = self._visit_expr_lowered(node.func.value)
            arg_stmts = []
            args = []
            for arg in node.args:
                s, e = self._visit_expr_lowered(arg)
                arg_stmts.extend(s)
                args.append(e)
            return obj_stmts + arg_stmts, ContainerMethodCall(obj, node.func.attr, Array(args))

        if isinstance(node, ast.Subscript):
            obj_stmts, obj = self._visit_expr_lowered(node.value)
            index_node = node.slice
            if isinstance(index_node, ast.Slice):
                raise RuntimeError("Slice is not supported")
            idx_stmts, index = self._visit_expr_lowered(index_node)
            return obj_stmts + idx_stmts, ContainerGetItem(obj, index)

        if isinstance(node, ast.List):
            stmts = []
            elements = []
            for elt in node.elts:
                e_stmts, e_expr = self._visit_expr_lowered(elt)
                stmts.extend(e_stmts)
                elements.append(e_expr)
            return stmts, ListLiteral(Array(elements))

        if isinstance(node, ast.Set):
            stmts = []
            elements = []
            for elt in node.elts:
                e_stmts, e_expr = self._visit_expr_lowered(elt)
                stmts.extend(e_stmts)
                elements.append(e_expr)
            return stmts, SetLiteral(Array(elements))

        if isinstance(node, ast.Dict):
            stmts = []
            keys = []
            values = []
            for key, value in zip(node.keys, node.values):
                if key is None:
                    raise RuntimeError("Dict unpacking is not supported")
                k_stmts, k_expr = self._visit_expr_lowered(key)
                v_stmts, v_expr = self._visit_expr_lowered(value)
                stmts.extend(k_stmts)
                stmts.extend(v_stmts)
                keys.append(k_expr)
                values.append(v_expr)
            return stmts, DictLiteral(Array(keys), Array(values))

        return [], self.visit(node)

    def _expand_class_method(self, class_name, method_name, obj_symbol, arg_nodes):
        methods = self.class_methods.get(class_name)
        if methods is None or method_name not in methods:
            raise RuntimeError(f"Unknown method: {class_name}.{method_name}")

        method = methods[method_name]
        if not method.args.args or method.args.args[0].arg != "self":
            raise RuntimeError(f"Method {class_name}.{method_name} must have self")

        if len(arg_nodes) != len(method.args.args) - 1:
            raise RuntimeError(
                f"{class_name}.{method_name} expects {len(method.args.args) - 1} args"
            )

        sig = self.class_method_sigs.get(class_name, {}).get(method_name, None)

        stmts = []
        prev_var_classes = self.current_var_class_types
        self.current_var_class_types = dict(self.current_var_class_types)

        self.context.new_scope(nodes=[])
        self.context.update_symbol("self", obj_symbol, "handle")
        self.current_var_class_types["self"] = class_name

        for idx, (param, arg_node) in enumerate(zip(method.args.args[1:], arg_nodes)):
            arg_stmts, value = self._visit_expr_lowered(arg_node)
            stmts.extend(arg_stmts)
            actual_ty = self.infer_type(value)
            if sig is not None:
                expected_ty = sig["params"][idx]
                if not self._is_type_compatible(expected_ty, actual_ty):
                    raise RuntimeError(
                        f"Type mismatch in {class_name}.{method_name} arg {idx + 1}: "
                        f"expected {expected_ty}, got {actual_ty}"
                    )
            tmp_name = self._new_temp_name(f"__{class_name}_{method_name}_arg_{idx}")
            tmp_stmt = AllocaVarStmt(tmp_name, actual_ty, value)
            stmts.append(tmp_stmt)
            self.context.update_symbol(param.arg, tmp_stmt.var, actual_ty)

        expected_ret_ty = "handle"
        if sig is not None:
            expected_ret_ty = sig["ret"]

        ret_name = self._new_temp_name(f"__{class_name}_{method_name}_ret")
        if expected_ret_ty == "handle":
            ret_init = obj_symbol
        else:
            ret_init = self._default_init_for_type(expected_ret_ty)
        ret_slot = AllocaVarStmt(ret_name, expected_ret_ty, ret_init)
        stmts.append(ret_slot)
        self.context.update_symbol(ret_name, ret_slot.var, expected_ret_ty)

        has_ret_name = self._new_temp_name(f"__{class_name}_{method_name}_has_ret")
        has_ret_slot = AllocaVarStmt(has_ret_name, "bool", Bool(False))
        stmts.append(has_ret_slot)
        self.context.update_symbol(has_ret_name, has_ret_slot.var, "bool")

        body_stmt = self._lower_method_body_with_return(
            method.body,
            ret_slot.var,
            has_ret_slot.var,
            expected_ret_ty,
            class_name,
            method_name,
        )
        stmts.append(body_stmt)

        self.context.pop_scope()
        self.current_var_class_types = prev_var_classes
        return stmts, ret_slot.var

    def _guard_stmt_with_has_ret(self, stmt, has_ret_var):
        return IfStmt(logic_not(has_ret_var), stmt, self._empty_seq())

    def _lower_method_body_with_return(
        self,
        body_nodes,
        ret_var,
        has_ret_var,
        expected_ret_ty,
        class_name,
        method_name,
    ):
        seq = []
        for stmt_node in body_nodes:
            lowered = self._lower_method_stmt_with_return(
                stmt_node,
                ret_var,
                has_ret_var,
                expected_ret_ty,
                class_name,
                method_name,
            )
            for stmt in lowered:
                if isinstance(stmt, AllocaVarStmt):
                    seq.append(stmt)
                else:
                    seq.append(self._guard_stmt_with_has_ret(stmt, has_ret_var))
        return SeqStmt(Array(seq))

    def _lower_method_stmt_with_return(
        self,
        stmt_node,
        ret_var,
        has_ret_var,
        expected_ret_ty,
        class_name,
        method_name,
    ):
        if isinstance(stmt_node, ast.Return):
            if stmt_node.value is None:
                ret_stmts = []
                ret_expr = NullImm()
            else:
                ret_stmts, ret_expr = self._visit_expr_lowered(stmt_node.value)
            actual_ret_ty = self.infer_type(ret_expr)
            if not self._is_type_compatible(expected_ret_ty, actual_ret_ty):
                raise RuntimeError(
                    f"Type mismatch in return of {class_name}.{method_name}: "
                    f"expected {expected_ret_ty}, got {actual_ret_ty}"
                )
            seq = list(ret_stmts)
            seq.append(AssignStmt(ret_var, ret_expr))
            seq.append(AssignStmt(has_ret_var, Bool(True)))
            return [SeqStmt(Array(seq))]

        if isinstance(stmt_node, ast.Assign):
            if len(stmt_node.targets) != 1:
                raise RuntimeError("Only single-target assignment is supported")
            lhs_node = stmt_node.targets[0]
            if isinstance(lhs_node, ast.Name):
                rhs_stmts, rhs = self._visit_expr_lowered(stmt_node.value)
                pre = []
                symbol = self.context.lookup_symbol(lhs_node.id)
                if symbol is None:
                    inf_ty = self.infer_type(rhs)
                    alloca_stmt = AllocaVarStmt(
                        lhs_node.id, inf_ty, self._default_init_for_type(inf_ty)
                    )
                    self.context.update_symbol(lhs_node.id, alloca_stmt.var, inf_ty)
                    symbol = alloca_stmt.var
                    pre.append(alloca_stmt)
                else:
                    self.ensure_assignable(lhs_node.id, self.infer_type(rhs))
                self.current_var_class_types.pop(lhs_node.id, None)
                assign_seq = SeqStmt(Array(list(rhs_stmts) + [AssignStmt(symbol, rhs)]))
                return pre + [assign_seq]

        if isinstance(stmt_node, ast.If):
            cond_stmts, cond = self._visit_expr_lowered(stmt_node.test)
            self.context.new_scope(nodes=[])
            then_case = self._lower_method_body_with_return(
                stmt_node.body,
                ret_var,
                has_ret_var,
                expected_ret_ty,
                class_name,
                method_name,
            )
            self.context.pop_scope()
            self.context.new_scope(nodes=[])
            else_case = self._lower_method_body_with_return(
                stmt_node.orelse,
                ret_var,
                has_ret_var,
                expected_ret_ty,
                class_name,
                method_name,
            )
            self.context.pop_scope()
            out = list(cond_stmts)
            out.append(IfStmt(cond, then_case, else_case))
            return out

        if isinstance(stmt_node, ast.While):
            if stmt_node.orelse:
                raise RuntimeError("while-else is not supported")
            prime_stmts, prime_cond = self._visit_expr_lowered(stmt_node.test)
            self.context.new_scope(nodes=[])
            body = self._lower_method_body_with_return(
                stmt_node.body,
                ret_var,
                has_ret_var,
                expected_ret_ty,
                class_name,
                method_name,
            )
            self.context.pop_scope()
            if not prime_stmts:
                cond = logic_and(logic_not(has_ret_var), prime_cond)
                return [WhileStmt(cond, body)]

            update_stmts, update_cond = self._visit_expr_lowered(stmt_node.test)
            cond_type = self.infer_type(prime_cond)
            init_value = self._default_init_for_type(cond_type)
            cond_name = self._new_temp_name("__while_cond")
            cond_init = self.lookup_or_alloca(cond_name, init_value)
            cond_var = self.context.lookup_symbol(cond_name)
            prime = list(prime_stmts) + [AssignStmt(cond_var, prime_cond)]
            update = list(update_stmts) + [AssignStmt(cond_var, update_cond)]
            update_seq = SeqStmt(Array(update))
            while_body = SeqStmt(Array([body, update_seq]))
            cond = logic_and(logic_not(has_ret_var), cond_var)
            while_stmt = WhileStmt(cond, while_body)
            return [SeqStmt(Array([cond_init] + prime + [while_stmt]))]

        if isinstance(stmt_node, ast.For):
            if stmt_node.orelse:
                raise RuntimeError("for-else is not supported")
            if not isinstance(stmt_node.iter, ast.Call) or not isinstance(stmt_node.iter.func, ast.Name):
                raise RuntimeError("Only for-in-range is supported")
            if stmt_node.iter.func.id != "range":
                raise RuntimeError("Only for-in-range is supported")
            if not isinstance(stmt_node.target, ast.Name):
                raise RuntimeError("Only simple for loop targets are supported")

            args = stmt_node.iter.args
            prefix_stmts = []

            def eval_once(arg_node, tag):
                arg_stmts, arg_expr = self._visit_expr_lowered(arg_node)
                local = list(arg_stmts)
                tmp_name = self._new_temp_name(f"__range_{tag}")
                tmp_stmt = AllocaVarStmt(tmp_name, self.infer_type(arg_expr), arg_expr)
                self.context.update_symbol(tmp_name, tmp_stmt.var, self.infer_type(arg_expr))
                local.append(tmp_stmt)
                return local, tmp_stmt.var

            if len(args) == 1:
                start = PrimExpr(0)
                end_stmts, end = eval_once(args[0], "end")
                prefix_stmts.extend(end_stmts)
                step = PrimExpr(1)
            elif len(args) == 2:
                start_stmts, start = eval_once(args[0], "start")
                end_stmts, end = eval_once(args[1], "end")
                prefix_stmts.extend(start_stmts)
                prefix_stmts.extend(end_stmts)
                step = PrimExpr(1)
            elif len(args) == 3:
                start_stmts, start = eval_once(args[0], "start")
                end_stmts, end = eval_once(args[1], "end")
                step_stmts, step = eval_once(args[2], "step")
                prefix_stmts.extend(start_stmts)
                prefix_stmts.extend(end_stmts)
                prefix_stmts.extend(step_stmts)
            else:
                raise RuntimeError("range() expects 1-3 arguments")

            init = self.lookup_or_alloca(stmt_node.target.id, start)
            loop_var = self.context.lookup_symbol(stmt_node.target.id)
            step_is_neg = lt(step, PrimExpr(0))
            cond_pos = lt(loop_var, end)
            cond_neg = gt(loop_var, end)
            cond = self._build_if_expr(step_is_neg, cond_neg, cond_pos)

            self.context.new_scope(nodes=[])
            body_stmt = self._lower_method_body_with_return(
                stmt_node.body,
                ret_var,
                has_ret_var,
                expected_ret_ty,
                class_name,
                method_name,
            )
            self.context.pop_scope()
            inc = AssignStmt(loop_var, add(loop_var, step))
            body_seq = SeqStmt(Array([body_stmt, inc]))
            while_stmt = WhileStmt(logic_and(logic_not(has_ret_var), cond), body_seq)
            return [SeqStmt(Array(prefix_stmts + [init, while_stmt]))]

        stmt_ir = self.visit(stmt_node)
        if stmt_ir is None:
            return []
        return [stmt_ir]

    def visit_Return(self, node):
        stmts, ret_expr = self._visit_expr_lowered(node.value)
        if stmts:
            return SeqStmt(Array(stmts + [ReturnStmt(ret_expr)]))
        return ReturnStmt(ret_expr)

    def visit_Pass(self, _node):
        return SeqStmt(Array([]))

    def lookup_or_alloca(self, name_hint, init_value):
        inf_ty = self.infer_type(init_value)
        symbol = self.context.lookup_symbol(name_hint)
        if symbol is None:
            alloca_stmt = AllocaVarStmt(name_hint, inf_ty, init_value)
            self.context.update_symbol(name_hint, alloca_stmt.var, inf_ty)
            return alloca_stmt
        self.ensure_assignable(name_hint, inf_ty)
        return AssignStmt(symbol, init_value)

    def infer_type(self, value):
        return self._infer_expr_type(value)

    def ensure_assignable(self, name_hint, inferred):
        existing = self.context.lookup_type(name_hint)
        if existing is None:
            return
        if self._is_type_compatible(existing, inferred):
            return
        raise RuntimeError(
            f"Type mismatch: variable '{name_hint}' expected {existing}, got {inferred}"
        )

    def _is_type_compatible(self, expected, actual):
        if expected == actual:
            return True
        if expected == "handle":
            return True
        return False

    def _infer_expr_type(self, value):
        if isinstance(value, Bool):
            return "bool"
        if isinstance(value, FloatImm):
            return "float64"
        if isinstance(value, ContainerGetItem):
            dtype = getattr(value, "datatype", None)
            if dtype is not None:
                dtype_str = str(dtype)
                if dtype_str.startswith("int"):
                    return "int64"
                if dtype_str.startswith("float"):
                    return "float64"
                if dtype_str == "bool":
                    return "bool"
                if dtype_str == "handle":
                    return "handle"
            return "handle"
        if isinstance(
            value,
            (
                StrImm,
                ListLiteral,
                DictLiteral,
                SetLiteral,
                ContainerMethodCall,
                NullImm,
            ),
        ):
            return "handle"

        name = value.__class__.__name__
        if name in ("PrimEq", "PrimNe", "PrimLt", "PrimLe", "PrimGt", "PrimGe"):
            return "bool"
        if name in ("PrimAnd", "PrimOr", "PrimNot"):
            return "bool"
        if name in ("PrimAdd", "PrimSub", "PrimMul", "PrimDiv", "PrimMod"):
            left = self._infer_expr_type(getattr(value, "a", None))
            right = self._infer_expr_type(getattr(value, "b", None))
            return self._merge_numeric(left, right)

        dtype = getattr(value, "datatype", None)
        if dtype is not None:
            dtype_str = str(dtype)
            if dtype_str.startswith("float"):
                return "float64"
            if dtype_str == "bool":
                return "bool"
            if dtype_str == "handle":
                return "handle"
        return "int64"

    def _merge_numeric(self, left, right):
        if left == "handle" or right == "handle":
            return "handle"
        if left == "float64" or right == "float64":
            return "float64"
        if left == "bool" and right == "bool":
            return "bool"
        if left == "bool" and right == "int64":
            return "int64"
        if left == "int64" and right == "bool":
            return "int64"
        return "int64"

    def _lower_ctor_assign(self, lhs_name, class_name, arg_nodes):
        methods = self.class_methods.get(class_name, {})
        init_method = methods.get("__init__")
        if init_method is None:
            raise RuntimeError(f"Class {class_name} missing __init__")

        empty_obj = DictLiteral(Array([]), Array([]))
        init_stmt = self.lookup_or_alloca(lhs_name, empty_obj)
        self.current_var_class_types[lhs_name] = class_name

        obj_symbol = self.context.lookup_symbol(lhs_name)
        ctor_stmts, _ = self._expand_class_method(
            class_name, "__init__", obj_symbol, arg_nodes
        )
        return SeqStmt(Array([init_stmt] + ctor_stmts))

    def visit_Assign(self, node: ast.Assign):
        if len(node.targets) != 1:
            raise RuntimeError("Only one-valued assignment is supported now")

        lhs_node = node.targets[0]

        if isinstance(lhs_node, ast.Name):
            if isinstance(node.value, ast.Call) and isinstance(node.value.func, ast.Name):
                class_name = node.value.func.id
                if class_name in self.class_defs:
                    return self._lower_ctor_assign(lhs_node.id, class_name, node.value.args)

        rhs_stmts, rhs_value = self._visit_expr_lowered(node.value)

        if isinstance(lhs_node, ast.Name):
            stmt = self.lookup_or_alloca(lhs_node.id, rhs_value)
            self.current_var_class_types.pop(lhs_node.id, None)
            if rhs_stmts:
                return SeqStmt(Array(rhs_stmts + [stmt]))
            return stmt

        if isinstance(lhs_node, ast.Subscript):
            target_obj, target_index = self.parse_subscript(lhs_node)
            stmt = Evaluate(ContainerSetItem(target_obj, target_index, rhs_value))
            if rhs_stmts:
                return SeqStmt(Array(rhs_stmts + [stmt]))
            return stmt

        if isinstance(lhs_node, ast.Attribute):
            if isinstance(lhs_node.value, ast.Name):
                base_name = lhs_node.value.id
                class_name = self.current_var_class_types.get(base_name)
                if class_name is not None:
                    target_obj = self.visit(lhs_node.value)
                    target_index = StrImm(lhs_node.attr)
                    attr_types = self.class_attr_types.get(class_name, {})
                    expected_attr_ty = attr_types.get(lhs_node.attr)
                    actual_attr_ty = self.infer_type(rhs_value)
                    if expected_attr_ty is not None and not self._is_type_compatible(
                        expected_attr_ty, actual_attr_ty
                    ):
                        raise RuntimeError(
                            f"Type mismatch for {class_name}.{lhs_node.attr}: "
                            f"expected {expected_attr_ty}, got {actual_attr_ty}"
                        )
                    stmt = Evaluate(ContainerSetItem(target_obj, target_index, rhs_value))
                    if rhs_stmts:
                        return SeqStmt(Array(rhs_stmts + [stmt]))
                    return stmt
            raise RuntimeError("Unsupported assignment target")

        raise RuntimeError("Unsupported assignment target")

    def visit_BinOp(self, node):
        lhs = self.visit(node.left)
        rhs = self.visit(node.right)
        op = self._op_maker[type(node.op)]
        return op(lhs, rhs)

    def visit_Compare(self, node: ast.Compare):
        if len(node.ops) != len(node.comparators):
            raise RuntimeError("Compare ops and comparators mismatch")
        left = self.visit(node.left)
        parts = []
        for op_node, comp in zip(node.ops, node.comparators):
            right = self.visit(comp)
            op = self._cmp_maker[type(op_node)]
            parts.append(op(left, right))
            left = right
        expr = parts[0]
        for part in parts[1:]:
            expr = logic_and(expr, part)
        return expr

    def visit_BoolOp(self, node: ast.BoolOp):
        if len(node.values) < 2:
            raise RuntimeError("BoolOp requires at least two operands")
        values = [self.visit(v) for v in node.values]
        if isinstance(node.op, ast.And):
            expr = values[0]
            for value in values[1:]:
                expr = logic_and(expr, value)
            return expr
        if isinstance(node.op, ast.Or):
            expr = values[0]
            for value in values[1:]:
                expr = logic_or(expr, value)
            return expr
        raise RuntimeError("Unsupported BoolOp")

    def visit_UnaryOp(self, node: ast.UnaryOp):
        if isinstance(node.op, ast.Not):
            return logic_not(self.visit(node.operand))
        if isinstance(node.op, ast.UAdd):
            return self.visit(node.operand)
        if isinstance(node.op, ast.USub):
            return sub(PrimExpr(0), self.visit(node.operand))
        raise RuntimeError("Unsupported UnaryOp")

    def visit_If(self, node: ast.If):
        cond_stmts, cond = self._visit_expr_lowered(node.test)
        self.context.new_scope(nodes=node.body)
        then_case = self.parse_body()
        self.context.pop_scope()

        if node.orelse:
            self.context.new_scope(nodes=node.orelse)
            else_case = self.parse_body()
            self.context.pop_scope()
        else:
            else_case = SeqStmt(Array([]))
        if_stmt = IfStmt(cond, then_case, else_case)
        if cond_stmts:
            return SeqStmt(Array(cond_stmts + [if_stmt]))
        return if_stmt

    def visit_While(self, node: ast.While):
        if node.orelse:
            raise RuntimeError("while-else is not supported")
        prime_stmts, prime_cond = self._visit_expr_lowered(node.test)
        self.context.new_scope(nodes=node.body)
        body = self.parse_body()
        self.context.pop_scope()
        if not prime_stmts:
            cond = prime_cond
            return WhileStmt(cond, body)

        update_stmts, update_cond = self._visit_expr_lowered(node.test)

        cond_type = self.infer_type(prime_cond)
        init_value = self._default_init_for_type(cond_type)

        cond_name = self._new_temp_name("__while_cond")
        cond_init = self.lookup_or_alloca(cond_name, init_value)
        cond_var = self.context.lookup_symbol(cond_name)

        prime = prime_stmts + [AssignStmt(cond_var, prime_cond)]
        update = update_stmts + [AssignStmt(cond_var, update_cond)]
        update_seq = SeqStmt(Array(update))
        while_body = SeqStmt(Array([body, update_seq]))
        while_stmt = WhileStmt(cond_var, while_body)
        return SeqStmt(Array([cond_init] + prime + [while_stmt]))

    def visit_For(self, node: ast.For):
        if node.orelse:
            raise RuntimeError("for-else is not supported")
        if not isinstance(node.iter, ast.Call) or not isinstance(node.iter.func, ast.Name):
            raise RuntimeError("Only for-in-range is supported")
        if node.iter.func.id != "range":
            raise RuntimeError("Only for-in-range is supported")

        args = node.iter.args
        prefix_stmts = []

        def eval_once(arg_node, tag):
            arg_stmts, arg_expr = self._visit_expr_lowered(arg_node)
            stmts = list(arg_stmts)
            tmp_name = self._new_temp_name(f"__range_{tag}")
            tmp_stmt = AllocaVarStmt(tmp_name, self.infer_type(arg_expr), arg_expr)
            self.context.update_symbol(tmp_name, tmp_stmt.var, self.infer_type(arg_expr))
            stmts.append(tmp_stmt)
            return stmts, tmp_stmt.var

        if len(args) == 1:
            start = PrimExpr(0)
            end_stmts, end = eval_once(args[0], "end")
            prefix_stmts.extend(end_stmts)
            step = PrimExpr(1)
        elif len(args) == 2:
            start_stmts, start = eval_once(args[0], "start")
            end_stmts, end = eval_once(args[1], "end")
            prefix_stmts.extend(start_stmts)
            prefix_stmts.extend(end_stmts)
            step = PrimExpr(1)
        elif len(args) == 3:
            start_stmts, start = eval_once(args[0], "start")
            end_stmts, end = eval_once(args[1], "end")
            step_stmts, step = eval_once(args[2], "step")
            prefix_stmts.extend(start_stmts)
            prefix_stmts.extend(end_stmts)
            prefix_stmts.extend(step_stmts)
        else:
            raise RuntimeError("range() expects 1-3 arguments")

        if not isinstance(node.target, ast.Name):
            raise RuntimeError("Only simple for loop targets are supported")

        init = self.lookup_or_alloca(node.target.id, start)

        loop_var = self.context.lookup_symbol(node.target.id)
        step_is_neg = lt(step, PrimExpr(0))
        cond_pos = lt(loop_var, end)
        cond_neg = gt(loop_var, end)
        cond = self._build_if_expr(step_is_neg, cond_neg, cond_pos)

        self.context.new_scope(nodes=node.body)
        body_stmt = self.parse_body()
        self.context.pop_scope()

        inc = AssignStmt(loop_var, add(loop_var, step))
        body_seq = SeqStmt(Array([body_stmt, inc]))

        while_stmt = WhileStmt(cond, body_seq)
        return SeqStmt(Array(prefix_stmts + [init, while_stmt]))

    def visit_Name(self, node):
        sym = self.context.lookup_symbol(node.id)
        if sym is None:
            raise RuntimeError(f"Unknown variable: {node.id}")
        return sym

    def visit_Attribute(self, node: ast.Attribute):
        obj = self.visit(node.value)
        if isinstance(node.value, ast.Name):
            base_name = node.value.id
            class_name = self.current_var_class_types.get(base_name)
            if class_name is not None:
                expr = ContainerGetItem(obj, StrImm(node.attr))
                attr_types = self.class_attr_types.get(class_name, {})
                dtype = attr_types.get(node.attr)
                if dtype is not None:
                    expr.datatype = PrimType(dtype)
                return expr
        if self.infer_type(obj) == "handle":
            return ContainerGetItem(obj, StrImm(node.attr))
        raise RuntimeError("Only class attribute access is supported")

    def visit_Constant(self, node: ast.Constant):
        if isinstance(node.value, bool):
            return Bool(node.value)
        if isinstance(node.value, int):
            return PrimExpr(node.value)
        if isinstance(node.value, float):
            return FloatImm(node.value)
        if isinstance(node.value, str):
            return StrImm(node.value)
        if node.value is None:
            return NullImm()
        raise RuntimeError(f"Unsupported constant: {node.value!r}")

    def visit_Call(self, node: ast.Call):
        if isinstance(node.func, ast.Attribute):
            obj = self.visit(node.func.value)
            method = node.func.attr
            args = [self.visit(arg) for arg in node.args]
            return ContainerMethodCall(obj, method, Array(args))
        raise RuntimeError("Only container method calls are supported")

    def visit_List(self, node: ast.List):
        elements = [self.visit(e) for e in node.elts]
        return ListLiteral(Array(elements))

    def visit_Set(self, node: ast.Set):
        elements = [self.visit(e) for e in node.elts]
        return SetLiteral(Array(elements))

    def visit_Dict(self, node: ast.Dict):
        keys = []
        values = []
        for key, value in zip(node.keys, node.values):
            if key is None:
                raise RuntimeError("Dict unpacking is not supported")
            keys.append(self.visit(key))
            values.append(self.visit(value))
        return DictLiteral(Array(keys), Array(values))

    def visit_Subscript(self, node: ast.Subscript):
        obj, index = self.parse_subscript(node)
        return ContainerGetItem(obj, index)

    def parse_subscript(self, node: ast.Subscript):
        obj = self.visit(node.value)
        index_node = node.slice
        if isinstance(index_node, ast.Slice):
            raise RuntimeError("Slice is not supported")
        index = self.visit(index_node)
        return obj, index

    def _build_if_expr(self, test, body, orelse):
        return logic_or(
            logic_and(test, body),
            logic_and(logic_not(test), orelse),
        )

    def visit_IfExpr(self, node: ast.IfExp):
        return self._build_if_expr(
            self.visit(node.test),
            self.visit(node.body),
            self.visit(node.orelse),
        )
