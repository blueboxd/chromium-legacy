# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class _GlobalNameAndFeature(object):
    def __init__(self, global_name, feature=None):
        assert isinstance(global_name, str)
        assert feature is None or isinstance(feature, str)

        self._global_name = global_name
        self._feature = feature

    @property
    def global_name(self):
        return self._global_name

    @property
    def feature(self):
        return self._feature


class Exposure(object):
    def __init__(self, other=None):
        assert other is None or isinstance(other, Exposure)

        if other:
            self._global_names_and_features = tuple(
                other.global_names_and_features)
            self._runtime_enabled_features = tuple(
                other.runtime_enabled_features)
            self._context_enabled_features = tuple(
                other.context_enabled_features)
            self._only_in_secure_contexts = other.only_in_secure_contexts
        else:
            self._global_names_and_features = tuple()
            self._runtime_enabled_features = tuple()
            self._context_enabled_features = tuple()
            self._only_in_secure_contexts = False

    @property
    def global_names_and_features(self):
        """
        Returns a list of pairs of global name and runtime enabled feature,
        which is None if not applicable.
        """
        return self._global_names_and_features

    @property
    def runtime_enabled_features(self):
        """
        Returns a list of runtime enabled features.  This construct is exposed
        only when these features are enabled.
        """
        return self._runtime_enabled_features

    @property
    def context_enabled_features(self):
        """
        Returns a list of context enabled features.  This construct is exposed
        only when these features are enabled in the context.
        """
        return self._context_enabled_features

    @property
    def only_in_secure_contexts(self):
        """
        Returns whether this construct is available only in secure contexts or
        not.  The returned value will be either of a boolean (always true or
        false) or a flag name (only when the flag is enabled).

        https://heycam.github.io/webidl/#dfn-available-only-in-secure-contexts
        """
        return self._only_in_secure_contexts


class ExposureMutable(Exposure):
    def __init__(self):
        Exposure.__init__(self)

        self._global_names_and_features = []
        self._runtime_enabled_features = []
        self._context_enabled_features = []
        self._only_in_secure_contexts = False

    def __getstate__(self):
        assert False, "ExposureMutable must not be pickled."

    def __setstate__(self, state):
        assert False, "ExposureMutable must not be pickled."

    def add_global_name_and_feature(self, global_name, feature_name=None):
        self._global_names_and_features.append(
            _GlobalNameAndFeature(global_name, feature_name))

    def add_runtime_enabled_feature(self, name):
        assert isinstance(name, str)
        self._runtime_enabled_features.append(name)

    def add_context_enabled_feature(self, name):
        assert isinstance(name, str)
        self._context_enabled_features.append(name)

    def set_only_in_secure_contexts(self, value):
        assert isinstance(value, (bool, str))
        assert self._only_in_secure_contexts is False and value is not False
        self._only_in_secure_contexts = value
