class ScopeContext:
    def __init__(self):
        self.node_stack = []
        self.symbols = []
        self.referenced_symbols = []
        self.var_types = []
        self.func_params = []
        self.func_locals_ = {}
        self.func_dict_attr = {}
        self.func_var_env_dict = {}
        self.func_ret_type = None
        self.ssa_ctx = {}

    def pop_scope(self):
        self.symbols.pop()
        self.referenced_symbols.pop()
        self.var_types.pop()
        self.node_stack.pop()

    def new_scope(self, nodes=None):
        if nodes is None:
            nodes = []
        self.node_stack.append(list(reversed(nodes)))
        self.symbols.append(dict())
        self.referenced_symbols.append(dict())
        self.var_types.append(dict())

    def update_symbol(self, name, symbol, dtype=None):
        self.symbols[-1][name] = symbol
        self.referenced_symbols[-1][symbol] = [None, 0]
        if dtype is not None:
            self.var_types[-1][name] = dtype

    def bind_reference(self, symbol, origin_symbol):
        self.referenced_symbols[-1][symbol] = [origin_symbol, 0]
        return self.referenced_symbols[-1][symbol]

    def remove_symbol(self, name):
        for symbols, view_symbols in zip(reversed(self.symbols), reversed(self.referenced_symbols)):
            if name in symbols:
                view_symbols.pop(symbols[name])
                symbols.pop(name)
                return
        raise RuntimeError(
            "Internal error of matx script parser: no symbol named" + name
        )

    def lookup_symbol(self, name):
        for symbols in reversed(self.symbols):
            if name in symbols:
                return symbols[name]
        return None

    def lookup_type(self, name):
        for types in reversed(self.var_types):
            if name in types:
                return types[name]
        return None

    def lookup_symbol_with_level(self, name):
        level = -1
        for symbols in reversed(self.symbols):
            if name in symbols:
                return symbols[name], level
            level -= 1
        return None, None

    def lookup_referenced_symbol(self, symbol, update_count=True):
        for symbols in reversed(self.referenced_symbols):
            if symbol in symbols:
                if update_count:
                    symbols[symbol][1] += 1
                return symbols[symbol][0]
        return None
