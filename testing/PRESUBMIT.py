# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for testing.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
  output = []
  blacklist = [r'gmock.*', r'gtest.*']
  output.extend(input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', [r'^.+_unittest\.py$']))
  skia_gold_env = dict(input_api.environ)
  skia_gold_env.update({
      'PYTHONPATH': input_api.PresubmitLocalPath(),
      'PYTHONDONTWRITEBYTECODE': '1',
  })
  output.extend(input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, 'skia_gold_common', [r'^.+_unittest\.py$'],
      env=skia_gold_env))
  output.extend(input_api.canned_checks.RunPylint(
      input_api, output_api, black_list=blacklist))
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
