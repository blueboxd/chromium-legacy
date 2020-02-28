# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import collections
import re
import textwrap
from color import Color

_FILE_PATH = os.path.dirname(os.path.realpath(__file__))

_JSON5_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party',
                           'pyjson5', 'src')
sys.path.insert(1, _JSON5_PATH)
import json5

_JINJA2_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party')
sys.path.insert(1, _JINJA2_PATH)
import jinja2


class Modes:
    LIGHT = 'light'
    DARK = 'dark'
    ALL = [LIGHT, DARK]


class ModeVariables:
    '''A representation of variables to generate for a single Mode.
    '''

    def __init__(self):
        self.colors = collections.OrderedDict()

    def AddColor(self, name, value_str):
        if name in self.colors:
            raise ValueError('%s defined more than once' % name)

        try:
            self.colors[name] = Color(value_str)
        except Exception as err:
            raise ValueError('Error parsing "%s": %s' % (value_str, err))


class BaseGenerator:
    '''A generic style variable generator.

    Subclasses should provide format-specific generation templates, filters and
    globals to render their output.
    '''

    def __init__(self):
        self._mode_variables = dict()
        # The mode that colors will fallback to when not specified in a
        # non-default mode. An error will be raised if a color in any mode is
        # not specified in the default mode.
        self._default_mode = Modes.LIGHT

        for mode in Modes.ALL:
            self._mode_variables[mode] = ModeVariables()

    def AddColor(self, name, value_obj):
        if isinstance(value_obj, unicode):
            self._mode_variables[self._default_mode].AddColor(name, value_obj)
        elif isinstance(value_obj, dict):
            for mode in Modes.ALL:
                if mode in value_obj:
                    self._mode_variables[mode].AddColor(name, value_obj[mode])

    def AddJSONFileToModel(self, path):
        with open(path, 'r') as f:
            # TODO(calamity): Add allow_duplicate_keys=False once pyjson5 is
            # rolled.
            data = json5.loads(
                f.read(), object_pairs_hook=collections.OrderedDict)

        try:
            for name, value in data['colors'].items():
                if not re.match('^[a-z0-9_]+$', name):
                    raise ValueError(
                        '%s is not a valid variable name (lower case, 0-9, _)'
                        % name)

                self.AddColor(name, value)
        except Exception as err:
            raise ValueError('\n%s:\n    %s' % (path, err))

    def ApplyTemplate(self, style_generator, path_to_template, params):
        current_dir = os.path.dirname(os.path.realpath(__file__))
        jinja_env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(current_dir),
            keep_trailing_newline=True)
        jinja_env.globals.update(style_generator.GetGlobals())
        jinja_env.filters.update(style_generator.GetFilters())
        template = jinja_env.get_template(path_to_template)
        return template.render(params)

    def Validate(self):
        def CheckIsInDefaultModel(name):
            if name not in self._mode_variables[self._default_mode].colors:
                raise ValueError(
                    "%s not defined in '%s' mode" % (name, self._default_mode))

        # Check all colors in all models refer to colors that exist in the
        # default mode.
        for m in self._mode_variables.values():
            for name, value in m.colors.items():
                if value.var:
                    CheckIsInDefaultModel(value.var)
                if value.rgb_var:
                    CheckIsInDefaultModel(value.rgb_var[:-4])

        # TODO(calamity): Check for circular references.

    # TODO(crbug.com/1053372): Prune unused rgb values.
