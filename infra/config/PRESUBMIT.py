# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces luci-milo.cfg consistency.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def _CommonChecks(input_api, output_api):
  commands = []

  if ('infra/config/luci-milo.cfg' in input_api.LocalPaths() or
      'infra/config/lint-luci-milo.py' in input_api.LocalPaths()):
    commands.append(
      input_api.Command(
          name='lint-luci-milo',
          cmd=[input_api.python_executable, 'lint-luci-milo.py'],
          kwargs={},
          message=output_api.PresubmitError))
    commands.append(
      # Technically doesn't rely on lint-luci-milo.py, but is a lightweight
      # enough check it should be fine to trigger.
      input_api.Command(
        name='testing/buildbot config checks',
        cmd=[input_api.python_executable, input_api.os_path.join(
                '..', '..', 'testing', 'buildbot',
                'generate_buildbot_json.py',),
            '--check'],
        kwargs={}, message=output_api.PresubmitError))

  if 'infra/config/commit-queue.cfg' in input_api.LocalPaths():
    commands.append(
      input_api.Command(
        name='commit-queue.cfg presubmit', cmd=[
            input_api.python_executable, input_api.os_path.join(
                'cq_cfg_presubmit.py'),
            '--check'],
        kwargs={}, message=output_api.PresubmitError),
    )

  commands.extend(input_api.canned_checks.CheckLucicfgGenOutput(
      input_api, output_api, 'main.star'))
  commands.extend(input_api.canned_checks.CheckLucicfgGenOutput(
      input_api, output_api, 'dev.star'))

  commands.extend(input_api.canned_checks.GetUnitTestsRecursively(
      input_api, output_api,
      input_api.os_path.join(input_api.PresubmitLocalPath()),
      whitelist=[r'.+_unittest\.py$'], blacklist=[]))

  results = []

  results.extend(input_api.RunTests(commands))
  results.extend(input_api.canned_checks.CheckChangedLUCIConfigs(
      input_api, output_api))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
