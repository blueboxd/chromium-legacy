# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mako.lookup
import mako.template


class MakoRenderer(object):
    """Represents a renderer object implemented with Mako templates."""

    def __init__(self, template_dirs=None):
        template_dirs = template_dirs or []
        self._template_lookup = mako.lookup.TemplateLookup(
            directories=template_dirs)

        self._caller_stack = []

    def render(self,
               caller,
               template_path=None,
               template_text=None,
               template_vars=None):
        """
        Renders the template with variable bindings.

        It's okay to invoke |render| method recursively and |caller| is pushed
        onto the call stack, which is accessible via |callers| and |last_caller|
        methods.

        Args:
            template_path: A filepath to a template file.
            template_text: A text content to be used as a template.  Either of
                |template_path| or |template_text| must be specified.
            template_vars: Template variable bindings.
            caller: An object to be pushed onto the call stack.
        """

        assert template_path is not None or template_text is not None
        assert template_path is None or template_text is None
        assert isinstance(template_vars, dict)
        assert caller is not None

        self._caller_stack.append(caller)

        try:
            if template_path is not None:
                template = self._template_lookup.get_template(template_path)
            elif template_text is not None:
                template = mako.template.Template(text=template_text)
            text = template.render(**template_vars)
        finally:
            self._caller_stack.pop()

        return text

    @property
    def callers(self):
        """
        Returns the callers of this renderer in the order from the last caller
        to the first caller.
        """
        return reversed(self._caller_stack)

    @property
    def last_caller(self):
        """Returns the last caller in the call stack of this renderer."""
        return self._caller_stack[-1]
