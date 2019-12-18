#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander

from core.css import css_properties


class StyleCascadeSlotsWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(StyleCascadeSlotsWriter, self).__init__([], output_dir)

        json_properties = css_properties.CSSProperties(json5_file_paths)

        self._properties = json_properties.longhands
        self._outputs = {
            'style_cascade_slots.h': self.generate_header,
            'style_cascade_slots.cc': self.generate_impl,
        }

    @template_expander.use_jinja('core/css/templates/style_cascade_slots.h.tmpl')
    def generate_header(self):
        return {'properties': self._properties}

    @template_expander.use_jinja('core/css/templates/style_cascade_slots.cc.tmpl')
    def generate_impl(self):
        return {'properties': self._properties}

if __name__ == '__main__':
    json5_generator.Maker(StyleCascadeSlotsWriter).main()
