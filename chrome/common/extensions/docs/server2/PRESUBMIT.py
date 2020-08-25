# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting extensions docs server

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os

def _WarnIfAppYamlHasntChanged(input_api, output_api):
  app_yaml_path = os.path.join(input_api.PresubmitLocalPath(), 'app.yaml')
  if app_yaml_path in input_api.AbsoluteLocalPaths():
    return []
  return [output_api.PresubmitPromptOrNotify('''
**************************************************
CHANGE DETECTED IN SERVER2 WITHOUT APP.YAML UPDATE
**************************************************
Maybe this is ok? Follow this simple guide:

Q: Does this change any data that might get stored?
  * Did you add/remove/update a field to a data source?
  * Did you add/remove/update some data that gets sent to templates?
  * Is this change to support a new feature in the templates?
  * Does this change include changes to templates?
Yes? Bump the middle version and zero out the end version, i.e. 2-5-2 -> 2-6-0.
     THIS WILL CAUSE THE CURRENTLY RUNNING SERVER TO STOP UPDATING.
     PUSH THE NEW VERSION ASAP.
No? Continue.
Q: Is this a non-trivial change to the server?
Yes? Bump the end version.
     Unlike above, the server will *not* stop updating.
No? Are you sure? How much do you bet? This can't be rolled back...

Q: Is this a spelling correction? New test? Better comments?
Yes? Ok fine. Ignore this warning.
No? I guess this presubmit check doesn't work.
''')]

def _RunPresubmit(input_api, output_api):
  # TODO(crbug.com/434363): Enable pylint for the docserver.
  return _WarnIfAppYamlHasntChanged(input_api, output_api)

def CheckChangeOnUpload(input_api, output_api):
  return _RunPresubmit(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _RunPresubmit(input_api, output_api)
