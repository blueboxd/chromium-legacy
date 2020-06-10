# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""TestResultSink uploads test results and artifacts to ResultDB via ResultSink.

ResultSink is a micro service that simplifies integration between ResultDB
and domain-specific test frameworks. It runs a given test framework and uploads
all the generated test results and artifacts to ResultDB in a progressive way.
- APIs: https://godoc.org/go.chromium.org/luci/resultdb/proto/sink/v1

TestResultSink implements methods for uploading test results and artifacts
via ResultSink, and is activated only if LUCI_CONTEXT is present with ResultSink
section.
"""

import json
import logging
import os
import urllib2

_log = logging.getLogger(__name__)


def CreateTestResultSink(artifacts_directory):
    """Creates TestResultSink, if result_sink is present in LUCI_CONTEXT.

    Args:
        artifacts_directory: the path of the directory, where test artifacts
            are stored.
    Returns:
        TestResultSink object if result_sink section is present in LUCI_CONTEXT.
            None, otherwise.
    """
    luci_ctx_path = os.environ.get('LUCI_CONTEXT', None)
    if luci_ctx_path is None:
        return None

    with open(luci_ctx_path, 'r') as f:
        sink_ctx = json.load(f).get('result_sink', None)
    if sink_ctx is None:
        return None

    return TestResultSink(sink_ctx, artifacts_directory)


class TestResultSink(object):
    """A class for uploading test results and artifacts via ResultSink."""

    def __init__(self, sink_ctx, artifacts_directory):
        self._sink_ctx = sink_ctx
        self._artifacts_directory = artifacts_directory
        self._sink_url = (
            'http://%s/prpc/luci.resultdb.sink.v1.Sink/ReportTestResults' %
            self._sink_ctx['address'])

    def _send(self, data):
        req = urllib2.Request(
            url=self._sink_url,
            data=json.dumps(data),
            headers={
                'Content-Type': 'application/json',
                'Accept': 'application/json',
                'Authorization':
                'ResultSink %s' % self._sink_ctx['auth_token'],
            },
        )
        return urllib2.urlopen(req)

    def sink(self, expected, test_result):
        # TODO(crbug/1084459): implement sink
        pass
