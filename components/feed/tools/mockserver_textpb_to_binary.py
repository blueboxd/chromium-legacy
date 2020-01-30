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

Make sure you have absl-py installed via 'python3 -m pip install absl-py'.

Usage example:
    python3 ./mockserver_textpb_to_binary.py
    --chromium_path ~/chromium/src
    --output_file /tmp/binary.pb
    --source_file /tmp/original.textpb
    --alsologtostderr
"""

import glob
import os
import protoc_util
import subprocess

from absl import app
from absl import flags

FLAGS = flags.FLAGS
FLAGS = flags.FLAGS
flags.DEFINE_string('chromium_path', '', 'The path of your chromium depot.')
flags.DEFINE_string('output_file', '', 'The target output binary file path.')
flags.DEFINE_string('source_file', '',
                    'The source proto file, in textpb format, path.')

ENCODE_NAMESPACE = 'components.feed.core.proto.wire.mockserver.MockServer'
COMPONENT_FEED_PROTO_PATH = 'components/feed/core/proto'


def main(argv):
  if len(argv) > 1:
    raise app.UsageError('Too many command-line arguments.')
  if not FLAGS.chromium_path:
    raise app.UsageError('chromium_path flag must be set.')
  if not FLAGS.source_file:
    raise app.UsageError('source_file flag must be set.')
  if not FLAGS.output_file:
    raise app.UsageError('output_file flag must be set.')

  with open(FLAGS.source_file) as file:
    value_text_proto = file.read()

  encoded = protoc_util.encode_proto(value_text_proto, ENCODE_NAMESPACE,
                                     FLAGS.chromium_path,
                                     COMPONENT_FEED_PROTO_PATH)
  with open(FLAGS.output_file, 'wb') as file:
    file.write(encoded)


if __name__ == '__main__':
  app.run(main)
