# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Paths to description XML files in this directory."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util


def _FindHistogramsXmlFiles(base_dir):
  """Gets a list of all histograms.xml files under the directory tree, prefixed
  with tools/metrics/histograms/."""
  file_list = []
  for dirName, _, fileList in os.walk(base_dir):
    for filename in fileList:
      if filename == 'histograms.xml':
        filepath = os.path.join('tools/metrics/histograms/', dirName, filename)
        file_list.append(filepath)
  return file_list


ENUMS_XML_RELATIVE = 'tools/metrics/histograms/enums.xml'
# This file path accounts for cases where the script is executed from other
# metrics-related directories.
PATH_TO_HISTOGRAMS_XML_DIR = os.path.join('..', 'histograms/histograms_xml')
# In the middle state, histogram paths include both the large histograms.xml
# file as well as the split up files.
# TODO: Improve on the current design to avoid calling `os.walk()` at the time
# of module import.
HISTOGRAMS_XMLS_RELATIVE = (['tools/metrics/histograms/histograms.xml'] +
                            _FindHistogramsXmlFiles(PATH_TO_HISTOGRAMS_XML_DIR))
OBSOLETE_XML_RELATIVE = ('tools/metrics/histograms/histograms_xml/'
                         'obsolete_histograms.xml')
ALL_XMLS_RELATIVE = [ENUMS_XML_RELATIVE, OBSOLETE_XML_RELATIVE
                     ] + HISTOGRAMS_XMLS_RELATIVE

ENUMS_XML = path_util.GetInputFile(ENUMS_XML_RELATIVE)
UKM_XML = path_util.GetInputFile('tools/metrics/ukm/ukm.xml')
HISTOGRAMS_XMLS = [path_util.GetInputFile(f) for f in HISTOGRAMS_XMLS_RELATIVE]
OBSOLETE_XML = path_util.GetInputFile(OBSOLETE_XML_RELATIVE)
ALL_XMLS = [ENUMS_XML, OBSOLETE_XML] + HISTOGRAMS_XMLS

ALL_TEST_XMLS_RELATIVE = [
    'tools/metrics/histograms/test_data/enums.xml',
    'tools/metrics/histograms/test_data/histograms.xml',
    'tools/metrics/histograms/test_data/ukm.xml',
]
ALL_TEST_XMLS = [path_util.GetInputFile(f) for f in ALL_TEST_XMLS_RELATIVE]
TEST_ENUMS_XML, TEST_HISTOGRAMS_XML, TEST_UKM_XML = ALL_TEST_XMLS
