# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .common import WithIdentifier


class Proxy(object):
    """
    Proxies attributes of the target object.  The target object can be set
    after the construction, but its attributes to proxy must be known in the
    construction.
    """

    def __init__(self, target_object=None, target_attributes=None):
        self._target_object = target_object
        self._target_attributes = target_attributes

    def __getattr__(self, attribute):
        assert self._target_object
        assert self._target_attributes
        if attribute in self._target_attributes:
            return getattr(self._target_object, attribute)
        raise AttributeError

    def set_target_object(self, target_object):
        assert self._target_object is None
        self._target_object = target_object

    def set_target_attributes(self, target_attributes):
        assert self._target_attrributes is None
        self._target_attributes = target_attributes

    @property
    def target_object(self):
        assert self._target_object
        return self._target_object


class IdlReferenceFactory(object):
    """
    Creates reference proxies to refer an IDL definition.

    Because we cannot have direct references to IDL definitions until we
    actually make the definitions, but we need the references to point to
    definitions when compiling IDLs.
    """

    class _IdlReference(Proxy, WithIdentifier):
        """
        Proxies attributes of target object, that can be specified by an
        identifier.
        """

        def __init__(self, identifier):
            Proxy.__init__(self)
            WithIdentifier.__init__(self, identifier)

    def __init__(self):
        self._idl_references = set()
        self._resolved = False

    def create(self, identifier):
        assert not self._resolved
        ref = IdlReferenceFactory._IdlReference(identifier)
        self._idl_references.add(ref)
        return ref

    def resolve(self, resolver):
        """
        Updates stored references' targets.

        |resolver| is a callback function that takes a Proxy
        instance and updates its target object.
        """
        assert not self._resolved
        self._resolved = True
        for ref in self._idl_references:
            resolver(ref)
