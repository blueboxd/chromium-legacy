# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.gerrit_mock import MockGerritAPI, MockGerritCL
from blinkpy.w3c.export_notifier import ExportNotifier, PRStatusInfo
from blinkpy.w3c.wpt_github import PullRequest
from blinkpy.w3c.wpt_github_mock import MockWPTGitHub


class ExportNotifierTest(LoggingTestCase):
    def setUp(self):
        super(ExportNotifierTest, self).setUp()
        self.host = MockHost()
        self.git = self.host.git()
        self.gerrit = MockGerritAPI()
        self.notifier = ExportNotifier(self.host, self.git, self.gerrit)

    def test_get_gerrit_sha_from_comment_success(self):
        gerrit_comment = self.generate_notifier_comment(
            123, 'bar', 'num', None)

        actual = PRStatusInfo.get_gerrit_sha_from_comment(gerrit_comment)

        self.assertEqual(actual, 'num')

    def test_get_gerrit_sha_from_comment_fail(self):
        gerrit_comment = 'ABC'

        actual = PRStatusInfo.get_gerrit_sha_from_comment(gerrit_comment)

        self.assertIsNone(actual)

    def test_to_gerrit_comment(self):
        pr_status_info = PRStatusInfo('bar', 123, 'num')
        expected = self.generate_notifier_comment(123, 'bar', 'num', None)

        actual = pr_status_info.to_gerrit_comment()

        self.assertEqual(expected, actual)

    def test_to_gerrit_comment_latest(self):
        pr_status_info = PRStatusInfo('bar', 123, None)
        expected = self.generate_notifier_comment(123, 'bar', 'Latest', None)

        actual = pr_status_info.to_gerrit_comment()

        self.assertEqual(expected, actual)

    def test_to_gerrit_comment_with_patchset(self):
        pr_status_info = PRStatusInfo('bar', 123, 'num')
        expected = self.generate_notifier_comment(123, 'bar', 'num', 3)

        actual = pr_status_info.to_gerrit_comment(3)

        self.assertEqual(expected, actual)

    def test_get_failure_taskcluster_status_success(self):
        taskcluster_status = [{
            "state": "failure",
            "context": "Community-TC (pull_request)",
        }, {
            "state": "success",
            "context": "random",
        }]

        self.assertEqual(
            self.notifier.get_failure_taskcluster_status(
                taskcluster_status, 123), {
                    "state": "failure",
                    "context": "Community-TC (pull_request)",
            })

    def test_get_failure_taskcluster_status_fail(self):
        taskcluster_status = [
            {
                "state": "success",
                "context": "Community-TC (pull_request)",
            },
        ]

        self.assertEqual(
            self.notifier.get_failure_taskcluster_status(
                taskcluster_status, 123), None)

    def test_has_latest_taskcluster_status_commented_false(self):
        pr_status_info = PRStatusInfo('bar', 123, 'num')
        messages = [{
            "date": "2019-08-20 17:42:05.000000000",
            "message": "Uploaded patch set 1.\nInitial upload",
            "_revision_number": 1
        }]

        actual = self.notifier.has_latest_taskcluster_status_commented(
            messages, pr_status_info)

        self.assertFalse(actual)

    def test_has_latest_taskcluster_status_commented_true(self):
        pr_status_info = PRStatusInfo('bar', 123, 'num')
        messages = [
            {
                "date": "2019-08-20 17:42:05.000000000",
                "message": "Uploaded patch set 1.\nInitial upload",
                "_revision_number": 1
            },
            {
                "date":
                "2019-08-21 17:41:05.000000000",
                "message": self.generate_notifier_comment(123, 'bar', 'num', 3),
                "_revision_number":
                2
            },
        ]

        actual = self.notifier.has_latest_taskcluster_status_commented(
            messages, pr_status_info)

        self.assertTrue(actual)

        self.assertTrue(actual)

    def test_get_taskcluster_statuses_success(self):
        self.notifier.wpt_github = MockWPTGitHub(pull_requests=[
            PullRequest(
                title='title1',
                number=1234,
                body='description\nWPT-Export-Revision: 1',
                state='open',
                labels=[]),
        ])
        status = [{"description": "foo"}]
        self.notifier.wpt_github.status = status
        actual = self.notifier.get_taskcluster_statuses(123)

        self.assertEqual(actual, status)
        self.assertEqual(self.notifier.wpt_github.calls, [
            'get_pr_branch',
            'get_branch_statuses',
        ])

    def test_process_failing_prs_success(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id':
                'abc',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date":
                        "2019-08-21 17:41:05.000000000",
                        "message": self.generate_notifier_comment(123, 'notbar', 'notnum', 3),
                        "_revision_number":
                        2
                    },
                ],
                'revisions': {
                    'num': {
                        '_number': 1
                    }
                }
            },
            api=self.notifier.gerrit)
        gerrit_dict = {'abc': PRStatusInfo('bar', 123, 'num')}
        expected = self.generate_notifier_comment(123, 'bar', 'num', 1)

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted,
                         [('/a/changes/abc/revisions/current/review', {
                             'message': expected
                         })])

    def test_process_failing_prs_has_commented(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id':
                'abc',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date":
                        "2019-08-21 17:41:05.000000000",
                        "message": self.generate_notifier_comment(123, 'bar', 'num', 3),
                        "_revision_number":
                        2
                    },
                ],
                'revisions': {
                    'num': {
                        '_number': 1
                    }
                }
            },
            api=self.notifier.gerrit)
        gerrit_dict = {'abc': PRStatusInfo('bar', 123, 'num')}

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted, [])

    def test_process_failing_prs_with_latest_sha(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id':
                'abc',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date":
                        "2019-08-21 17:41:05.000000000",
                        "message": self.generate_notifier_comment(123, 'notbar', 'notnum', 3),
                        "_revision_number":
                        2
                    },
                ],
                'revisions': {
                    'num': {
                        '_number': 1
                    }
                }
            },
            api=self.notifier.gerrit)
        expected = self.generate_notifier_comment(123, 'bar', 'Latest')
        gerrit_dict = {'abc': PRStatusInfo('bar', 123, None)}

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted,
                         [('/a/changes/abc/revisions/current/review', {
                             'message': expected
                         })])

    def test_process_failing_prs_raise_gerrit_error(self):
        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI(raise_error=True)
        gerrit_dict = {'abc': PRStatusInfo('bar', 'num')}

        self.notifier.process_failing_prs(gerrit_dict)

        self.assertEqual(self.notifier.gerrit.cls_queried, ['abc'])
        self.assertEqual(self.notifier.gerrit.request_posted, [])
        self.assertLog([
            'INFO: Processing 1 CLs with failed Taskcluster status.\n',
            'ERROR: Could not process Gerrit CL abc: Error from query_cl\n'
        ])

    def test_export_notifier_success(self):
        self.notifier.wpt_github = MockWPTGitHub(pull_requests=[])
        self.notifier.wpt_github.recent_failing_pull_requests = [
            PullRequest(
                title='title1',
                number=1234,
                body='description\nWPT-Export-Revision: hash\nChange-Id: decafbad',
                state='open',
                labels=[''])
        ]
        status = [{
            "state": "failure",
            "context": "Community-TC (pull_request)",
            "node_id": "foo",
            "target_url": "bar"
        }]
        self.notifier.wpt_github.status = status

        self.notifier.dry_run = False
        self.notifier.gerrit = MockGerritAPI()
        self.notifier.gerrit.cl = MockGerritCL(
            data={
                'change_id':
                'decafbad',
                'messages': [
                    {
                        "date": "2019-08-20 17:42:05.000000000",
                        "message": "Uploaded patch set 1.\nInitial upload",
                        "_revision_number": 1
                    },
                    {
                        "date":
                        "2019-08-21 17:41:05.000000000",
                        "message": self.generate_notifier_comment(1234, 'notbar', 'notnum', 3),
                        "_revision_number":
                        2
                    },
                ],
                'revisions': {
                    'hash': {
                        '_number': 2
                    }
                }
            },
            api=self.notifier.gerrit)
        expected = self.generate_notifier_comment(1234, 'bar', 'hash', 2)

        exit_code = self.notifier.main()

        self.assertFalse(exit_code)
        self.assertEqual(self.notifier.wpt_github.calls, [
            'recent_failing_chromium_exports',
            'get_pr_branch',
            'get_branch_statuses',
        ])
        self.assertEqual(self.notifier.gerrit.cls_queried, ['decafbad'])
        self.assertEqual(self.notifier.gerrit.request_posted,
                         [('/a/changes/decafbad/revisions/current/review', {
                             'message': expected
                         })])

    def generate_notifier_comment(self, pr_number, link, sha, patchset=None):
        if patchset:
            comment = (
                'The exported PR, https://github.com/web-platform-tests/wpt/pull/{}, '
                'has failed Taskcluster check(s) on GitHub, which could indicate '
                'cross-browser failures on the exported changes. Please contact '
                'ecosystem-infra@chromium.org for more information.\n\n'
                'Taskcluster Link: {}\n'
                'Gerrit CL SHA: {}\n'
                'Patchset Number: {}'
                '\n\nAny suggestions to improve this service are welcome; '
                'crbug.com/1027618.').format(
                pr_number, link, sha, patchset
            )
        else:
            comment = (
                'The exported PR, https://github.com/web-platform-tests/wpt/pull/{}, '
                'has failed Taskcluster check(s) on GitHub, which could indicate '
                'cross-browser failures on the exported changes. Please contact '
                'ecosystem-infra@chromium.org for more information.\n\n'
                'Taskcluster Link: {}\n'
                'Gerrit CL SHA: {}'
                '\n\nAny suggestions to improve this service are welcome; '
                'crbug.com/1027618.').format(
                pr_number, link, sha
            )
        return comment
