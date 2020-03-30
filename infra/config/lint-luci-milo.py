#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks that the main console and subconsole configs are consistent."""

import collections
import difflib
import json
import os
import sys

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_ROOT = os.path.join(THIS_DIR, '..', '..')
sys.path.insert(1, os.path.join(
    SRC_ROOT, "third_party", "protobuf", "python"))

import google.protobuf.text_format
import project_pb2


def compare_builders(name, main_builders, sub_builders):
  # Checks that the builders on a subwaterfall on the main waterfall
  # are consistent with the builders on that subwaterfall's main page.
  # Any build present in the main console must also be present in the
  # subwaterfall console, though the reverse is not necessarily true.
  # For the builders that are present, the entries must be the same with the
  # exception that the sub waterfall's entry will not have the subwaterfall name
  # as the first category component.
  def to_dict(builder, category_prefix=None):
    d = {'name': '.'.join(builder.name)}
    category = '|'.join(c for c in (category_prefix, builder.category) if c)
    if category:
      d['category'] = category
    if builder.short_name:
      d['short_name'] = builder.short_name
    return d

  main_list = [to_dict(b) for b in main_builders]
  required = set(d['name'] for d in main_list)

  # We don't require that every builder in the subwaterfall appear in the main
  # console, just that the ones that are present are the equivalent
  sub_list = [to_dict(b, name) for b in sub_builders]
  sub_list = [d for d in sub_list if d['name'] in required]

  if main_list != sub_list:
    print ('Entries differ between main waterfall and stand-alone {} waterfall:'
           .format(name))
    def to_json(obj):
      return json.dumps(obj, indent=2, separators=(',', ':'))
    print '\n'.join(difflib.unified_diff(
        to_json(sub_list).splitlines(), to_json(main_list).splitlines(),
        fromfile=name, tofile='main', lineterm=''))
    print
    return False
  return True


def main():
  project = project_pb2.Project()
  with open(os.path.join(THIS_DIR, 'generated', 'luci-milo.cfg'), 'rb') as f:
    google.protobuf.text_format.Parse(f.read(), project)

  # Maps subwaterfall name to list of builders on that subwaterfall
  # on the main waterfall.
  subwaterfalls = collections.defaultdict(list)
  for console in project.consoles:
    if console.id == 'main':
      # Chromium main waterfall console.
      for builder in console.builders:
        subwaterfall = builder.category.split('|', 1)[0]
        subwaterfalls[subwaterfall].append(builder)

  # subwaterfalls contains the waterfalls referenced by the main console
  # Check that every referenced subwaterfall has its own console, unless it's
  # explicitly excluded below.
  excluded_names = [
      # This is the chrome console in src-internal.
      'chrome',
  ]
  all_console_names = [console.id for console in project.consoles]
  referenced_names = set(subwaterfalls.keys())
  missing_names = referenced_names - set(all_console_names + excluded_names)
  if missing_names:
    print 'Missing subwaterfall console for', missing_names
    return 1

  # Check that the bots on a subwaterfall match the corresponding bots on the
  # main waterfall
  all_good = True
  for console in project.consoles:
    if console.id in subwaterfalls:
      if not compare_builders(console.id, subwaterfalls[console.id],
                              console.builders):
        all_good = False
  return 0 if all_good else 1


if __name__ == '__main__':
  sys.exit(main())
