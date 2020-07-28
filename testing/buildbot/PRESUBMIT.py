# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces json format.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


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


def CommonChecks(input_api, output_api):
  commands = [
    input_api.Command(
      name='generate_buildbot_json', cmd=[
        input_api.python_executable, 'generate_buildbot_json.py', '--check',
        '--verbose'],
      kwargs={}, message=output_api.PresubmitError),

    input_api.Command(
      name='generate_buildbot_json_unittest', cmd=[
        input_api.python_executable, 'generate_buildbot_json_unittest.py'],
      kwargs={}, message=output_api.PresubmitError),

    input_api.Command(
      name='generate_buildbot_json_coveragetest', cmd=[
        input_api.python_executable, 'generate_buildbot_json_coveragetest.py'],
      kwargs={}, message=output_api.PresubmitError),

    input_api.Command(
      name='buildbot_json_magic_substitutions_unittest', cmd=[
        input_api.python_executable,
        'buildbot_json_magic_substitutions_unittest.py',
      ], kwargs={}, message=output_api.PresubmitError
    ),

    input_api.Command(
      name='manage', cmd=[
        input_api.python_executable, 'manage.py', '--check'],
      kwargs={}, message=output_api.PresubmitError),
  ]
  messages = []

  messages.extend(input_api.RunTests(commands))
  messages.extend(EnforceProductionFreeze(input_api, output_api))
  return messages


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
