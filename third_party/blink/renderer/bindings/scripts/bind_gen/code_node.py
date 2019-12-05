# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The code generator generates code based on a graph of code fragments.  Each node
of the graph is represented with CodeNode and its subclasses.  This module
provides a collection of the classes that represent code nodes independent from
specific bindings, such as ECMAScript bindings.
"""

import copy

from .codegen_accumulator import CodeGenAccumulator
from .codegen_format import format_template
from .mako_renderer import MakoRenderer


class Likeliness(object):
    """
    Represents how much likely a code node will be executed.

    Used in SymbolScopeNode in order to determine where SymbolDefinitionNodes
    should be inserted.  Likeliness level can change only at SymbolScopeNode.

    Relational operators are supported, and it's guaranteed to be:
      NEVER < UNLIKELY < LIKELY < ALWAYS
    """

    class Level(int):
        pass

    NEVER = Level(0)
    UNLIKELY = Level(1)
    LIKELY = Level(2)
    ALWAYS = Level(3)


class CodeNode(object):
    """
    This is the base class of all code fragment nodes.

    - Graph structure
    CodeNode can be nested and |outer| points to the nesting CodeNode.  Also
    CodeNode can make a sequence and |prev| points to the previous CodeNode.
    See also |ListNode|.

    - Template rendering
    CodeNode has template text and template variable bindings.  Either of
    |__str__| or |render| returns a text of generated code.  However, these
    methods have side effects on rendering states, and repeated calls may return
    different results.
    """

    class _RenderState(object):
        """
        Represents a set of per-render states.  Every call to CodeNode.render
        resets all the per-render states.
        """

        def __init__(self):
            # Symbols used in generated code, but not yet defined.  See also
            # SymbolNode.  Code symbols are accumulated in order from the first
            # appearance to the last.  This order affects the insertion order of
            # SymbolDefinitionNodes at SymbolScopeNode.
            self.undefined_code_symbols = []

            # Dict from a SymbolNode to a set of tuples of SymbolScopeNodes
            # where the symbol was used.
            #
            # For example, given a code symbol |x|, the following code
            # structure:
            #   {  // Scope1
            #     {  // Scope2A
            #       {  // Scope3
            #         x;          // [1]
            #       }
            #       x;            // [2]
            #     }
            #     x;              // [3]
            #     {  // Scope2B
            #       x;            // [4]
            #     }
            #     x;              // [5]
            #   }
            # is translated into an entry of the dict below:
            #   set([
            #     (Scope1),                   # [3], [5]
            #     (Scope1, Scope2A),          # [2]
            #     (Scope1, Scope2A, Scope3),  # [1]
            #     (Scope1, Scope2B),          # [4]
            #   ])
            self.undefined_code_symbols_scope_chains = {}

    _gensym_seq_id = 0

    @classmethod
    def gensym(cls):
        """
        Creates a new template variable that never conflicts with anything.

        The name 'gensym' came from 'gensym' (generated symbol) in Lisp that
        exists for exactly the same purpose.

        Note that |gensym| is used to produce a new Mako template variable while
        SymbolNode is used to represent a code symbol (such as a local variable)
        in generated code.

        Bad example:
            template_text = "abc ${tmp} xyz"
            a = CodeNodeA(template_text='123')
            b = CodeNodeB(template_text=template_text, {'tmp': a})
        |b| expects "abc 123 xyz" but what if 'tmp' were already bound to
        something else?

        Good example:
            sym = CodeNode.gensym()
            template_text = format_template(
                "abc ${{{node_a}}} xyz", node_a=sym)
            a = CodeNodeA(template_text='123')
            b = CodeNodeB(template_text=template_text, {sym: a})
        "{{" and "}}" are literal of "{" and "}" themselves, and the innermost
        "{node_a}" will be replaced with |sym|.  The resulting template text
        will be "abc ${gensym1} xyz" when |sym| is 'gensym1'.
        """
        cls._gensym_seq_id += 1
        return "gensym{}".format(cls._gensym_seq_id)

    def __init__(self, template_text=None, template_vars=None):
        assert template_text is None or isinstance(template_text, str)
        assert template_vars is None or isinstance(template_vars, dict)

        # The outer CodeNode or None iff this is a top-level node
        self._outer = None
        # The previous CodeNode if this is a Sequence or None
        self._prev = None

        # Mako's template text, bindings dict
        self._template_text = template_text
        self._template_vars = {}

        self._accumulator = None  # CodeGenAccumulator
        self._accumulate_requests = []

        self._renderer = None  # MakoRenderer

        self._render_state = CodeNode._RenderState()
        self._is_rendering = False

        if template_vars:
            self.add_template_vars(template_vars)

    def __str__(self):
        """
        Renders this CodeNode object into a Mako template.  This is supposed to
        be used in a Mako template as ${code_node}.
        """
        return self.render()

    def render(self):
        """
        Renders this CodeNode object as a text string and also propagates
        updates to related CodeNode objects.  As this method has side-effects
        not only to this object but also other related objects, the resulting
        text may change on each invocation.
        """
        assert self.renderer

        last_render_state = self._render_state
        self._render_state = CodeNode._RenderState()
        self._is_rendering = True

        try:
            text = self._render(
                renderer=self.renderer, last_render_state=last_render_state)
        finally:
            self._is_rendering = False

        if self._accumulate_requests:
            assert self.accumulator
            for request in self._accumulate_requests:
                request(self.accumulator)

        return text

    def _render(self, renderer, last_render_state):
        """
        Renders this CodeNode object as a text string and also propagates
        updates to related CodeNode objects.

        Only limited subclasses may override this method.
        """
        assert self._template_text is not None
        return renderer.render(
            caller=self,
            template_text=self._template_text,
            template_vars=self.template_vars)

    @property
    def outer(self):
        """Returns the outer CodeNode or None iff this is a top-level node."""
        return self._outer

    def set_outer(self, outer):
        assert isinstance(outer, CodeNode)
        assert self._outer is None
        self._outer = outer

    def reset_outer(self, outer):
        assert isinstance(outer, CodeNode) or outer is None
        self._outer = outer

    @property
    def prev(self):
        """Returns the previous CodeNode if this is a Sequence or None."""
        return self._prev

    def set_prev(self, prev):
        assert isinstance(prev, CodeNode)
        assert self._prev is None
        self._prev = prev

    def reset_prev(self, prev):
        assert isinstance(prev, CodeNode) or prev is None
        self._prev = prev

    @property
    def upstream_of_scope(self):
        """
        Returns the upstream CodeNode in the same or outer scope.  Only the set
        of recursively-collected |upstream_of_scope|s can bring symbol
        definitions effective to this node.
        """
        if self.prev is None:
            return self.outer

        node = self.prev
        while isinstance(node, SequenceNode):
            if isinstance(node, SymbolScopeNode):
                return node.upstream_of_scope
            if not node:
                break
            node = node[-1]
        return node

    @property
    def template_vars(self):
        """
        Returns the template variable bindings available at this point, i.e.
        bound at this node or outer nodes.

        CAUTION: Do not modify the returned dict.  This method may return the
        original dict in a CodeNode.
        """
        if not self.outer:
            return self._template_vars

        if not self._template_vars:
            return self.outer.template_vars

        binds = copy.copy(self.outer.template_vars)
        for name, value in self._template_vars.iteritems():
            assert name not in binds, (
                "Duplicated template variable binding: {}".format(name))
            binds[name] = value
        return binds

    def add_template_var(self, name, value):
        assert name not in self._template_vars, (
            "Duplicated template variable binding: {}".format(name))
        if isinstance(value, CodeNode):
            value.set_outer(self)
        self._template_vars[name] = value

    def add_template_vars(self, template_vars):
        assert isinstance(template_vars, dict)
        for name, value in template_vars.iteritems():
            self.add_template_var(name, value)

    @property
    def accumulator(self):
        # Always consistently use the accumulator of the root node.
        if self.outer is not None:
            return self.outer.accumulator
        return self._accumulator

    def set_accumulator(self, accumulator):
        assert isinstance(accumulator, CodeGenAccumulator)
        assert self._accumulator is None
        self._accumulator = accumulator

    def accumulate(self, request):
        """
        While rendering the code node, |request| will be called with the
        argument of self.accumulator.
        """
        assert callable(request)
        self._accumulate_requests.append(request)

    @property
    def renderer(self):
        # Always use the renderer of the root node in order not to mix renderers
        # during rendering of a single code node tree.
        if self.outer is not None:
            return self.outer.renderer
        return self._renderer

    def set_renderer(self, renderer):
        assert isinstance(renderer, MakoRenderer)
        assert self._renderer is None
        self._renderer = renderer

    @property
    def current_render_state(self):
        assert self._is_rendering
        return self._render_state

    @property
    def last_render_state(self):
        assert not self._is_rendering
        return self._render_state

    def is_code_symbol_defined(self, symbol_node):
        """
        Returns True if |symbol_node| is defined at this point or upstream.
        """
        if self.outer:
            return self.upstream_of_scope.is_code_symbol_defined(symbol_node)
        return False

    def is_code_symbol_registered(self, symbol_node):
        """
        Returns True if |symbol_node| is registered and available for use at
        this point.  See also |SymbolScopeNode.register_code_symbol|.
        """
        if self.outer is not None:
            return self.outer.is_code_symbol_registered(symbol_node)
        return False

    def on_undefined_code_symbol_found(self, symbol_node, symbol_scope_chain):
        """Receives a report of use of an undefined symbol node."""
        assert isinstance(symbol_node, SymbolNode)
        assert isinstance(symbol_scope_chain, tuple)
        assert all(
            isinstance(scope, SymbolScopeNode) for scope in symbol_scope_chain)
        state = self.current_render_state
        if symbol_node not in state.undefined_code_symbols:
            state.undefined_code_symbols.append(symbol_node)
        state.undefined_code_symbols_scope_chains.setdefault(
            symbol_node, set()).add(symbol_scope_chain)


class LiteralNode(CodeNode):
    """
    Represents a literal text, which will be rendered as is without any template
    magic applied.
    """

    def __init__(self, literal_text):
        literal_text_gensym = CodeNode.gensym()
        template_text = format_template(
            "${{{literal_text}}}", literal_text=literal_text_gensym)
        template_vars = {literal_text_gensym: literal_text}

        CodeNode.__init__(
            self, template_text=template_text, template_vars=template_vars)


class TextNode(CodeNode):
    """
    Represents a template text node.

    TextNode is designed to be a leaf node of a code node tree.  TextNode
    represents a template text while LiteralNode represents a literal text.
    All template magics will be applied to |template_text|.
    """

    def __init__(self, template_text):
        CodeNode.__init__(self, template_text=template_text)


class CompositeNode(CodeNode):
    """
    Represents a composition of multiple code nodes.  Composition will be done
    by using |CodeNode.gensym| so that it won't contaminate a namespace of the
    template variables.
    """

    def __init__(self, template_format_str, *args, **kwargs):
        """
        Args:
            template_format_str: A format string that is used to produce the
                template text.
            args:
            kwargs: Arguments to be passed to |format_template|.  Not
                necessarily be CodeNode, but also anything renderable can be
                passed in.
        """
        assert isinstance(template_format_str, str)
        gensym_args = []
        gensym_kwargs = {}
        template_vars = {}
        for arg in args:
            assert isinstance(arg, (CodeNode, int, long, str))
            gensym = CodeNode.gensym()
            gensym_args.append("${{{}}}".format(gensym))
            template_vars[gensym] = arg
        for key, value in kwargs.iteritems():
            assert isinstance(key, (int, long, str))
            assert isinstance(value, (CodeNode, int, long, str))
            gensym = CodeNode.gensym()
            gensym_kwargs[key] = "${{{}}}".format(gensym)
            template_vars[gensym] = value
        template_text = format_template(template_format_str, *gensym_args,
                                        **gensym_kwargs)

        CodeNode.__init__(
            self, template_text=template_text, template_vars=template_vars)


class ListNode(CodeNode):
    """
    Represents a list of nodes.

    append, extend, insert, and remove work just like built-in list's methods
    except that addition and removal of None have no effect.
    """

    def __init__(self, code_nodes=None, separator=" ", separator_last=""):
        assert isinstance(separator, str)
        assert isinstance(separator_last, str)

        element_nodes_gensym = CodeNode.gensym()
        element_nodes = []
        template_text = format_template(
            """\
% for node in {element_nodes}:
${node}\\
% if not loop.last:
{separator}\\
% endif
% endfor
{separator_last}\\
""",
            element_nodes=element_nodes_gensym,
            separator=separator,
            separator_last=separator_last)
        template_vars = {element_nodes_gensym: element_nodes}

        CodeNode.__init__(
            self, template_text=template_text, template_vars=template_vars)

        self._element_nodes = element_nodes

        if code_nodes is not None:
            self.extend(code_nodes)

    def __getitem__(self, index):
        return self._element_nodes[index]

    def __iter__(self):
        return iter(self._element_nodes)

    def __len__(self):
        return len(self._element_nodes)

    def append(self, node):
        if node is None:
            return
        assert isinstance(node, CodeNode)
        assert node.outer is None and node.prev is None

        if len(self._element_nodes) == 0:
            self._element_nodes.append(node)
        else:
            node.set_prev(self._element_nodes[-1])
            self._element_nodes.append(node)
        node.set_outer(self)

    def extend(self, nodes):
        for node in nodes:
            self.append(node)

    def insert(self, index, node):
        if node is None:
            return
        assert isinstance(index, (int, long))
        assert isinstance(node, CodeNode)
        assert node.outer is None and node.prev is None

        if index < 0:
            index += len(self._element_nodes)
        index = max(0, min(index, len(self._element_nodes)))

        if (len(self._element_nodes) == 0
                or index == len(self._element_nodes)):
            return self.append(node)

        next_node = self._element_nodes[index]
        if next_node.prev:
            node.set_prev(next_node.prev)
        next_node.reset_prev(node)
        node.set_outer(self)
        self._element_nodes.insert(index, node)

    def remove(self, node):
        if node is None:
            return
        assert node in self

        index = self._element_nodes.index(node)
        if index + 1 < len(self._element_nodes):
            next_node = self._element_nodes[index + 1]
            prev_node = self._element_nodes[index - 1] if index != 0 else None
            next_node.reset_prev(prev_node)
        del self._element_nodes[index]
        node.reset_outer(None)
        node.reset_prev(None)


class SequenceNode(ListNode):
    """
    Represents a sequence of generated code without introducing any new scope,
    and provides the points where SymbolDefinitionNodes can be inserted.
    """

    def __init__(self, code_nodes=None, separator="\n", separator_last=""):
        ListNode.__init__(
            self,
            code_nodes=code_nodes,
            separator=separator,
            separator_last=separator_last)

    def _render(self, renderer, last_render_state):
        duplicates = []
        for element_node in self:
            if (isinstance(element_node, SymbolDefinitionNode)
                    and element_node.is_duplicated()):
                duplicates.append(element_node)
        for element_node in duplicates:
            self.remove(element_node)

        return super(SequenceNode, self)._render(
            renderer=renderer, last_render_state=last_render_state)


class SymbolScopeNode(SequenceNode):
    """
    Represents a scope of generated code.

    If SymbolNodes are rendered inside this node, this node will attempt to
    insert corresponding SymbolDefinitionNodes appropriately.
    """

    def __init__(self, code_nodes=None, separator="\n", separator_last=""):
        SequenceNode.__init__(
            self,
            code_nodes=code_nodes,
            separator=separator,
            separator_last=separator_last)

        self._likeliness = Likeliness.ALWAYS
        self._registered_code_symbols = set()

    def _render(self, renderer, last_render_state):
        for symbol_node in last_render_state.undefined_code_symbols:
            if (self.is_code_symbol_registered(symbol_node)
                    and not self.is_code_symbol_defined(symbol_node)):
                self._insert_symbol_definition(symbol_node, last_render_state)

        return super(SymbolScopeNode, self)._render(
            renderer=renderer, last_render_state=last_render_state)

    def _insert_symbol_definition(self, symbol_node, last_render_state):
        def count_by_likeliness(render_state):
            counts = {
                Likeliness.UNLIKELY: 0,
                Likeliness.LIKELY: 0,
                Likeliness.ALWAYS: 0,
            }

            scope_chains = (render_state.undefined_code_symbols_scope_chains.
                            get(symbol_node))
            if not scope_chains:
                return counts

            self_index = iter(scope_chains).next().index(self)
            scope_chains = map(
                lambda scope_chain: scope_chain[self_index + 1:], scope_chains)
            for scope_chain in scope_chains:
                if not scope_chain:
                    counts[Likeliness.ALWAYS] += 1
                else:
                    likeliness = min(
                        map(lambda scope: scope.likeliness, scope_chain))
                    counts[likeliness] += 1
            return counts

        def likeliness_at(render_state):
            counts = count_by_likeliness(render_state)
            for likeliness in (Likeliness.ALWAYS, Likeliness.LIKELY,
                               Likeliness.UNLIKELY):
                if counts[likeliness] > 0:
                    return likeliness
            return Likeliness.NEVER

        def insert_before_threshold(sequence_node, threshold):
            for index, node in enumerate(sequence_node):
                if (isinstance(node, SequenceNode)
                        and not isinstance(node, SymbolScopeNode)):
                    did_insert = insert_before_threshold(node, threshold)
                    if did_insert:
                        return True
                elif likeliness_at(node.last_render_state) >= threshold:
                    sequence_node.insert(index,
                                         symbol_node.create_definition_node())
                    return True
            return False

        counts = count_by_likeliness(last_render_state)
        if counts[Likeliness.ALWAYS] >= 1:
            did_insert = insert_before_threshold(self, Likeliness.UNLIKELY)
            assert did_insert
        elif counts[Likeliness.LIKELY] >= 2:
            did_insert = insert_before_threshold(self, Likeliness.LIKELY)
            assert did_insert
        else:
            pass  # Do nothing and let descendant SymbolScopeNodes do the work.

    def is_code_symbol_registered(self, symbol_node):
        if symbol_node in self._registered_code_symbols:
            return True
        return super(SymbolScopeNode,
                     self).is_code_symbol_registered(symbol_node)

    def register_code_symbol(self, symbol_node):
        """Registers a SymbolNode and makes it available in this scope."""
        assert isinstance(symbol_node, SymbolNode)
        self.add_template_var(symbol_node.name, symbol_node)
        self._registered_code_symbols.add(symbol_node)

    def register_code_symbols(self, symbol_nodes):
        for symbol_node in symbol_nodes:
            self.register_code_symbol(symbol_node)

    @property
    def likeliness(self):
        """
        Returns how much likely that this SymbolScopeNode will be executed in
        runtime.  The likeliness is relative to the closest outer
        SymbolScopeNode.
        """
        return self._likeliness

    def set_likeliness(self, likeliness):
        assert isinstance(likeliness, Likeliness.Level)
        self._likeliness = likeliness


class SymbolNode(CodeNode):
    """
    Represents a code symbol such as a local variable of generated code.

    Using a SymbolNode combined with SymbolScopeNode, SymbolDefinitionNode(s)
    will be automatically inserted iff this symbol is referenced.
    """

    def __init__(self, name, template_text=None, definition_constructor=None):
        """
        Args:
            name: The name of this code symbol.
            template_text: Template text to be used to define the code symbol.
            definition_constructor: A callable that creates and returns a new
                definition node.  This SymbolNode will be passed as the
                argument.
                Either of |template_text| or |definition_constructor| must be
                given.
        """
        assert isinstance(name, str) and name
        assert (template_text is not None
                or definition_constructor is not None)
        assert template_text is None or definition_constructor is None
        if template_text is not None:
            assert isinstance(template_text, str)
        if definition_constructor is not None:
            assert callable(definition_constructor)

        CodeNode.__init__(self)

        self._name = name

        if template_text is not None:

            def constructor(symbol_node):
                return SymbolDefinitionNode(
                    symbol_node=symbol_node,
                    code_nodes=[TextNode(template_text)])

            self._definition_constructor = constructor
        else:
            self._definition_constructor = definition_constructor

    def _render(self, renderer, last_render_state):
        symbol_scope_chain = tuple(
            filter(lambda node: isinstance(node, SymbolScopeNode),
                   renderer.callers_from_first_to_last))

        for caller in renderer.callers_from_last_to_first:
            if caller.is_code_symbol_defined(self):
                break
            caller.on_undefined_code_symbol_found(self, symbol_scope_chain)

        return self.name

    @property
    def name(self):
        return self._name

    def create_definition_node(self):
        """Creates a new definition node."""
        node = self._definition_constructor(self)
        assert isinstance(node, SymbolDefinitionNode)
        assert node.is_code_symbol_defined(self)
        return node


class SymbolDefinitionNode(SequenceNode):
    """
    Represents a definition of a code symbol.

    It's allowed to define the same code symbol multiple times, and most
    upstream definition(s) are effective.
    """

    def __init__(self, symbol_node, code_nodes=None):
        assert isinstance(symbol_node, SymbolNode)

        SequenceNode.__init__(self, code_nodes)

        self._symbol_node = symbol_node

    def _render(self, renderer, last_render_state):
        if self.is_duplicated():
            return ""

        return super(SymbolDefinitionNode, self)._render(
            renderer=renderer, last_render_state=last_render_state)

    def is_code_symbol_defined(self, symbol_node):
        if symbol_node is self._symbol_node:
            return True
        return super(SymbolDefinitionNode,
                     self).is_code_symbol_defined(symbol_node)

    def is_duplicated(self):
        return self.upstream_of_scope.is_code_symbol_defined(self._symbol_node)


class ConditionalNode(CodeNode):
    """
    This is the base class of code nodes that directly contain conditional
    execution.  Not all the code nodes inside this node will be executed.
    """


class CaseBranchNode(ConditionalNode):
    """
    Represents a set of nodes that will be conditionally executed.

    This node mostly follows the concept of 'cond' in Lisp, which is a chain of
    if - else if - else if - ... - else.
    """

    def __init__(self, clause_nodes):
        # clause_nodes = [
        #   (conditional_node1, body_node1, likeliness1),
        #   (conditional_node2, body_node2, likeliness2),
        #   ...,
        #   (None, body_nodeN, likelinessN),  # Corresponds to 'else { ... }'
        # ]
        assert False


class ConditionalExitNode(ConditionalNode):
    """
    Represents a conditional node that always exits whenever the condition
    meets.  It's not supposed that the body node does not exit.  Equivalent to

      if (CONDITIONAL) { BODY }

    where BODY ends with a return statement.
    """

    def __init__(self, cond, body, body_likeliness):
        assert isinstance(cond, CodeNode)
        assert isinstance(body, SymbolScopeNode)
        assert isinstance(body_likeliness, Likeliness.Level)

        gensyms = {
            "conditional": CodeNode.gensym(),
            "body": CodeNode.gensym(),
        }
        template_text = format_template(
            """\
if (${{{conditional}}}) {{
  ${{{body}}}
}}\\
""", **gensyms)
        template_vars = {
            gensyms["conditional"]: cond,
            gensyms["body"]: body,
        }

        ConditionalNode.__init__(
            self, template_text=template_text, template_vars=template_vars)

        self._cond_node = cond
        self._body_node = body
        self._body_node.set_likeliness(body_likeliness)


class LikelyExitNode(ConditionalExitNode):
    """
    Represents a conditional exit node where it's likely that the condition
    meets.
    """

    def __init__(self, cond, body):
        ConditionalExitNode.__init__(
            self, cond=cond, body=body, body_likeliness=Likeliness.LIKELY)


class UnlikelyExitNode(ConditionalExitNode):
    """
    Represents a conditional exit node where it's unlikely that the condition
    meets.
    """

    def __init__(self, cond, body):
        ConditionalExitNode.__init__(
            self, cond=cond, body=body, body_likeliness=Likeliness.UNLIKELY)


class FunctionDefinitionNode(CodeNode):
    """Represents a function definition."""

    def __init__(self,
                 name,
                 arg_decls,
                 return_type,
                 member_initializer_list=None,
                 local_vars=None,
                 body=None,
                 comment=None):
        """
        Args:
            name: Function name node, which may include nested-name-specifier
                (i.e. 'namespace_name::' and/or 'type_name::').
            arg_decls: List of argument declaration nodes.
            return_type: Return type node.
            member_initializer_list: List of initializer nodes.
            local_vars: List of SymbolNodes that can be used in the function
                body.
            body: Function body node (of type SymbolScopeNode).
            comment: Function header comment node.
        """
        assert isinstance(name, CodeNode)
        assert isinstance(arg_decls, (list, tuple))
        assert all(isinstance(arg_decl, CodeNode) for arg_decl in arg_decls)
        assert isinstance(return_type, CodeNode)
        if member_initializer_list is None:
            member_initializer_list = []
        assert isinstance(member_initializer_list, (list, tuple))
        assert all(
            isinstance(initializer, CodeNode)
            for initializer in member_initializer_list)
        if local_vars is None:
            local_vars = []
        assert isinstance(local_vars, (list, tuple))
        assert all(
            isinstance(local_var, SymbolNode) for local_var in local_vars)
        if body is None:
            body = SymbolScopeNode()
        assert isinstance(body, CodeNode)
        if comment is None:
            comment = LiteralNode("")
        assert isinstance(comment, CodeNode)

        gensyms = {
            "name": CodeNode.gensym(),
            "arg_decls": CodeNode.gensym(),
            "return_type": CodeNode.gensym(),
            "member_initializer_list": CodeNode.gensym(),
            "body": CodeNode.gensym(),
            "comment": CodeNode.gensym(),
        }

        maybe_colon = " : " if member_initializer_list else ""

        template_text = format_template(
            """\
${{{comment}}}
${{{return_type}}} ${{{name}}}(${{{arg_decls}}})\
{maybe_colon}${{{member_initializer_list}}} {{
  ${{{body}}}
}}\\
""",
            maybe_colon=maybe_colon,
            **gensyms)
        template_vars = {
            gensyms["name"]:
            name,
            gensyms["arg_decls"]:
            ListNode(arg_decls, separator=", "),
            gensyms["return_type"]:
            return_type,
            gensyms["member_initializer_list"]:
            ListNode(member_initializer_list, separator=", "),
            gensyms["body"]:
            body,
            gensyms["comment"]:
            comment,
        }

        CodeNode.__init__(
            self, template_text=template_text, template_vars=template_vars)

        self._body_node = body

        self._body_node.register_code_symbols(local_vars)

    @property
    def body(self):
        return self._body_node
