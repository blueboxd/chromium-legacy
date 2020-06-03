# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil

import common

BLINK_TOOLS_DIR = os.path.join(common.SRC_DIR, 'third_party', 'blink', 'tools')
WEB_TESTS_DIR = os.path.join(BLINK_TOOLS_DIR, os.pardir, 'web_tests')

class BaseWptScriptAdapter(common.BaseIsolatedScriptArgsAdapter):
    """The base class for script adapters that use wptrunner to execute web
    platform tests. This contains any code shared between these scripts, such
    as integrating output with the results viewer. Subclasses contain other
    (usually platform-specific) logic."""

    def generate_test_output_args(self, output):
        return ['--log-chromium', output]

    def generate_sharding_args(self, total_shards, shard_index):
        return ['--total-chunks=%d' % total_shards,
                # shard_index is 0-based but WPT's this-chunk to be 1-based
                '--this-chunk=%d' % (shard_index + 1)]

    def do_post_test_run_tasks(self):
        # Move json results into layout-test-results directory
        results_dir = os.path.dirname(self.options.isolated_script_test_output)
        layout_test_results = os.path.join(results_dir, 'layout-test-results')
        if os.path.exists(layout_test_results):
            shutil.rmtree(layout_test_results)
        os.mkdir(layout_test_results)
        shutil.copyfile(self.options.isolated_script_test_output,
                        os.path.join(layout_test_results, 'full_results.json'))
        # create full_results_jsonp.js file which is used to
        # load results into the results viewer
        with open(self.options.isolated_script_test_output, 'r') \
            as full_results, \
            open(os.path.join(
                layout_test_results, 'full_results_jsonp.js'), 'w') \
                as json_js:
            json_js.write('ADD_FULL_RESULTS(%s);' % full_results.read())
        # copy layout test results viewer to layout-test-results directory
        shutil.copyfile(
            os.path.join(WEB_TESTS_DIR, 'fast', 'harness', 'results.html'),
            os.path.join(layout_test_results, 'results.html'))