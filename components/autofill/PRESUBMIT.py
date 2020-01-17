# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/components/autofill.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

def _CheckNoBaseTimeCalls(input_api, output_api):
  """Checks that no files call base::Time::Now() or base::TimeTicks::Now()."""
  pattern = input_api.re.compile(
      r'(base::(Time|TimeTicks)::Now)\(\)',
      input_api.re.MULTILINE)
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (f.LocalPath().startswith('components/autofill/') and
        not f.LocalPath().endswith("PRESUBMIT.py")):
      contents = input_api.ReadFile(f)
      if pattern.search(contents):
        files.append(f)

  if len(files):
    return [ output_api.PresubmitPromptWarning(
        'Consider to not call base::Time::Now() or base::TimeTicks::Now() ' +
        'directly but use AutofillClock::Now() and '+
        'Autofill::TickClock::NowTicks(), respectively. These clocks can be ' +
        'manipulated through TestAutofillClock and TestAutofillTickClock '+
        'for testing purposes, and using AutofillClock and AutofillTickClock '+
        'throughout Autofill code makes sure Autofill tests refers to the '+
        'same (potentially manipulated) clock.',
        files) ]
  return []


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(_CheckNoBaseTimeCalls(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results
