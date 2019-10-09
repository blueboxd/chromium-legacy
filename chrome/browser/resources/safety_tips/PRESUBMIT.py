# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit checks for Safety Tips proto for the component updater.
"""

"""Returns true if any line in changed_contents contains the string |s|.
|changed_contents| is a tuple containing (line number, line text) pairs.
"""
def ContainsLine(changed_contents, s):
  for _, line in changed_contents:
    if line.strip().startswith(s):
      return True
  return False

def CheckVersionUpdatedInProto(input_api, output_api):
  def IsSafetyTipProto(x):
    return (input_api.os_path.basename(x.LocalPath()) ==
            'safety_tips.asciipb')

  safety_tips_proto = input_api.AffectedFiles(file_filter=IsSafetyTipProto)
  if not safety_tips_proto:
    return []

  contents = safety_tips_proto[0].ChangedContents()
  # Must not have any changes containing flagged_page or allowed_pattern:
  if (ContainsLine(contents, 'flagged_page:') or
      ContainsLine(contents, 'allowed_pattern:')):
      return [output_api.PresubmitError(
      'Do not check in the full safety_tips.asciipb proto. '
      'Only increment |version_id|.')]

  # It's enticing to do something fancy like checking whether the ID was in fact
  # incremented or whether this is a whitespace-only or comment-only change.
  # However, currently deleted lines don't show up in ChangedContents() and
  # attempting to parse the asciipb file any more than we are doing above is
  # likely not worth the trouble.
  #
  # At worst, the submitter can skip the presubmit check on upload if it isn't
  # correct.
  if not ContainsLine(contents, 'version_id:'):
    return [output_api.PresubmitError(
        'Increment |version_id| in safety_tips.asciipb if you are '
        'updating the file types proto.')]

  return []


def CheckChangeOnUpload(input_api, output_api):
  return CheckVersionUpdatedInProto(input_api, output_api)
