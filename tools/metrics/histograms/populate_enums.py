# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for populating enums with ukm events."""

import os
import sys
import xml.dom.minidom

import extract_histograms

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'ukm'))
import codegen


def PopulateEnumWithUkmEvents(doc, enum, ukm_events):
  """Populates the enum node with a list of ukm events.

  Args:
    doc: The document to create the node in.
    enum: The enum node needed to be populated.
    ukm_events: A list of ukm event nodes.
  """
  for event in ukm_events:
    node = doc.createElement('int')
    event_name = event.getAttribute('name')
    # The value is UKM event name hash truncated to 31 bits. This is recorded in
    # https://cs.chromium.org/chromium/src/components/ukm/ukm_recorder_impl.cc?rcl=728ad079d8e52ada4e321fb4f53713e4f0588072&l=114
    node.attributes['value'] = str(codegen.HashName(event_name) & 0x7fffffff)
    label = event_name
    # If the event is obsolete, mark it in the int's label.
    if event.getElementsByTagName('obsolete'):
      label += ' (Obsolete)'
    node.attributes['label'] = label
    enum.appendChild(node)


def PopulateEnumsWithUkmEvents(doc, enums, ukm_events):
  """Populates enum nodes in the enums with a list of ukm events

  Args:
    doc: The document to create the node in.
    enums: The enums node to be iterated.
    ukm_events: A list of ukm event nodes.
  """
  for enum in extract_histograms.IterElementsWithTag(enums, 'enum', 1):
    # We only special case 'UkmEventNameHash' currently.
    if enum.getAttribute('name') == 'UkmEventNameHash':
      PopulateEnumWithUkmEvents(doc, enum, ukm_events)