# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def RunOtherPresubmit(function_name, input_api, output_api):
  # Apply the PRESUBMIT for components/policy/resources to run the syntax check.
  presubmit_path = (
      input_api.change.RepositoryRoot() + \
          '/components/policy/resources/PRESUBMIT.py')
  presubmit_content = input_api.ReadFile(presubmit_path)
  global_vars = {}
  exec (presubmit_content, global_vars)
  return global_vars[function_name](input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
  return RunOtherPresubmit("CheckChangeOnUpload", input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return RunOtherPresubmit("CheckChangeOnCommit", input_api, output_api)
