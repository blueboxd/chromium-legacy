# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces luci-milo.cfg consistency.

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


def _CommonChecks(input_api, output_api):
  commands = []

  if ('infra/config/generated/luci-milo.cfg' in input_api.LocalPaths() or
      'infra/config/lint-luci-milo.py' in input_api.LocalPaths()):
    commands.append(
      input_api.Command(
          name='lint-luci-milo',
          cmd=[input_api.python_executable, 'lint-luci-milo.py'],
          kwargs={},
          message=output_api.PresubmitError))
  if ('infra/config/generated/luci-milo.cfg' in input_api.LocalPaths() or
      'infra/config/generated/luci-milo-dev.cfg' in input_api.LocalPaths()):
    commands.append(
      input_api.Command(
        name='testing/buildbot config checks',
        cmd=[input_api.python_executable, input_api.os_path.join(
                '..', '..', 'testing', 'buildbot',
                'generate_buildbot_json.py',),
            '--check'],
        kwargs={}, message=output_api.PresubmitError))

  commands.extend(input_api.canned_checks.CheckLucicfgGenOutput(
      input_api, output_api, 'main.star'))
  commands.extend(input_api.canned_checks.CheckLucicfgGenOutput(
      input_api, output_api, 'dev.star'))

  results = []

  results.extend(input_api.RunTests(commands))
  results.extend(input_api.canned_checks.CheckChangedLUCIConfigs(
      input_api, output_api))
  results.extend(EnforceProductionFreeze(input_api, output_api))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
