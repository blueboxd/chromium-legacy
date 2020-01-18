# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander
from collections import defaultdict
from make_runtime_features_utilities import origin_trials


class FeaturePolicyFeatureWriter(json5_generator.Writer):
    file_basename = 'feature_policy_helper'

    def __init__(self, json5_file_path, output_dir):
        super(FeaturePolicyFeatureWriter, self).__init__(json5_file_path, output_dir)
        self._outputs = {
            (self.file_basename + '.cc'): self.generate_implementation,
        }

        self._features = self.json5_file.name_dictionaries
        # Set runtime and feature policy features
        self._runtime_features = []
        self._feature_policy_features = []
        for feature in self._features:
            if feature['feature_policy_name']:
                self._feature_policy_features.append(feature)
            else:
                self._runtime_features.append(feature)

        origin_trials_set = origin_trials(self._runtime_features)

        self._origin_trial_dependency_map = defaultdict(list)
        self._runtime_to_feature_policy_map = defaultdict(list)
        for feature in self._feature_policy_features:
            for dependency in feature['depends_on']:
                if str(dependency) in origin_trials_set:
                    self._origin_trial_dependency_map[feature['name']].append(dependency)
                else:
                    self._runtime_to_feature_policy_map[dependency].append(feature['name'])

        self._header_guard = self.make_header_guard(self._relative_output_dir + self.file_basename + '.h')

    def _template_inputs(self):
        return {
            'feature_policy_features': self._feature_policy_features,
            'header_guard': self._header_guard,
            'input_files': self._input_files,
            'runtime_features': self._runtime_features,
            'runtime_to_feature_policy_map': self._runtime_to_feature_policy_map,
            'origin_trial_dependency_map': self._origin_trial_dependency_map,
        }

    @template_expander.use_jinja('templates/' + file_basename + '.cc.tmpl')
    def generate_implementation(self):
        return self._template_inputs()


if __name__ == '__main__':
    json5_generator.Maker(FeaturePolicyFeatureWriter).main()
