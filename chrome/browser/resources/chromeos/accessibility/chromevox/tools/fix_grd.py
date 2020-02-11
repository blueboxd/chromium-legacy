#!/usr/bin/env python

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Enforces message text style.

Expects one argument to be the path of the .grd.

Sample usage:

./fix_grd.py some_messages.grd
git diff
# Audit changes (e.g. formatting, quotes, etc).
'''

import optparse
import sys
import xml.etree.ElementTree as ET

def Die(message):
  '''Prints an error message and exit the program.'''
  print >> sys.stderr, message
  sys.exit(1)

def Process(xml_file):
  xml = ET.parse(xml_file)
  root = xml.getroot()
  messages = root.findall('release/messages/message')
  modified = False
  for message in messages:
    # Re-write messages containing a period at the end (excluding whitespace)
    # and no other periods. This excludes phrases that have multiple sentences
    # which should retain periods.
    text = message.text.rstrip()
    if text.endswith('.') and text.find('.') == text.rfind('.'):
      modified = True
      message.text = message.text.replace('.', '')

  if modified:
    xml.write(xml_file, encoding='UTF-8')

if __name__ == '__main__':
  options, args = optparse.OptionParser().parse_args()
  if len(args) != 1:
    Die('Expected one .grd file')
  Process(args[0])
