# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def make_copy(obj, memo=None):
    """
    Creates a copy of the given object, which should be an IR or part of IR.

    The copy is created basically as a deep copy of the object, but |make_copy|
    method is used to create a (part of) copy if the object (or part of it) has
    the method.  |memo| argument behaves as the same as |deepcopy|.
    """

    if memo is None:
        memo = dict()

    if obj is None:
        return None

    if hasattr(obj, 'make_copy'):
        return obj.make_copy(memo=memo)

    if obj.__hash__ is not None:
        copy = memo.get(obj, None)
        if copy is not None:
            return copy

    cls = type(obj)

    if isinstance(obj, (bool, int, long, float, complex, basestring)):
        # Subclasses of simple builtin types are expected to have a copy
        # constructor.
        return cls.__new__(cls, obj)

    if isinstance(obj, (list, tuple, set, frozenset)):
        return cls(map(lambda x: make_copy(x, memo), obj))

    if isinstance(obj, dict):
        return cls([(make_copy(key, memo), make_copy(value, memo))
                    for key, value in obj.iteritems()])

    if hasattr(obj, '__dict__'):
        copy = cls.__new__(cls)
        memo[obj] = copy
        for name, value in obj.__dict__.iteritems():
            setattr(copy, name, make_copy(value, memo))
        return copy

    assert False, 'Unsupported type of object: {}'.format(cls)
