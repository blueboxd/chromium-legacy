# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import sys


def _CommonChecks(input_api, output_api):
  results = []
  # TODO(tkent): {kv-storage,toast,virtual-scroller}/.eslintrc.js specify babel-eslint parser, which
  # is not in third_party/node/node_modules/.
  mjs_files = input_api.AffectedFiles(
      file_filter=lambda f: (f.LocalPath().endswith('.mjs') and
                             not '/virtual-scroller/' in f.LocalPath() and
                             not '/kv-storage/' in f.LocalPath() and
                             not '/toast/' in f.LocalPath()),
      include_deletes=False)
  if not mjs_files:
    return results
  try:
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', '..', '..',
                                        '..', '..', 'tools')]
    import web_dev_style.js_checker
    checker = web_dev_style.js_checker.JSChecker(input_api, output_api)
    results += checker.RunEsLintChecks(mjs_files)
  finally:
    sys.path = old_sys_path
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
