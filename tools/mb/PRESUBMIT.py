# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# TODO(crbug.com/1109980): Remove this once the production freeze is over, which
# is expected to be on August 3rd.
def EnforceProductionFreeze(input_api, output_api):
  footers = input_api.change.GitFootersFromDescription()
  if footers.get('Ignore-Cq-Freeze'):
    return []

  message = """
  Your change is modifying files which may impact the Chromium CQ. The Chromium
  CQ is currently in a production freeze. Please get a review from someone in
  the //infra/OWNERS file (preferably a trooper), and then add the
  'Ignore-CQ-Freeze' git footer to your CL. See https://crbug.com/1109980 for
  more details.
  """
  return [output_api.PresubmitError(message)]


def _CommonChecks(input_api, output_api):
  results = []

  # Run Pylint over the files in the directory.
  pylint_checks = input_api.canned_checks.GetPylint(input_api, output_api)
  results.extend(input_api.RunTests(pylint_checks))

  # Run the MB unittests.
  results.extend(input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', [ r'^.+_unittest\.py$']))

  # Validate the format of the mb_config.pyl file.
  cmd = [input_api.python_executable, 'mb.py', 'validate']
  kwargs = {'cwd': input_api.PresubmitLocalPath()}
  results.extend(input_api.RunTests([
      input_api.Command(name='mb_validate',
                        cmd=cmd, kwargs=kwargs,
                        message=output_api.PresubmitError)]))
  results.extend(EnforceProductionFreeze(input_api, output_api))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
