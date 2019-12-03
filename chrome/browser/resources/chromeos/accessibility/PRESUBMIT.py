# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for accessibility component extensions."""

import sys


def CheckChangeOnUpload(input_api, output_api):
  paths = input_api.AbsoluteLocalPaths()

  def ShouldCheckFile(path):
    return path.endswith('.js')

  # Only care about changes to JS files.
  paths = [p for p in paths if ShouldCheckFile(p)]
  if not paths:
    return []

  if not sys.platform.startswith('linux'):
    return []
  sys.path.insert(
      0, input_api.os_path.join(input_api.PresubmitLocalPath(), 'common'))
  try:
    from check_style import CheckA11yStyle
  finally:
    sys.path.pop(0)
  success, output = CheckA11yStyle(paths)
  if not success:
    return [
        output_api.PresubmitError(
            'Accessibility component extension style is not observed',
            long_text=output)
    ]
