#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility for generating classes for structured metrics events.

It takes as input a structured.xml file describing all events and produces a
c++ header and implementation file exposing builders for those events.
"""

import argparse
import sys

import model
import events_template
import compile_time_validation

parser = argparse.ArgumentParser(
  description='Generate structured metrics events')
parser.add_argument('--input', help='Path to structured.xml')
parser.add_argument('--output', help='Path to generated files.')

def main(argv):
  args = parser.parse_args()

  data = model.XML_TYPE.Parse(open(args.input).read())
  relpath = 'components/metrics/structured'
  compile_time_validation.validate(data)
  events_template.WriteFiles(args.output, relpath, data)

  return 0

if '__main__' == __name__:
  sys.exit(main(sys.argv))
