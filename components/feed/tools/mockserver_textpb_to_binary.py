#!/usr/bin/python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Lint as: python3
"""The tool converts a textpb into a binary proto using chromium protoc binary.

After converting a feed response textpb file into a mockserver textpb file using
the proto_convertor script, then a engineer runs this script to encode the
mockserver textpb file into a binary proto file that is being used by the feed
card render test (Refers to go/create-a-feed-card-render-test for more).

Make sure you have absl-py installed via pip install absl-py.

Usage example:
    python ./mockserver_textpb_to_binary.py
    --chromium_path ~/chromium/src
    --output_file /tmp/binary.pb
    --source_file ~/tmp/original.textpb
    --alsologtostderr
"""

import glob
import os
import subprocess

from absl import app
from absl import flags

FLAGS = flags.FLAGS
FLAGS = flags.FLAGS
flags.DEFINE_string('chromium_path', '', 'The path of your chromium depot.')
flags.DEFINE_string('output_file', '', 'The target output binary file path.')
flags.DEFINE_string('source_file', '',
                    'The source proto file, in textpb format, path.')

CMD_TEMPLATE = r'cat {} | {} --encode={} -I{} -I$(find {} -name "*.proto") > {}'
ENCODE_NAMESPACE = 'components.feed.core.proto.wire.mockserver.MockServer'
COMPONENT_FEED_PROTO_PATH = 'components/feed/core/proto'
_protoc_path = None


def protoc_path(root_dir):
  """Returns the path to the proto compiler, protoc."""
  global _protoc_path
  if not _protoc_path:
    protoc_list = list(
        glob.glob(os.path.join(root_dir, "out") + "/*/protoc")) + list(
            glob.glob(os.path.join(root_dir, "out") + "/*/*/protoc"))
    if not len(protoc_list):
      print("Can't find a suitable build output directory",
            "(it should have protoc)")
      sys.exit(1)
    _protoc_path = protoc_list[0]
  return _protoc_path


def main(argv):
  if len(argv) > 1:
    raise app.UsageError('Too many command-line arguments.')
  if not FLAGS.chromium_path:
    raise app.UsageError('chromium_path flag must be set.')
  if not FLAGS.source_file:
    raise app.UsageError('source_file flag must be set.')
  if not FLAGS.output_file:
    raise app.UsageError('output_file flag must be set.')

  protoc_cmd = CMD_TEMPLATE.format(
      FLAGS.source_file, protoc_path(FLAGS.chromium_path), ENCODE_NAMESPACE,
      FLAGS.chromium_path,
      os.path.join(FLAGS.chromium_path,
                   COMPONENT_FEED_PROTO_PATH), FLAGS.output_file)
  subprocess.call(protoc_cmd, shell=True)


if __name__ == '__main__':
  app.run(main)
