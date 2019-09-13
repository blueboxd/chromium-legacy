# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from mako.lookup import TemplateLookup

_TEMPLATES_DIR = os.path.dirname(__file__)


class MakoRenderer(object):
    def __init__(self, template_dirs=None):
        template_dirs = template_dirs or [_TEMPLATES_DIR]
        self._template_lookup = TemplateLookup(directories=template_dirs)

    def render(self, template_path, **variable_bindings):
        template = self._template_lookup.get_template(template_path)
        return template.render(**variable_bindings)
