# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .common import WithIdentifier


class Proxy(object):
    """
    Proxies attribute access on this object to the target object.
    """

    def __init__(self,
                 target_object=None,
                 target_attrs=None,
                 target_attrs_with_priority=None):
        """
        Creates a new proxy to |target_object|.

        Keyword arguments:
        target_object -- The object to which attribute access is proxied.  This
            can be set later by set_target_object.
        target_attrs -- None or list of attribute names to be proxied.  If None,
            all the attribute access is proxied.
        target_attrs_with_priority -- None or list of attribute names to be
            unconditionally proxied with priority over attributes defined on
            |self|.  If None, no attribute has priority over own attributes.
        """
        if target_attrs is not None:
            assert isinstance(target_attrs, (tuple, list))
            assert all(isinstance(attr, str) for attr in target_attrs)
        self._target_object = target_object
        self._target_attrs = target_attrs
        self._target_attrs_with_priority = target_attrs_with_priority

    def __getattr__(self, attribute):
        target_object = object.__getattribute__(self, '_target_object')
        target_attrs = object.__getattribute__(self, '_target_attrs')
        assert target_object
        if target_attrs is None or attribute in target_attrs:
            return getattr(target_object, attribute)
        raise AttributeError

    def __getattribute__(self, attribute):
        target_object = object.__getattribute__(self, '_target_object')
        target_attrs = object.__getattribute__(self,
                                               '_target_attrs_with_priority')
        # It's okay to access own attributes, such as 'identifier', even when
        # the target object is not yet resolved.
        if target_object is None:
            return object.__getattribute__(self, attribute)
        if target_attrs is not None and attribute in target_attrs:
            return getattr(target_object, attribute)
        return object.__getattribute__(self, attribute)

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
    can apply a function to all the references.  This allows you to resolve all
    the references at very end of the compilation phases.
    """

    class _RefById(Proxy, WithIdentifier):
        """
        Represents a reference to an object specified with the given identifier,
        which reference will be resolved later.

        This reference is also a proxy to the object for convenience so that you
        can treat this reference as if the object itself.
        """

        def __init__(self,
                     identifier,
                     target_attrs=None,
                     target_attrs_with_priority=None):
            Proxy.__init__(
                self,
                target_attrs=target_attrs,
                target_attrs_with_priority=target_attrs_with_priority)
            WithIdentifier.__init__(self, identifier)

    def __init__(self, target_attrs=None, target_attrs_with_priority=None):
        self._references = set()
        self._did_resolve = False
        self._target_attrs = target_attrs
        self._target_attrs_with_priority = target_attrs_with_priority

    @classmethod
    def is_reference(cls, obj):
        return isinstance(obj, cls._RefById)

    def create(self, identifier):
        assert not self._did_resolve
        ref = RefByIdFactory._RefById(identifier, self._target_attrs,
                                      self._target_attrs_with_priority)
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
