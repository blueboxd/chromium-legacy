# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'histograms'))
import extract_histograms
import histogram_paths
import merge_xml

class UkmXmlValidation(object):
  """Validations for the content of ukm.xml."""

  def __init__(self, ukm_config):
    """Attributes:

    config: A XML minidom Element representing the root node of the UKM config
        tree.
    """
    self.config = ukm_config

  def checkEventsHaveOwners(self):
    """Check that every event in the config has at least one owner."""
    errors = []

    for event_node in self.config.getElementsByTagName('event'):
      event_name = event_node.getAttribute('name')
      owner_nodes = event_node.getElementsByTagName('owner')

      # Check <owner> tag is present for each event.
      if not owner_nodes:
        errors.append("<owner> tag is required for event '%s'." % event_name)
        continue

      for owner_node in owner_nodes:
        # Check <owner> tag actually has some content.
        if not owner_node.childNodes:
          errors.append(
              "<owner> tag for event '%s' should not be empty." % event_name)
          continue

        for email in owner_node.childNodes:
          # Check <owner> tag's content is an email address, not a username.
          if not ('@chromium.org' in email.data or '@google.com' in email.data):
            errors.append("<owner> tag for event '%s' expects a Chromium or "
                          "Google email address." % event_name)

    isSuccess = not errors

    return (isSuccess, errors)

  def checkMetricTypeIsSpecified(self):
    """Check each metric is either specified with an enum or a unit."""
    errors = []
    warnings = []

    enum_tree = merge_xml.MergeFiles([histogram_paths.ENUMS_XML])
    enums, _ = extract_histograms.ExtractEnumsFromXmlTree(enum_tree)

    for event_node in self.config.getElementsByTagName('event'):
      for metric_node in event_node.getElementsByTagName('metric'):
        if metric_node.hasAttribute('enum'):
          enum_name = metric_node.getAttribute('enum');
          # Check if the enum is defined in enums.xml.
          if enum_name not in enums:
            errors.append("Unknown enum %s in ukm metric %s:%s." %
                          (enum_name, event_node.getAttribute('name'),
                          metric_node.getAttribute('name')))
        elif not metric_node.hasAttribute('unit'):
          warnings.append("Warning: Neither \'enum\' or \'unit\' is specified "
                          "for ukm metric %s:%s."
                          % (event_node.getAttribute('name'),
                             metric_node.getAttribute('name')))

    isSuccess = not errors
    return (isSuccess, errors, warnings)
