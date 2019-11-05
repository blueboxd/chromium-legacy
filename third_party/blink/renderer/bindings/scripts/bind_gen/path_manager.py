# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

from . import name_style


class PathManager(object):
    """
    Provides a variety of paths such as Blink headers and output files.

    About output files, there are two cases.
    - cross-components case:
        APIs are generated in 'core' and implementations are generated in
        'modules'.
    - single component case:
        Everything is generated in a single component.
    """

    def __init__(self, idl_definition, output_dirs):
        idl_path = idl_definition.debug_info.location.filepath
        self._idl_basepath, _ = os.path.splitext(idl_path)
        self._idl_dir, self._idl_basename = os.path.split(self._idl_basepath)

        components = idl_definition.components
        assert 0 < len(components)

        if len(components) == 1:
            component = components[0]
            self._is_cross_components = False
            self._api_component = component
            self._impl_component = component
        else:
            assert ("core", "modules") == sorted(components)
            self._is_cross_components = True
            self._api_component = "core"
            self._impl_component = "modules"

        self._api_dir = output_dirs[self._api_component]
        self._impl_dir = output_dirs[self._impl_component]

        self._out_basename = name_style.file("v8", idl_definition.identifier)

    @property
    def idl_dir(self):
        return self._idl_dir

    def blink_path(self, filename=None, ext=None):
        # The IDL file and Blink headers are supposed to exist in the same
        # directory.
        return self._join(
            dirpath=self.idl_dir,
            filename=(filename or self._idl_basename),
            ext=ext)

    @property
    def is_cross_components(self):
        return self._is_cross_components

    @property
    def api_component(self):
        return self._api_component

    @property
    def api_dir(self):
        return self._api_dir

    def api_path(self, filename=None, ext=None):
        return self._join(
            dirpath=self.api_dir,
            filename=(filename or self._out_basename),
            ext=ext)

    @property
    def impl_component(self):
        return self._impl_component

    @property
    def impl_dir(self):
        return self._impl_dir

    def impl_path(self, filename=None, ext=None):
        return self._join(
            dirpath=self.impl_dir,
            filename=(filename or self._out_basename),
            ext=ext)

    def _join(self, dirpath, filename, ext=None):
        if ext is not None:
            filename = "{}.{}".format(filename, ext)
        return os.path.join(dirpath, filename)
