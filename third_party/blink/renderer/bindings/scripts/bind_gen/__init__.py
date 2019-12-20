# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import platform
import sys


# Set up |sys.path| so that this module works without user-side setup of
# PYTHONPATH assuming Chromium's directory tree structure.
def _setup_sys_path():
    expected_path = 'third_party/blink/renderer/bindings/scripts/bind_gen/'

    this_dir = os.path.dirname(__file__)
    root_dir = os.path.join(this_dir, *(['..'] * expected_path.count('/')))

    sys.path = [
        # //third_party/blink/renderer/bindings/scripts/web_idl
        os.path.join(root_dir, 'third_party', 'blink', 'renderer', 'bindings',
                     'scripts'),
        # //third_party/blink/renderer/build/scripts/blinkbuild
        os.path.join(root_dir, 'third_party', 'blink', 'renderer', 'build',
                     'scripts'),
        # //third_party/mako/mako
        os.path.join(root_dir, 'third_party', 'mako'),
    ] + sys.path


_setup_sys_path()


from . import clang_format
from .dictionary import generate_dictionaries
from .interface import generate_interfaces
from .path_manager import PathManager


def _setup_clang_format():
    expected_path = 'third_party/blink/renderer/bindings/scripts/bind_gen/'

    this_dir = os.path.dirname(__file__)
    root_dir = os.path.join(this_dir, *(['..'] * expected_path.count('/')))

    # //third_party/depot_tools/clang-format
    command_name = ('clang-format.bat'
                    if platform.system() == 'Windows' else 'clang-format')
    command_path = os.path.abspath(
        os.path.join(root_dir, 'third_party', 'depot_tools', command_name))

    clang_format.init(command_path=command_path)


def init(root_src_dir, root_gen_dir, component_reldirs):
    """
    Args:
        root_src_dir: Project's root directory, which corresponds to "//" in GN.
        root_gen_dir: Root directory of generated files, which corresponds to
            "//out/Default/gen" in GN.
        component_reldirs: Pairs of component and output directory.
    """
    _setup_clang_format()
    PathManager.init(
        root_src_dir=root_src_dir,
        root_gen_dir=root_gen_dir,
        component_reldirs=component_reldirs)
