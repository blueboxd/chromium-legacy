#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
 Convert the ASCII safety_tips.asciipb proto into a binary resource.
"""

import os
import sys

# Disable warnings for "safety_tips_pb2" which is dynamically imported.
# pylint: disable=undefined-variable

# Subdirectory to be copied to Google Cloud Storage. Contains a copy of the
# generated proto under a versioned directory.
# TODO(meacer): Remove this. Safety tips does not read the proto from a local
# resource bundle, it only uses the proto passed from component updater. It does
# not need two copies of the file.
GS_COPY_DIR = "gs_copy"

# Import the binary proto generator. Walks up to the root of the source tree
# which is five directories above, and finds the protobufs directory from there.
proto_generator_path = os.path.normpath(os.path.join(os.path.abspath(__file__),
    *[os.path.pardir] * 5 + ['chrome/browser/resources/protobufs']))
sys.path.insert(0, proto_generator_path)
from binary_proto_generator import BinaryProtoGenerator

def MakeSubDirs(outfile):
  """ Make the subdirectories needed to create file |outfile| """
  dirname = os.path.dirname(outfile)
  if not os.path.exists(dirname):
    os.makedirs(dirname)

class SafetyTipsProtoGenerator(BinaryProtoGenerator):
  def ImportProtoModule(self):
    import safety_tips_pb2
    globals()['safety_tips_pb2'] = safety_tips_pb2

  def EmptyProtoInstance(self):
    return safety_tips_pb2.SafetyTipsConfig()

  def ValidatePb(self, opts, pb):
    assert pb.version_id > 0

    for flagged_page in pb.flagged_page:
      assert flagged_page.pattern
      assert flagged_page.type != safety_tips_pb2.FlaggedPage.UNKNOWN

    for allowed_pattern in pb.allowed_pattern:
      assert allowed_pattern.pattern

    flagged_patterns = [p.pattern for p in pb.flagged_page]
    assert sorted(flagged_patterns) == flagged_patterns, (
        "Please sort flagged_page entries by pattern.")

    allowed_patterns = [p.pattern for p in pb.allowed_pattern]
    assert sorted(allowed_patterns) == allowed_patterns, (
        "Please sort allowed_pattern entries by pattern.")

  def ProcessPb(self, opts, pb):
    binary_pb_str = pb.SerializeToString()
    outfile = os.path.join(opts.outdir, opts.outbasename)

    # Write two copies of the proto:
    # 1. Under the root of the gen directory for .grd files to refer to
    #    (./safety_tips/safety_tips.pb)
    # 2. Under a versioned directory for the proto pusher to refer to
    #    (./safety_tips/gs_copy/<version>/all/safety_tips.pb)
    outfile = os.path.join(opts.outdir, opts.outbasename)
    with open(outfile, 'wb') as f:
      f.write(binary_pb_str)

    outfile_copy = os.path.join(opts.outdir, GS_COPY_DIR, str(pb.version_id),
                                "all", opts.outbasename)
    MakeSubDirs(outfile_copy)
    with open(outfile_copy, 'wb') as f:
      f.write(binary_pb_str)


def main():
  return SafetyTipsProtoGenerator().Run()

if __name__ == '__main__':
  sys.exit(main())
