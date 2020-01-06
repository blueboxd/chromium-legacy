# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class CodeGenAccumulator(object):
    """
    Accumulates a variety of information and helps generate code based on the
    information.
    """

    def __init__(self):
        # Headers to be included
        self._include_headers = set()
        # Forward declarations of C++ class
        self._class_decls = set()
        # Forward declarations of C++ struct
        self._struct_decls = set()

    def total_size(self):
        return (len(self.include_headers) + len(self.class_decls) + len(
            self.struct_decls))

    @property
    def include_headers(self):
        return self._include_headers

    def add_include_header(self, header):
        self._include_headers.add(header)

    def add_include_headers(self, headers):
        self._include_headers.update(headers)

    @staticmethod
    def require_include_headers(headers):
        return lambda accumulator: accumulator.add_include_headers(headers)

    @property
    def class_decls(self):
        return self._class_decls

    def add_class_decl(self, class_name):
        self._class_decls.add(class_name)

    def add_class_decls(self, class_names):
        self._class_decls.update(class_names)

    @staticmethod
    def require_class_decls(class_names):
        return lambda accumulator: accumulator.add_class_decls(class_names)

    @property
    def struct_decls(self):
        return self._struct_decls

    def add_struct_decl(self, struct_name):
        self._struct_decls.add(struct_name)

    def add_struct_decls(self, struct_names):
        self._struct_decls.update(struct_names)

    @staticmethod
    def require_struct_decls(struct_names):
        return lambda accumulator: accumulator.add_struct_decls(struct_names)
