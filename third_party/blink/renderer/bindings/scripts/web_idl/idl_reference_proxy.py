# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .common import WithIdentifier


class Proxy(object):
    """
    Proxies attribute access on this object to the target object.
    """

    def __init__(self, target_object=None, target_attributes=None):
        """
        Creates a new proxy to |target_object|.

        Keyword arguments:
        target_object -- The object to which attribute access is proxied.  This
            can be set later by set_target_object.
        target_attributes -- None or list of attribute names to be proxied.  If
            None, all the attribute access is proxied.
        """
        if target_attributes is not None:
            assert isinstance(target_attributes, (tuple, list))
            assert all(isinstance(attr, str) for attr in target_attributes)
        self._target_object = target_object
        self._target_attributes = target_attributes

    def __getattr__(self, attribute):
        assert self._target_object
        if (self._target_attributes is None
                or attribute in self._target_attributes):
            return getattr(self._target_object, attribute)
        raise AttributeError

    def set_target_object(self, target_object):
        assert self._target_object is None
        assert isinstance(target_object, object)
        self._target_object = target_object

    @property
    def target_object(self):
        assert self._target_object
        return self._target_object


class RefByIdFactory(object):
    """
    Creates a group of references that are later resolvable.

    All the references created by this factory are grouped per factory, and you
    can apply a function to all the references.  This allows you to resolve
    all the references at very end of the compilation phases.
    """

    class _RefById(Proxy, WithIdentifier):
        """
        Represents a reference to an object specified with the given identifier,
        which reference will be resolved later.

        This reference is also a proxy to the object for convenience so that you
        can treat this reference as if the object itself.
        """

        def __init__(self, identifier, target_attributes=None):
            Proxy.__init__(self, target_attributes=target_attributes)
            WithIdentifier.__init__(self, identifier)

    def __init__(self, target_attributes):
        self._references = set()
        self._did_resolve = False
        self._target_attributes = target_attributes

    def create(self, identifier):
        assert not self._did_resolve
        ref = RefByIdFactory._RefById(identifier, self._target_attributes)
        self._references.add(ref)
        return ref

    def resolve(self, resolver):
        """
        Applies a callback function |resolver| to all the references created by
        this factory.
        """
        assert not self._did_resolve
        self._did_resolve = True
        for ref in self._references:
            resolver(ref)
