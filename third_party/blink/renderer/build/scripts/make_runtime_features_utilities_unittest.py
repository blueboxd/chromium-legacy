# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import make_runtime_features_utilities as util
from blinkbuild.name_style_converter import NameStyleConverter


class MakeRuntimeFeaturesUtilitiesTest(unittest.TestCase):
    def test_cycle_in_dependency(self):
        # Cycle: 'c' => 'd' => 'e' => 'c'
        graph = {
            'a': ['b'],
            'b': [],
            'c': ['a', 'd'],
            'd': ['e'],
            'e': ['c']
        }
        with self.assertRaises(AssertionError):
            util.origin_trials([{'name': name, 'depends_on': deps} for name, deps in graph.items()])

    def test_bad_dependency(self):
        with self.assertRaises(AssertionError):
            util.origin_trials([{'name': 'a', 'depends_on': 'x'}])

    def test_in_origin_trials_flag(self):
        features = [
            {'name': NameStyleConverter('a'), 'depends_on': [], 'origin_trial_feature_name': None},
            {'name': NameStyleConverter('b'), 'depends_on': ['a'], 'origin_trial_feature_name': 'OriginTrials'},
            {'name': NameStyleConverter('c'), 'depends_on': ['b'], 'origin_trial_feature_name': None},
            {'name': NameStyleConverter('d'), 'depends_on': ['b'], 'origin_trial_feature_name': None},
            {'name': NameStyleConverter('e'), 'depends_on': ['d'], 'origin_trial_feature_name': None},
        ]
        self.assertSetEqual(util.origin_trials(features), {'b', 'c', 'd', 'e'})

        features = [{
            'name': 'a',
            'depends_on': ['x'],
            'origin_trial_feature_name': None
        }, {
            'name': 'b',
            'depends_on': ['x', 'y'],
            'origin_trial_feature_name': None
        }, {
            'name': 'c',
            'depends_on': ['y', 'z'],
            'origin_trial_feature_name': None
        }, {
            'name': 'x',
            'depends_on': [],
            'origin_trial_feature_name': None
        }, {
            'name': 'y',
            'depends_on': ['x'],
            'origin_trial_feature_name': 'y'
        }, {
            'name': 'z',
            'depends_on': ['y'],
            'origin_trial_feature_name': None
        }]

        self.assertSetEqual(util.origin_trials(features), {'b', 'c', 'y', 'z'})


if __name__ == "__main__":
    unittest.main()
