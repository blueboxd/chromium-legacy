# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from .mako_renderer import MakoRenderer
from .clang_format import clang_format


def run_example(web_idl_database, output_dirs):
    mako = MakoRenderer()
    mako_output = mako.render('example.cc.tmpl', web_idl=web_idl_database)

    filename = 'v8_example.cc'
    formatted_result = clang_format(mako_output, filename=filename)

    with open(os.path.join(output_dirs['core'], filename), 'w') as output_file:
        output_file.write(formatted_result.contents)
