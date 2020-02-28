# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from base_generator import Color, Modes, BaseGenerator


class CSSStyleGenerator(BaseGenerator):
    '''Generator for CSS Variables'''

    def Render(self):
        self.Validate()
        return self.ApplyTemplate(self, 'css_generator.tmpl',
                                  self.GetParameters())

    def GetParameters(self):
        return {
            'light_variables': self._mode_variables[Modes.LIGHT],
            'dark_variables': self._mode_variables[Modes.DARK],
        }

    def GetFilters(self):
        return {
            'to_var_name': self._ToVarName,
            'css_color': self._CssColor,
            'css_color_rgb': self._CssColorRGB,
        }

    def GetGlobals(self):
        return {
            'css_color_from_rgb_var': self._CssColorFromRGBVar,
        }

    def _ToVarName(self, var_name):
        return '--%s' % var_name.replace('_', '-')

    def _CssColor(self, c):
        '''Returns the CSS color representation of |c|'''
        assert (isinstance(c, Color))
        if c.var:
            return 'var(%s)' % self._ToVarName(c.var)

        if c.rgb_var:
            if c.a != 1:
                return 'rgba(var(%s), %g)' % (self._ToVarName(c.rgb_var), c.a)
            else:
                return 'rgb(var(%s))' % self._ToVarName(c.rgb_var)

        if c.a != 1:
            return 'rgba(%d, %d, %d, %g)' % (c.r, c.g, c.b, c.a)
        else:
            return 'rgb(%d, %d, %d)' % (c.r, c.g, c.b)

    def _CssColorRGB(self, c):
        '''Returns the CSS rgb representation of |c|'''
        if c.var:
            return 'var(%s-rgb)' % self._ToVarName(c.var)

        if c.rgb_var:
            return 'var(%s)' % self._ToVarName(c.rgb_var)

        return '%d, %d, %d' % (c.r, c.g, c.b)

    def _CssColorFromRGBVar(self, name, alpha):
        '''Returns the CSS color representation given a color name and alpha'''
        if alpha != 1:
            return 'rgba(var(%s-rgb), %g)' % (self._ToVarName(name), alpha)
        else:
            return 'rgb(var(%s-rgb))' % self._ToVarName(name)
