# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file defines some classes to implement extended attributes
# https://heycam.github.io/webidl/#idl-extended-attributes

from itertools import groupby


class ExtendedAttribute(object):
    """ExtendedAttribute represents an exnteded attribute that
    is described in either of following formats as spec defines.
    a. [Key]
    b. [Key=Value]
    c. [Key=(Value1,Value2,...)]
    d. [Key(ValueL1 ValueR1, ValueL2 ValueR2,...)]
    e. [Key=Name(ValueL1 ValueR1, ValueL2 ValueR2,...)]
    """
    _FORM_NO_ARGS = 'NoArgs'  # for (a)
    _FORM_IDENT = 'Ident'  # for (b)
    _FORM_IDENT_LIST = 'IdentList'  # for (c)
    _FORM_ARG_LIST = 'ArgList'  # for (d)
    _FORM_NAMED_ARG_LIST = 'NamedArgList'  # for (e)

    def __init__(self, key, values=None, arguments=None, name=None):
        # |values| can be either of None, a single string, or a list.
        assert values is None or isinstance(values, (str, tuple, list))
        # |arguments| can be either of None or a list of pairs of strings.
        assert arguments is None or isinstance(arguments, (tuple, list))

        self._format = None
        self._key = key
        self._values = values
        self._arguments = arguments
        self._name = name

        if name is not None:
            self._format = self._FORM_NAMED_ARG_LIST
            if values is not None or arguments is None:
                raise ValueError('Unknown format for ExtendedAttribute')
        elif arguments is not None:
            assert all(
                isinstance(arg, (tuple, list)) and len(arg) == 2
                and isinstance(arg[0], str) and isinstance(arg[1], str)
                for arg in arguments)
            self._format = self._FORM_ARG_LIST
            if values is not None:
                raise ValueError('Unknown format for ExtendedAttribute')
        elif values is None:
            self._format = self._FORM_NO_ARGS
        elif isinstance(values, (tuple, list)):
            self._format = self._FORM_IDENT_LIST
            self._values = tuple(values)
        elif isinstance(values, str):
            self._format = self._FORM_IDENT
        else:
            raise ValueError('Unknown format for ExtendedAttribute')

    def __str__(self):
        if self._format == self._FORM_NO_ARGS:
            return self._key
        if self._format == self._FORM_IDENT:
            return '{}={}'.format(self._key, self._values)
        if self._format == self._FORM_IDENT_LIST:
            return '{}=({})'.format(self._key, ', '.join(self._values))
        args_str = '({})'.format(', '.join(
            [' '.join(arg) for arg in self._arguments]))
        if self._format == self._FORM_ARG_LIST:
            return '{}{}'.format(self._key, args_str)
        if self._format == self._FORM_NAMED_ARG_LIST:
            return '{}={}{}'.format(self._key, self._name, args_str)
        # Should not reach here.
        assert False, 'Unknown format: {}'.format(self._format)

    def make_copy(self):
        return ExtendedAttribute(
            key=self._key,
            values=self._values,
            arguments=self._arguments,
            name=self._name)

    @property
    def key(self):
        """
        Returns the key.
        @return str
        """
        return self._key

    @property
    def value(self):
        """
        Returns the value for the format Ident. Returns None for the format
        NoArgs. Raises an error otherwise.
        @return str
        """
        if self._format in (self._FORM_NO_ARGS, self._FORM_IDENT):
            return self._values
        raise ValueError('"{}" does not have a single value.'.format(
            str(self)))

    @property
    def values(self):
        """
        Returns a list of values for formats Ident and IdentList. Returns an
        empty list for the format NorArgs. Raises an error otherwise.
        @return tuple(str)
        """
        if self._format == self._FORM_NO_ARGS:
            return ()
        if self._format == self._FORM_IDENT:
            return (self._values, )
        if self._format == self._FORM_IDENT_LIST:
            return self._values
        raise ValueError('"{}" does not have values.'.format(str(self)))

    @property
    def arguments(self):
        """
        Returns a tuple of value pairs for formats ArgList and NamedArgList.
        Raises an error otherwise.
        @return tuple(Argument)
        """
        if self._format in (self._FORM_ARG_LIST, self._FORM_NAMED_ARG_LIST):
            return self._arguments
        raise ValueError('"{}" does not have arguments.'.format(str(self)))

    @property
    def name(self):
        """
        Returns |Name| for the format NamedArgList. Raises an error otherwise.
        @return str
        """
        if self._format == self._FORM_NAMED_ARG_LIST:
            return self._name
        raise ValueError('"{}" does not have a name.'.format(str(self)))


class ExtendedAttributes(object):
    """
    ExtendedAttributes is a dict-like container for ExtendedAttribute instances.
    With a key string, you can get an ExtendedAttribute or a list of them.

    For an IDL fragment
      [A, A=(foo, bar), B=baz]
    the generated ExtendedAttributes instance will be like
      {
        'A': (ExtendedAttribute('A'), ExtendedAttribute('A', values=('foo', 'bar'))),
        'B': (ExtendedAttribute('B', value='baz')),
      }
    """

    def __init__(self, attributes=None):
        assert attributes is None or (isinstance(attributes, (list, tuple))
                                      and all(
                                          isinstance(attr, ExtendedAttribute)
                                          for attr in attributes))
        attributes = sorted(attributes or [], key=lambda x: x.key)
        self._attributes = {
            k: tuple(v)
            for k, v in groupby(attributes, key=lambda x: x.key)
        }

    def __bool__(self):
        return bool(self._attributes)

    def __contains__(self, key):
        """
        Returns True if this has an extended attribute with the |key|.
        @param  str key
        @return bool
        """
        return key in self._attributes

    def __iter__(self):
        """
        Yields all ExtendedAttribute instances.
        """
        for attrs in self._attributes.values():
            for attr in attrs:
                yield attr

    def __len__(self):
        return len(list(self.__iter__()))

    def __str__(self):
        attrs = [str(attr) for attr in self]
        return '[{}]'.format(', '.join(attrs))

    def make_copy(self):
        return ExtendedAttributes(map(ExtendedAttribute.make_copy, self))

    def keys(self):
        return self._attributes.keys()

    def get(self, key):
        """
        Returns an exnteded attribute whose key is |key|.
        If |self| has no such elements, returns None,
        and if |self| has multiple elements, raises an error.
        @param  str key
        @return ExtendedAttriute?
        """
        values = self.get_list_of(key)
        if len(values) == 0:
            return None
        if len(values) == 1:
            return values[0]
        raise ValueError(
            'There are multiple extended attributes for the key "{}"'.format(
                key))

    def get_list_of(self, key):
        """
        Returns a list of extended attributes whose keys are |identifier|.
        @param  str key
        @return tuple(ExtendedAttribute)
        """
        return self._attributes.get(key, ())

    def append(self, extended_attribute):
        assert isinstance(extended_attribute, ExtendedAttribute)
        key = extended_attribute.key
        self._attributes[key] = self.get_list_of(key) + (extended_attribute, )
