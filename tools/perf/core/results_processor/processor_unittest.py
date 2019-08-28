# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import posixpath
import re
import unittest

import mock

from core.results_processor import processor


# To easily mock module level symbols within the processor module.
def module(symbol):
  return 'core.results_processor.processor.' + symbol


class ProcessOptionsTestCase(unittest.TestCase):
  def setUp(self):
    # Mock os module within results_processor so path manipulations do not
    # depend on the file system of the test environment.
    mock_os = mock.patch(module('os')).start()

    def abspath(path):
      return posixpath.join(mock_os.getcwd(), path)

    def expanduser(path):
      return re.sub(r'~', '/path/to/home', path)

    mock_os.getcwd.return_value = '/path/to/curdir'
    mock_os.path.abspath.side_effect = abspath
    mock_os.path.expanduser.side_effect = expanduser
    mock_os.path.dirname.side_effect = posixpath.dirname
    mock_os.path.join.side_effect = posixpath.join

    telemetry_util = mock.patch(module('util')).start()
    telemetry_util.GetBaseDir.return_value = '/path/to/output_dir'

  def tearDown(self):
    mock.patch.stopall()

  def ParseArgs(self, args):
    parser = processor.ArgumentParser()
    options = parser.parse_args(args)
    processor.ProcessOptions(options)
    return options


class TestProcessOptions(ProcessOptionsTestCase):
  def testOutputDir_default(self):
    options = self.ParseArgs([])
    self.assertEqual(options.output_dir, '/path/to/output_dir')

  def testOutputDir_homeDir(self):
    options = self.ParseArgs(['--output-dir', '~/my_outputs'])
    self.assertEqual(options.output_dir, '/path/to/home/my_outputs')

  def testOutputDir_relPath(self):
    options = self.ParseArgs(['--output-dir', 'my_outputs'])
    self.assertEqual(options.output_dir, '/path/to/curdir/my_outputs')

  def testOutputDir_absPath(self):
    options = self.ParseArgs(['--output-dir', '/path/to/somewhere/else'])
    self.assertEqual(options.output_dir, '/path/to/somewhere/else')

  @mock.patch(module('datetime'))
  def testIntermediateDir_default(self, mock_datetime):
    mock_datetime.datetime.utcnow.return_value = (
        datetime.datetime(2015, 10, 21, 7, 28))
    options = self.ParseArgs(['--output-dir', '/output'])
    self.assertEqual(options.intermediate_dir,
                     '/output/artifacts/run_20151021T072800Z')

  @mock.patch(module('datetime'))
  def testIntermediateDir_withResultsLabel(self, mock_datetime):
    mock_datetime.datetime.utcnow.return_value = (
        datetime.datetime(2015, 10, 21, 7, 28))
    options = self.ParseArgs(
        ['--output-dir', '/output', '--results-label', 'test my feature'])
    self.assertEqual(options.intermediate_dir,
                     '/output/artifacts/test_my_feature_20151021T072800Z')

  def testUploadBucket_noUploadResults(self):
    options = self.ParseArgs([])
    self.assertFalse(options.upload_results)
    self.assertIsNone(options.upload_bucket)

  @mock.patch(module('cloud_storage'))
  def testUploadBucket_uploadResultsToDefaultBucket(self, mock_storage):
    mock_storage.BUCKET_ALIASES = {'output': 'default-bucket'}
    options = self.ParseArgs(['--upload-results'])
    self.assertTrue(options.upload_results)
    self.assertEqual(options.upload_bucket, 'default-bucket')

  @mock.patch(module('cloud_storage'))
  def testUploadBucket_uploadResultsToBucket(self, mock_storage):
    mock_storage.BUCKET_ALIASES = {'output': 'default-bucket'}
    options = self.ParseArgs(
        ['--upload-results', '--upload-bucket', 'my_bucket'])
    self.assertTrue(options.upload_results)
    self.assertEqual(options.upload_bucket, 'my_bucket')

  @mock.patch(module('cloud_storage'))
  def testUploadBucket_uploadResultsToAlias(self, mock_storage):
    mock_storage.BUCKET_ALIASES = {
        'output': 'default-bucket', 'special': 'some-special-bucket'}
    options = self.ParseArgs(
        ['--upload-results', '--upload-bucket', 'special'])
    self.assertTrue(options.upload_results)
    self.assertEqual(options.upload_bucket, 'some-special-bucket')

  def testDefaultOutputFormat(self):
    options = self.ParseArgs([])
    self.assertEqual(options.output_formats, [])
    self.assertEqual(options.legacy_output_formats, ['html'])

  def testUnkownOutputFormatRaises(self):
    with self.assertRaises(SystemExit):
      self.ParseArgs(['--output-format', 'unknown'])

  @mock.patch.dict(module('SUPPORTED_FORMATS'), {'new-format': None})
  @mock.patch(module('command_line'))
  def testOutputFormatsSplit(self, telemetry_cli):
    telemetry_cli.LEGACY_OUTPUT_FORMATS = ['old-format']
    options = self.ParseArgs(
        ['--output-format', 'new-format', '--output-format', 'old-format'])
    self.assertEqual(options.output_formats, ['new-format'])
    self.assertEqual(options.legacy_output_formats, ['old-format'])
