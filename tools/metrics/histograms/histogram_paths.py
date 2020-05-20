# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Paths to description XML files in this directory."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

ALL_XMLS_RELATIVE = [
    'tools/metrics/histograms/enums.xml',
    'tools/metrics/histograms/histograms.xml'
]
ALL_XMLS = [path_util.GetInputFile(f) for f in ALL_XMLS_RELATIVE]
ENUMS_XML, HISTOGRAMS_XML = ALL_XMLS
