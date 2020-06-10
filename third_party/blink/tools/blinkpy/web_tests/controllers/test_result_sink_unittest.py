# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import json
import sys
import tempfile
import unittest

from blinkpy.web_tests.controllers.test_result_sink import CreateTestResultSink
from blinkpy.web_tests.models import test_results


@contextlib.contextmanager
def luci_context(**section_values):
    if not section_values:
        yield
        return

    tf = tempfile.NamedTemporaryFile(delete=False)
    old_envvar = os.environ.get('LUCI_CONTEXT', None)
    try:
        json.dump(section_values, tf)
        tf.close()
        os.environ['LUCI_CONTEXT'] = tf.name
        yield tf.name
    finally:
        os.unlink(tf.name)
        if old_envvar is None:
            del os.environ['LUCI_CONTEXT']
        else:
            os.environ['LUCI_CONTEXT'] = old_envvar


class TestCreateTestResultSink(unittest.TestCase):
    def test_without_luci_context(self):
        self.assertIsNone(CreateTestResultSink('/tmp/artifacts'))

    def test_without_result_sink_section(self):
        with luci_context(app={'foo': 'bar'}):
            self.assertIsNone(CreateTestResultSink('/tmp/artifacts'))

    def test_with_result_sink_section(self):
        with luci_context(result_sink={
                'address': 'localhost:123',
                'auth_token': 'super secret password'
        }):
            self.assertIsNotNone(CreateTestResultSink('/tmp/artifacts'))
