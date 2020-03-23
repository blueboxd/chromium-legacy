#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that the structured.xml file is well-formatted."""

import os
import re
import sys
from xml.dom import minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

STRUCTURED_XML = path_util.GetInputFile(('tools/metrics/structured/'
                                         'structured.xml'))

def checkEventsHaveOwners(config):
  """Check that every event in the config has at least one owner."""
  errors = []
  events = set()

  for event_node in config.getElementsByTagName('event'):
    event_name = event_node.getAttribute('name')
    # Check for duplicate event names.
    if event_name in events:
      errors.append("duplicate event name '%s'" % event_name)
    events.add(event_name)

    owner_nodes = event_node.getElementsByTagName('owner')
    metric_nodes = event_node.getElementsByTagName('metric')

    # Check <owner> tag is present for each event.
    if not owner_nodes:
      errors.append("<owner> tag is required for event '%s'." % event_name)
      continue

    for owner_node in owner_nodes:
      # Check <owner> tag actually has some content.
      if not owner_node.childNodes:
        errors.append(
            "<owner> tag for event '%s' should not be empty." % event_name)
      for email in owner_node.childNodes:
        # Check <owner> tag's content is an email address, not a username.
        if not re.match('^.+@(chromium\.org|google\.com)$', email.data):
          errors.append("<owner> tag for event '%s' expects a Chromium or "
                        "Google email address, instead found '%s'." %
                        (event_name, email.data.strip()))

    # Check for duplicate metric names within an event.
    metrics = set()
    for metric_node in metric_nodes:
      metric_name = metric_node.getAttribute('name')
      if metric_name in metrics:
        errors.append("duplicate metric name '%s' for event '%s'" %
                      (metric_name, event_name))
      metrics.add(metric_name)

  return errors

def main():
  with open(STRUCTURED_XML, 'r') as config_file:
    document = minidom.parse(config_file)
    [config] = document.getElementsByTagName('structured-metrics')
    [events] = document.getElementsByTagName('events')

    errors = checkEventsHaveOwners(config)
    if errors:
      return 'ERRORS:' + ''.join('\n  ' + e for e in errors)

if __name__ == '__main__':
  sys.exit(main())
