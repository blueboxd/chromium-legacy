# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates header/source files with C++ language bindings
# for the X11 protocol and its extensions.  The protocol information
# is obtained from xcbproto which provides XML files describing the
# wire format.  However, we don't parse the XML here; xcbproto ships
# with xcbgen, a python library that parses the files into python data
# structures for us.
#
# The generated header and source files will look like this:

# #ifndef GEN_UI_GFX_X_XPROTO_H_
# #define GEN_UI_GFX_X_XPROTO_H_
#
# #include <array>
# #include <cstddef>
# #include <cstdint>
# #include <cstring>
# #include <vector>
#
# #include "base/component_export.h"
# #include "ui/gfx/x/xproto_types.h"
#
# typedef struct _XDisplay XDisplay;
#
# namespace x11 {
#
# class COMPONENT_EXPORT(X11) XProto {
#  public:
#   explicit XProto(XDisplay* display);
#
#   XDisplay* display() { return display_; }
#
#   struct RGB {
#     uint16_t red{};
#     uint16_t green{};
#     uint16_t blue{};
#   };
#
#   struct QueryColorsRequest {
#     uint32_t cmap{};
#     std::vector<uint32_t> pixels{};
#   };
#
#   struct QueryColorsReply {
#     uint16_t colors_len{};
#     std::vector<RGB> colors{};
#   };
#
#   using QueryColorsResponse = Response<QueryColorsReply>;
#
#   Future<QueryColorsReply> QueryColors(const QueryColorsRequest& request);
#
#  private:
#   XDisplay* display_;
# };
#
# }  // namespace x11
#
# #endif  // GEN_UI_GFX_X_XPROTO_H_

# #include "xproto.h"
#
# #include <xcb/xcb.h>
# #include <xcb/xcbext.h>
#
# #include "base/logging.h"
# #include "ui/gfx/x/xproto_internal.h"
#
# namespace x11 {
#
# XProto::XProto(XDisplay* display) : display_(display) {}
#
# Future<XProto::QueryColorsReply>
# XProto::QueryColors(
#     const XProto::QueryColorsRequest& request) {
#   WriteBuffer buf;
#
#   auto& cmap = request.cmap;
#   auto& pixels = request.pixels;
#   size_t pixels_len = pixels.size();
#
#   // major_opcode
#   uint8_t major_opcode = 91;
#   Write(&major_opcode, &buf);
#
#   // pad0
#   Pad(&buf, 1);
#
#   // length
#   // Caller fills in length for writes.
#   Pad(&buf, sizeof(uint16_t));
#
#   // cmap
#   Write(&cmap, &buf);
#
#   // pixels
#   DCHECK_EQ(static_cast<size_t>(pixels_len), pixels.size());
#   for (auto& pixels_elem : pixels) {
#     Write(&pixels_elem, &buf);
#   }
#
#   return x11::SendRequest<XProto::QueryColorsReply>(display_, &buf);
# }
#
# template<> COMPONENT_EXPORT(X11)
# std::unique_ptr<XProto::QueryColorsReply>
# detail::ReadReply<XProto::QueryColorsReply>(const uint8_t* buffer) {
#   ReadBuffer buf{buffer, 0UL};
#   auto reply = std::make_unique<XProto::QueryColorsReply>();
#
#   auto& colors_len = (*reply).colors_len;
#   auto& colors = (*reply).colors;
#
#   // response_type
#   uint8_t response_type;
#   Read(&response_type, &buf);
#
#   // pad0
#   Pad(&buf, 1);
#
#   // sequence
#   uint16_t sequence;
#   Read(&sequence, &buf);
#
#   // length
#   uint32_t length;
#   Read(&length, &buf);
#
#   // colors_len
#   Read(&colors_len, &buf);
#
#   // pad1
#   Pad(&buf, 22);
#
#   // colors
#   colors.resize(colors_len);
#   for (auto& colors_elem : colors) {
#     auto& red = colors_elem.red;
#     auto& green = colors_elem.green;
#     auto& blue = colors_elem.blue;
#
#     // red
#     Read(&red, &buf);
#
#     // green
#     Read(&green, &buf);
#
#     // blue
#     Read(&blue, &buf);
#
#     // pad0
#     Pad(&buf, 2);
#
#   }
#
#   Align(&buf, 4);
#   DCHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);
#
#   return reply;
# }
#
# }  // namespace x11

from __future__ import print_function

import argparse
import collections
import os
import sys
import types

# __main__.output must be defined before importing xcbgen,
# so this global is unavoidable.
output = collections.defaultdict(int)


# Left-pad with 2 spaces while this class is alive.
class Indent:
    def __init__(self, xproto, opening_line, closing_line):
        self.xproto = xproto
        self.opening_line = opening_line
        self.closing_line = closing_line

    def __enter__(self):
        self.xproto.write(self.opening_line)
        self.xproto.indent += 1

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.xproto.indent -= 1
        self.xproto.write(self.closing_line)


class NullContext:
    def __init__(self):
        pass

    def __enter__(self):
        pass

    def __exit__(self, exc_type, exc_value, exc_traceback):
        pass


# Make all members of |obj|, given by |fields|, visible in
# the local scope while this class is alive.
class ScopedFields:
    def __init__(self, xproto, obj, fields):
        self.xproto = xproto
        self.obj = obj
        self.fields = fields
        self.n_pushed = 0

    def __enter__(self):
        for field in self.fields:
            self.n_pushed += self.xproto.add_field_to_scope(field, self.obj)

        if self.n_pushed:
            self.xproto.write()

    def __exit__(self, exc_type, exc_value, exc_traceback):
        for _ in range(self.n_pushed):
            self.xproto.scope.pop()


# Ensures |name| is usable as a C++ field by avoiding keywords and
# symbols that start with numbers.
def safe_name(name):
    RESERVED = [
        'and',
        'xor',
        'or',
        'class',
        'explicit',
        'new',
        'delete',
        'default',
        'private',
    ]
    if name[0].isdigit() or name in RESERVED:
        return 'c_' + name
    return name


class GenXproto:
    def __init__(self, args, xcbgen):
        self.args = args
        self.xcbgen = xcbgen
        self.prev_id = -1
        self.indent = 0
        self.file = None
        self.is_read = False
        self.scope = []
        self.namespace = []

    # Write a line to the current file.
    def write(self, line=''):
        indent = self.indent if line and not line.startswith('#') else 0
        print(('  ' * indent) + line, file=self.file)

    # Geenerate an ID suitable for use in temporary variable names.
    def new_uid(self, ):
        self.prev_id += 1
        return self.prev_id

    def type_suffix(self, t):
        if isinstance(t, self.xcbgen.xtypes.Error):
            return 'Error'
        elif isinstance(t, self.xcbgen.xtypes.Request):
            return 'Request'
        elif t.is_reply:
            return 'Reply'
        elif t.is_event:
            return 'Event'
        return ''

    # Given an xcbgen.xtypes.Type, returns a C++-namespace-qualified
    # string that looks like Input::InputClass::Key.
    def qualtype(self, t):
        # Work around a bug in xcbgen: ('int') should have been ('int',)
        name = list(('int', ) if t.name == 'int' else t.name)
        name[-1] += self.type_suffix(t)

        if name[0] == 'xcb':
            # Use namespace x11 instead of xcb.
            name[0] = 'x11'

            # We want the non-extension X11 structures to live in a class too.
            if len(name) == 2:
                name[1:1] = ['XProto']

        # Try to avoid adding namespace qualifiers if they're not necessary.
        chop = 0
        for t1, t2 in zip(name, self.namespace):
            if t1 != t2:
                break
            chop += 1
        return '::'.join(name[chop:])

    def add_field_to_scope(self, field, obj):
        if not field.visible or not field.wire:
            return 0

        self.scope.append(field)

        field_name = safe_name(field.field_name)
        # There's one case where we would have generated:
        #   auto& enable = enable.enable;
        # To prevent a compiler error from trying to use the variable
        # in its own definition, save to a temporary variable first.
        if field_name == obj:
            tmp_id = self.new_uid()
            self.write('auto& tmp%d = %s.%s;' % (tmp_id, obj, field_name))
            self.write('auto& %s = tmp%d;' % (field_name, tmp_id))
        else:
            self.write('auto& %s = %s.%s;' % (field_name, obj, field_name))

        if field.type.is_list:
            len_name = field_name + '_len'
            if not self.field_from_scope(len_name):
                self.write('size_t %s = %s.size();' % (len_name, field_name))

        return 1

    # Lookup |name| in the current scope.  Returns the deepest
    # (most local) occurrence of |name|.
    def field_from_scope(self, name):
        for field in reversed(self.scope):
            if field.field_name == name:
                return field
        return None

    # Work around conflicts caused by Xlib's liberal use of macros.
    def undef(self, name):
        self.write('#ifdef %s' % name)
        self.write('#undef %s' % name)
        self.write('#endif')

    def expr(self, expr):
        if expr.op == 'popcount':
            return 'PopCount(%s)' % self.expr(expr.rhs)
        if expr.op == '~':
            return '~(%s)' % self.expr(expr.rhs)
        if expr.op in ('+', '-', '*', '/', '&', '|'):
            return ('(%s) %s (%s)' % (self.expr(expr.lhs), expr.op,
                                      self.expr(expr.rhs)))
        if expr.op == 'calculate_len':
            return expr.lenfield_name
        if expr.op == 'sumof':
            tmp_id = self.new_uid()
            lenfield = self.field_from_scope(expr.lenfield_name)
            elem_type = lenfield.type.member
            fields = elem_type.fields if elem_type.is_container else []
            header = 'auto sum%d_ = SumOf([](%sauto& listelem_ref) {' % (
                tmp_id, '' if self.is_read else 'const ')
            footer = '}, %s);' % expr.lenfield_name
            with Indent(self, header, footer), ScopedFields(
                    self, 'listelem_ref', fields):
                body = self.expr(expr.rhs) if expr.rhs else 'listelem_ref'
                self.write('return %s;' % body)
            return 'sum%d_' % tmp_id
        if expr.op == 'listelement-ref':
            return 'listelem_ref'
        if expr.op == 'enumref':
            return '%s::%s' % (self.qualtype(expr.lenfield_type),
                               safe_name(expr.lenfield_name))

        assert expr.op == None
        if expr.nmemb:
            return str(expr.nmemb)

        assert expr.lenfield_name
        return expr.lenfield_name

    def copy_primitive(self, name):
        self.write(
            '%s(&%s, &buf);' % ('Read' if self.is_read else 'Write', name))

    def copy_special_field(self, field):
        type_name = self.qualtype(field.type)
        name = safe_name(field.field_name)

        if name in ('major_opcode', 'minor_opcode'):
            assert not self.is_read
            is_ext = any(
                [f.field_name == 'minor_opcode' for f in field.parent.fields])
            if is_ext and name == 'major_opcode':
                self.write('// Caller fills in extension major opcode.')
                self.write('Pad(&buf, sizeof(%s));' % type_name)
            else:
                self.write(
                    '%s %s = %s;' % (type_name, name, field.parent.opcode))
                self.copy_primitive(name)
        elif name in ('response_type', 'sequence', 'extension'):
            assert self.is_read
            self.write('%s %s;' % (type_name, name))
            self.copy_primitive(name)
        elif name == 'length':
            if not self.is_read:
                self.write('// Caller fills in length for writes.')
                self.write('Pad(&buf, sizeof(%s));' % type_name)
            else:
                self.write('%s %s;' % (type_name, name))
                self.copy_primitive(name)
        else:
            assert field.type.is_expr
            self.write(
                '%s %s = %s;' % (type_name, name, self.expr(field.type.expr)))
            self.copy_primitive(name)

    def declare_case(self, case, switch_var):
        assert case.type.is_case != case.type.is_bitcase

        with (Indent(self, 'struct {', '} %s;' % safe_name(case.field_name))
              if case.field_name else NullContext()):
            for case_field in case.type.fields:
                self.declare_field(case_field)

    def copy_case(self, case, switch_var):
        op = 'CaseEq' if case.type.is_case else 'CaseAnd'
        condition = ' || '.join([
            '%s(%s, %s)' % (op, switch_var, self.expr(expr))
            for expr in case.type.expr
        ])

        with Indent(self, 'if (%s) {' % condition, '}'):
            with (ScopedFields(self, case.field_name, case.type.fields)
                  if case.field_name else NullContext()):
                for case_field in case.type.fields:
                    assert case_field.wire
                    self.copy_field(case_field)

    def declare_switch(self, field):
        t = field.type
        name = safe_name(field.field_name)

        with Indent(self, 'struct {', '} %s;' % name):
            switch_var = name + '_expr'
            for case in t.bitcases:
                self.declare_case(case, switch_var)

    def copy_switch(self, field):
        t = field.type
        name = safe_name(field.field_name)

        scope_fields = []
        for case in t.bitcases:
            if case.field_name:
                scope_fields.append(case)
            else:
                scope_fields.extend(case.type.fields)
        with Indent(self, '{', '}'), ScopedFields(self, name, scope_fields):
            switch_var = name + '_expr'
            self.write('auto %s = %s;' % (switch_var, self.expr(t.expr)))
            for case in t.bitcases:
                self.copy_case(case, switch_var)

    def declare_list(self, field):
        t = field.type
        type_name = self.qualtype(field.type)
        name = safe_name(field.field_name)

        assert (t.nmemb not in (0, 1))
        if t.nmemb:
            type_name = 'std::array<%s, %d>' % (type_name, t.nmemb)
        else:
            if type_name == 'void':
                # xcb uses void* in some places, but we prefer to use
                # std::vector<T> when possible.  Use T=uint8_t instead of
                # T=void for containers.
                type_name = 'std::vector<uint8_t>'
            elif type_name == 'char':
                type_name = 'std::string'
            else:
                type_name = 'std::vector<%s>' % type_name
        self.write('%s %s{};' % (type_name, name))

    def copy_list(self, field):
        t = field.type
        name = safe_name(field.field_name)

        if not t.nmemb:
            size = self.expr(t.expr)
            if self.is_read:
                self.write('%s.resize(%s);' % (name, size))
            else:
                left = 'static_cast<size_t>(%s)' % size
                self.write('DCHECK_EQ(%s, %s.size());' % (left, name))
        with Indent(self, 'for (auto& %s_elem : %s) {' % (name, name), '}'):
            elem_name = name + '_elem'
            elem_type = t.member
            if elem_type.is_simple or elem_type.is_union:
                self.copy_primitive(elem_name)
            else:
                assert elem_type.is_container
                self.copy_container(elem_type, elem_name)

    def declare_field(self, field):
        t = field.type
        name = safe_name(field.field_name)

        if not field.wire or not field.visible:
            return

        if t.is_switch:
            self.declare_switch(field)
        elif t.is_list:
            self.declare_list(field)
        else:
            self.write('%s %s{};' % (self.qualtype(field.type), name))

    def copy_field(self, field):
        t = field.type
        name = safe_name(field.field_name)

        self.write('// ' + name)
        if t.is_pad:
            if t.align > 1:
                assert t.nmemb == 1
                assert t.align in (2, 4)
                self.write('Align(&buf, %d);' % t.align)
            else:
                self.write('Pad(&buf, %d);' % t.nmemb)
        elif not field.visible:
            self.copy_special_field(field)
        elif t.is_switch:
            self.copy_switch(field)
        elif t.is_list:
            self.copy_list(field)
        elif t.is_union:
            self.copy_primitive(name)
        elif t.is_container:
            with Indent(self, '{', '}'):
                self.copy_container(t, name)
        else:
            assert t.is_simple
            self.copy_primitive(name)

    def declare_enum(self, enum):
        def declare_enum_entry(name, value):
            name = safe_name(name)
            self.undef(name)
            self.write('%s = %s,' % (name, value))

        self.undef(enum.name[-1])
        with Indent(self, 'enum class %s {' % enum.name[-1], '};'):
            bitnames = set([name for name, _ in enum.bits])
            for name, value in enum.values:
                if name not in bitnames:
                    declare_enum_entry(name, value)
            for name, value in enum.bits:
                declare_enum_entry(name, '1 << ' + value)
        self.write()

    def declare_container(self, struct):
        name = struct.name[-1] + self.type_suffix(struct)
        self.undef(name)
        with Indent(self, 'struct %s {' % name, '};'):
            for field in struct.fields:
                field.parent = struct
                self.declare_field(field)
        self.write()

    def copy_container(self, struct, name):
        assert not struct.is_union
        with ScopedFields(self, name, struct.fields):
            for field in struct.fields:
                if field.wire:
                    self.copy_field(field)
                    self.write()

    def declare_union(self, union):
        name = union.name[-1]
        with Indent(self, 'union %s {' % name, '};'):
            self.write('%s() { memset(this, 0, sizeof(*this)); }' % name)
            self.write()
            for field in union.fields:
                type_name = self.qualtype(field.type)
                self.write('%s %s;' % (type_name, safe_name(field.field_name)))
        self.write(
            'static_assert(std::is_trivially_copyable<%s>::value, "");' % name)
        self.write()

    def declare_request(self, request):
        method_name = request.name[-1]
        request_name = method_name + 'Request'
        reply_name = method_name + 'Reply'

        self.declare_container(request)
        if request.reply:
            self.declare_container(request.reply)
        else:
            reply_name = 'void'

        self.write(
            'using %sResponse = Response<%s>;' % (method_name, reply_name))
        self.write()

        self.write('Future<%s> %s(' % (reply_name, method_name))
        self.write('    const %s& request);' % request_name)
        self.write()

    def define_request(self, request):
        method_name = '%s::%s' % (self.class_name, request.name[-1])
        request_name = method_name + 'Request'
        reply_name = method_name + 'Reply'

        reply = request.reply
        if not reply:
            reply_name = 'void'

        self.write('Future<%s>' % reply_name)
        self.write('%s(' % method_name)
        with Indent(self, '    const %s& request) {' % request_name, '}'):
            self.namespace = ['x11', self.class_name]
            self.write('WriteBuffer buf;')
            self.write()
            self.is_read = False
            self.copy_container(request, 'request')
            self.write(
                'return x11::SendRequest<%s>(display_, &buf);' % reply_name)
        self.write()

        if reply:
            self.write('template<> COMPONENT_EXPORT(X11)')
            self.write('std::unique_ptr<%s>' % reply_name)
            sig = 'detail::ReadReply<%s>(const uint8_t* buffer) {' % reply_name
            with Indent(self, sig, '}'):
                self.namespace = ['x11']
                self.write('ReadBuffer buf{buffer, 0UL};')
                self.write('auto reply = std::make_unique<%s>();' % reply_name)
                self.write()
                self.is_read = True
                self.copy_container(reply, '(*reply)')
                self.write('Align(&buf, 4);')
                offset = 'buf.offset < 32 ? 0 : buf.offset - 32'
                self.write('DCHECK_EQ(%s, 4 * length);' % offset)
                self.write()
                self.write('return reply;')
            self.write()

    def declare_type(self, item, name):
        if item.is_union:
            self.declare_union(item)
        elif isinstance(item, self.xcbgen.xtypes.Request):
            self.declare_request(item)
        elif item.is_container:
            item.name = name
            self.declare_container(item)
        elif isinstance(item, self.xcbgen.xtypes.Enum):
            self.declare_enum(item)
        else:
            assert item.is_simple
            self.write('using %s = %s;' % (name[-1], self.qualtype(item)))
            self.write()

    def gen_header(self):
        self.file = self.args.headerfile
        include_guard = self.args.headerfile.name.replace('/', '_').replace(
            '.', '_').upper() + '_'
        self.write('#ifndef ' + include_guard)
        self.write('#define ' + include_guard)
        self.write()
        self.write('#include <array>')
        self.write('#include <cstddef>')
        self.write('#include <cstdint>')
        self.write('#include <cstring>')
        self.write('#include <vector>')
        self.write()
        self.write('#include "base/component_export.h"')
        self.write('#include "ui/gfx/x/xproto_types.h"')
        for direct_import in self.module.direct_imports:
            self.write('#include "%s.h"' % direct_import[-1])
        self.write()
        self.write('typedef struct _XDisplay XDisplay;')
        self.write()
        self.write('namespace x11 {')
        self.write()

        name = self.class_name
        self.undef(name)
        with Indent(self, 'class COMPONENT_EXPORT(X11) %s {' % name, '};'):
            self.namespace = ['x11', self.class_name]
            self.write('public:')
            self.write('explicit %s(XDisplay* display);' % name)
            self.write()
            self.write('XDisplay* display() { return display_; }')
            self.write()
            for (name, item) in self.module.all:
                self.declare_type(item, name)
            self.write('private:')
            self.write('XDisplay* const display_;')

        self.write()
        self.write('}  // namespace x11')
        self.write()
        self.write('#endif  // ' + include_guard)

    def gen_source(self):
        self.file = self.args.sourcefile
        self.write('#include "%s.h"' % self.module.namespace.header)
        self.write()
        self.write('#include <xcb/xcb.h>')
        self.write('#include <xcb/xcbext.h>')
        self.write()
        self.write('#include "base/logging.h"')
        self.write('#include "ui/gfx/x/xproto_internal.h"')
        self.write()
        self.write('namespace x11 {')
        self.write()
        name = self.class_name
        self.write(
            '%s::%s(XDisplay* display) : display_(display) {}' % (name, name))
        self.write()
        for (name, item) in self.module.all:
            if isinstance(item, self.xcbgen.xtypes.Request):
                self.define_request(item)
        self.write('}  // namespace x11')

    def generate(self):
        self.module = self.xcbgen.state.Module(self.args.xmlfile.name, None)
        self.module.register()
        self.module.resolve()

        self.class_name = (self.module.namespace.ext_name
                           if self.module.namespace.is_ext else 'XProto')

        self.gen_header()
        self.gen_source()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('xmlfile', type=argparse.FileType('r'))
    parser.add_argument('headerfile', type=argparse.FileType('w'))
    parser.add_argument('sourcefile', type=argparse.FileType('w'))
    parser.add_argument('--sysroot')
    args = parser.parse_args()

    if args.sysroot:
        path = os.path.join(args.sysroot, 'usr', 'lib', 'python2.7',
                            'dist-packages')
        sys.path.insert(1, path)

    import xcbgen.xtypes
    import xcbgen.state

    generator = GenXproto(args, xcbgen)
    generator.generate()

    return 0


if __name__ == '__main__':
    sys.exit(main())
