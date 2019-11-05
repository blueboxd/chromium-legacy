# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
COLOR_PALETTE_RELATIVE_PATH = 'ui/android/java/res/values/color_palette.xml'
COLOR_PALETTE_PATH = os.path.join(_SRC_ROOT, COLOR_PALETTE_RELATIVE_PATH)
SEMANTIC_COLORS_PATH = os.path.join(_SRC_ROOT,
    'ui/android/java/res/values/semantic_colors.xml')

COLOR_PATTERN = re.compile(r'(>|")(#[0-9A-Fa-f]+)(<|")')
VALID_COLOR_PATTERN = re.compile(
    r'^#([0-9A-F][0-9A-E]|[0-9A-E][0-9A-F])?[0-9A-F]{6}$')
XML_APP_NAMESPACE_PATTERN = re.compile(
    r'xmlns:(\w+)="http://schemas.android.com/apk/res-auto"')
TEXT_APPEARANCE_STYLE_PATTERN = re.compile(r'^TextAppearance\.')
INCLUDED_PATHS = [
    r'^(chrome|ui|components|content)[\\/](.*[\\/])?java[\\/]res.+\.xml$'
]
# TODO(lazzzis): check color references in java source files
COLOR_REFERENCE_PATTERN = re.compile('''
    @color/   # starts with '@color'
    [\w|_]+   # color name is only composed of numbers, letters and underscore
''', re.VERBOSE)
