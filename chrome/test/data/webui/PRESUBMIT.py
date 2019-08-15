# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _CheckForModularizedTests(input_api, output_api):
  module_tests = [
      'cr_elements/cr_button_tests.m.js',
      'cr_elements/cr_checkbox_test.m.js',
      'cr_elements/cr_input_test.m.js',
      'cr_elements/cr_toast_test.m.js',
      'cr_elements/cr_toggle_test.m.js',
      'cr_elements/cr_view_manager_test.m.js',
  ]
  affected_files = [
      input_api.os_path.relpath(f, input_api.PresubmitLocalPath())
      for f in input_api.AbsoluteLocalPaths()
  ]

  errors = []
  for module_test in module_tests:
    test = module_test[:-5] + '.js'
    if ((test in affected_files) != (module_test in affected_files)):
      errors.append('original: %s, modularized: %s' % (test, module_test))
  if errors:
    msg = 'Found modified test files with corresponding module files not ' + \
        'updated, or vice versa. Please ensure module and original test ' + \
        'files are updated in parallel, so that behavior is verified for ' + \
        'both modularized and non-modularized code.\n  ' + '\n  '.join(errors)
    return [output_api.PresubmitPromptWarning(msg)]
  else:
    return []

def _CommonChecks(input_api, output_api):
  results = []
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', '..', 'tools')]
    import web_dev_style.presubmit_support
    results += web_dev_style.presubmit_support.CheckStyleESLint(
        input_api, output_api)
  finally:
    sys.path = old_sys_path
  results += _CheckForModularizedTests(input_api, output_api)
  results += input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                         check_js=True)
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
