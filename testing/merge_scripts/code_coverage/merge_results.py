#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Merge results from code coverage swarming runs.

This script merges code coverage profiles from multiple shards. It also merges
the test results of the shards.

It is functionally similar to merge_steps.py but it accepts the parameters
passed by swarming api.
"""

import argparse
import json
import logging
import os
import subprocess
import sys

import merge_lib as coverage_merger


def _MergeAPIArgumentParser(*args, **kwargs):
  """Parameters passed to this merge script, as per:
  https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipe_modules/swarming/resources/merge_api.py
  """
  parser = argparse.ArgumentParser(*args, **kwargs)
  parser.add_argument('--build-properties', help=argparse.SUPPRESS)
  parser.add_argument('--summary-json', help=argparse.SUPPRESS)
  parser.add_argument('--task-output-dir', help=argparse.SUPPRESS)
  parser.add_argument(
      '-o', '--output-json', required=True, help=argparse.SUPPRESS)
  parser.add_argument('jsons_to_merge', nargs='*', help=argparse.SUPPRESS)

  # Custom arguments for this merge script.
  parser.add_argument(
      '--additional-merge-script', help='additional merge script to run')
  parser.add_argument(
      '--additional-merge-script-args',
      help='JSON serialized string of args for the additional merge script')
  parser.add_argument(
      '--profdata-dir', required=True, help='where to store the merged data')
  parser.add_argument(
      '--llvm-profdata', required=True, help='path to llvm-profdata executable')
  return parser


def main():
  desc = "Merge profraw files in <--task-output-dir> into a single profdata."
  parser = _MergeAPIArgumentParser(description=desc)
  params = parser.parse_args()

  # NOTE: The coverage data merge script must make sure that the profraw files
  # are deleted from the task output directory after merging, otherwise, other
  # test results merge script such as layout tests will treat them as json test
  # results files and result in errors.
  logging.info('Merging code coverage profraw data')
  invalid_profiles = coverage_merger.merge_profiles(
      params.task_output_dir,
      os.path.join(params.profdata_dir, 'default.profdata'), '.profraw',
      params.llvm_profdata)
  if invalid_profiles:
    with open(os.path.join(params.profdata_dir, 'invalid_profiles.json'),
              'w') as f:
      json.dump(invalid_profiles, f)

  logging.info('Merging %d test results', len(params.jsons_to_merge))
  failed = False

  # If given, always run the additional merge script, even if we only have one
  # output json. Merge scripts sometimes upload artifacts to cloud storage, or
  # do other processing which can be needed even if there's only one output.
  if params.additional_merge_script:
    new_args = [
        '--build-properties',
        params.build_properties,
        '--summary-json',
        params.summary_json,
        '--task-output-dir',
        params.task_output_dir,
        '--output-json',
        params.output_json,
    ]

    if params.additional_merge_script_args:
      new_args += json.loads(params.additional_merge_script_args)

    new_args += params.jsons_to_merge

    args = [sys.executable, params.additional_merge_script] + new_args
    rc = subprocess.call(args)
    if rc != 0:
      failed = True
      logging.warning('Additional merge script %s exited with %s' %
                      (params.additional_merge_script, rc))
    mark_invalid_shards(coverage_merger.get_shards_to_retry(invalid_profiles),
                        params.summary_json, params.output_json)
  elif len(params.jsons_to_merge) == 1:
    logging.info("Only one output needs to be merged; directly copying it.")
    with open(params.jsons_to_merge[0]) as f_read:
      with open(params.output_json, 'w') as f_write:
        f_write.write(f_read.read())
    mark_invalid_shards(coverage_merger.get_shards_to_retry(invalid_profiles),
                        params.summary_json, params.output_json)
  else:
    logging.warning(
        "This script was told to merge %d test results, but no additional "
        "merge script was given.")

  return 1 if (failed or bool(invalid_profiles)) else 0

def mark_invalid_shards(bad_shards, summary_file, output_file):
  shard_indices = []
  with open(summary_file) as f:
    summary = json.load(f)

  for i in range(len(summary['shards'])):
    shard_id = summary['shards'][i]['task_id']
    if shard_id in bad_shards:
      shard_indices.append(i)

  with open(output_file) as f:
    output = json.load(f)
  output.setdefault('missing_shards', [])
  output['missing_shards'].extend(shard_indices)
  with open(output_file, 'w') as f:
    json.dump(output, f)


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s %(levelname)s] %(message)s', level=logging.INFO)
  sys.exit(main())
