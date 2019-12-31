# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import mako.template


_MAKO_TEMPLATE_PASS_KEY = object()


class MakoTemplate(object):
    """Represents a compiled template object."""

    _mako_template_cache = {}

    def __init__(self, template_text):
        assert isinstance(template_text, str)

        template_params = {
            "strict_undefined": True,
        }

        template = self._mako_template_cache.get(template_text)
        if template is None:
            template = mako.template.Template(
                text=template_text, **template_params)
            self._mako_template_cache[template_text] = template
        self._template = template

    def mako_template(self, pass_key=None):
        assert pass_key is _MAKO_TEMPLATE_PASS_KEY
        return self._template


class MakoRenderer(object):
    """Represents a renderer object implemented with Mako templates."""

    def __init__(self):
        self._caller_stack = []
        self._caller_stack_on_error = []

    def render(self, caller, template, template_vars):
        """
        Renders the template with variable bindings.

        It's okay to invoke |render| method recursively and |caller| is pushed
        onto the call stack, which is accessible via
        |callers_from_first_to_last| method, etc.

        Args:
            caller: An object to be pushed onto the call stack.
            template: A MakoTemplate.
            template_vars: A dict of template variable bindings.
        """
        assert caller is not None
        assert isinstance(template, MakoTemplate)
        assert isinstance(template_vars, dict)

        self._caller_stack.append(caller)

        try:
            mako_template = template.mako_template(
                pass_key=_MAKO_TEMPLATE_PASS_KEY)
            text = mako_template.render(**template_vars)
        except:
            # Print stacktrace of template rendering.
            sys.stderr.write("\n")
            sys.stderr.write("==== template rendering error ====\n")
            sys.stderr.write("  * name: {}, type: {}\n".format(
                _guess_caller_name(self.last_caller), type(self.last_caller)))
            sys.stderr.write("  * depth: {}, module_id: {}\n".format(
                len(self._caller_stack), mako_template.module_id))
            sys.stderr.write("---- template source ----\n")
            sys.stderr.write(mako_template.source)

            # Save the error state at the deepest call.
            current = self._caller_stack
            on_error = self._caller_stack_on_error
            if (len(current) <= len(on_error)
                    and all(current[i] == on_error[i]
                            for i in xrange(len(current)))):
                pass  # Error happened in a deeper caller.
            else:
                self._caller_stack_on_error = list(self._caller_stack)

            raise
        finally:
            self._caller_stack.pop()

        return text

    @property
    def callers_from_first_to_last(self):
        """
        Returns the callers of this renderer in the order from the first caller
        to the last caller.
        """
        return iter(self._caller_stack)

    @property
    def callers_from_last_to_first(self):
        """
        Returns the callers of this renderer in the order from the last caller
        to the first caller.
        """
        return reversed(self._caller_stack)

    @property
    def last_caller(self):
        """Returns the last caller in the call stack of this renderer."""
        return self._caller_stack[-1]

    @property
    def callers_on_error(self):
        """
        Returns the callers of this renderer in the order from the last caller
        to the first caller at the moment when an exception was thrown.
        """
        return reversed(self._caller_stack_on_error)

    @property
    def last_caller_on_error(self):
        """
        Returns the deepest caller at the moment when an exception was thrown.
        """
        return self._caller_stack_on_error[-1]


def _guess_caller_name(caller):
    """Returns the best-guessed name of |caller|."""
    try:
        # Outer CodeNode may have a binding to the caller.
        for name, value in caller.outer.template_vars.iteritems():
            if value is caller:
                return name
        try:
            # Outer ListNode may contain the caller.
            for index, value in enumerate(caller.outer, 1):
                if value is caller:
                    return "{}-of-{}-in-list".format(index, len(caller.outer))
        except:
            pass
        return "<no name>"
    except:
        return "<unknown>"
