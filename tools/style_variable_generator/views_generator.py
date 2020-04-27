# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from base_generator import Color, Modes, BaseGenerator


class ViewsStyleGenerator(BaseGenerator):
    '''Generator for Views Variables'''

    def Render(self):
        self.Validate()
        return self.ApplyTemplate(self, 'views_generator_h.tmpl',
                                  self.GetParameters())

    def GetParameters(self):
        return {
            'colors': self._CreateColorList(),
        }

    def GetFilters(self):
        return {
            'to_const_name': self._ToConstName,
            'cpp_color': self._CppColor,
        }

    def GetGlobals(self):
        globals = {
            'Modes': Modes,
            'out_file_path': None,
            'namespace_name': None,
        }
        if self.out_file_path:
            globals['out_file_path'] = self.out_file_path
            globals['namespace_name'] = os.path.splitext(
                os.path.basename(self.out_file_path))[0]
        return globals

    def _CreateColorList(self):
        color_list = []
        for color_name in (
                self._mode_variables[self._default_mode].colors.keys()):
            color_obj = {'name': color_name, 'mode_values': {}}
            for m in Modes.ALL:
                mode_colors = self._mode_variables[m].colors
                if color_name in mode_colors:
                    color_obj['mode_values'][m] = mode_colors[color_name]
            color_list.append(color_obj)

        return color_list

    def _ToConstName(self, var_name):
        return 'k%s' % var_name.title().replace('_', '')

    def _CppColor(self, c):
        '''Returns the C++ color representation of |c|'''
        assert (isinstance(c, Color))

        def AlphaToInt(alpha):
            return int(alpha * 255)

        if c.var:
            return ('ResolveColor(ColorName::%s, color_mode)' %
                    self._ToConstName(c.var))

        if c.rgb_var:
            return (
                'SkColorSetA(ResolveColor(ColorName::%s, color_mode), 0x%X)' %
                (self._ToConstName(c.RGBVarToVar()), AlphaToInt(c.a)))

        if c.a != 1:
            return 'SkColorSetARGB(0x%X, 0x%X, 0x%X, 0x%X)' % (AlphaToInt(c.a),
                                                               c.r, c.g, c.b)
        else:
            return 'SkColorSetRGB(0x%X, 0x%X, 0x%X)' % (c.r, c.g, c.b)
