# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys


# Set up |sys.path| so that this module works without user-side setup of
# PYTHONPATH assuming Chromium's directory tree structure.
def _setup_sys_path():
    expected_path = 'third_party/blink/renderer/bindings/scripts/web_idl/'

    this_dir = os.path.dirname(__file__)
    root_dir = os.path.join(this_dir, *(['..'] * expected_path.count('/')))
    sys.path = [
        # //third_party/ply
        os.path.join(root_dir, 'third_party'),
        # //tools/idl_parser
        os.path.join(root_dir, 'tools'),
        # //third_party/blink/renderer/build/scripts/blinkbuild
        os.path.join(this_dir, '..', '..', '..', 'build', 'scripts'),
    ] + sys.path


_setup_sys_path()


from .ast_group import AstGroup
from .composition_parts import Component
from .database import Database
from .database_builder import build_database


__all__ = [
    "AstGroup",
    "Component",
    "Database",
    "build_database",
]
