# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def make_copy(obj):
    """
    Creates a copy of the given object, which should be an IR or part of IR.

    The copy is created basically as a deep copy of the object, but |make_copy|
    method is used to create a (part of) copy if the object (or part of it) has
    the method.
    """

    def construct(class_, *args):
        return class_.__new__(class_, *args)

    if obj is None:
        return None

    if hasattr(obj, 'make_copy'):
        return obj.make_copy()

    if isinstance(obj, (bool, int, long, float, complex, basestring)):
        # Subclasses of simple builtin types are expected to have a copy
        # constructor.
        return construct(type(obj), obj)

    if isinstance(obj, (list, tuple, set, frozenset)):
        return type(obj)(map(make_copy, obj))

    if isinstance(obj, dict):
        return type(obj)([(make_copy(key), make_copy(value))
                          for key, value in obj.iteritems()])

    if hasattr(obj, '__dict__'):
        copy = construct(type(obj))
        for name, value in obj.__dict__.iteritems():
            setattr(copy, name, make_copy(value))
        return copy

    assert False, 'Unsupported type of object: {}'.format(type(obj))
