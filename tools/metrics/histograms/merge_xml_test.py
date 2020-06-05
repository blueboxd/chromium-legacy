# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import xml.dom.minidom

import histogram_paths
import merge_xml


class MergeXmlTest(unittest.TestCase):

  def testMergeFiles(self):
    """Checks that enums.xml and histograms.xml can merge successfully."""
    merged = merge_xml.MergeFiles(
        [histogram_paths.TEST_ENUMS_XML, histogram_paths.TEST_HISTOGRAMS_XML])
    # If ukm.xml is not provided, there is no need to populate the
    # UkmEventNameHash enum.
    expected_merged = xml.dom.minidom.parseString("""
<histogram-configuration><enums>

<enum name="Enum1">
  <int label="Value0" value="0"/>
  <int label="Value1" value="1"/>
</enum>

<enum name="TestEnum">
  <int label="Value0" value="0"/>
  <int label="Value1" value="1"/>
</enum>

<enum name="UkmEventNameHash">
<!-- Placeholder enum. The values are UKM event name hashes truncated to 31
     bits. This gets populated by the GetEnumsNodes function in merge_xml.py
     when producing the merged XML file. -->

</enum>

</enums><histograms>

<histogram name="Foo.Bar" units="xxxxxxxxxxxxxxxxxxyyyyyyyyyyyyyyyyyyyyyyzzzz">
  <summary>Foo</summary>
  <owner>person@chromium.org</owner>
  <component>Component</component>
</histogram>

<histogram name="Test.Histogram" units="microseconds">
  <obsolete>
    Removed 6/2020.
  </obsolete>
  <owner>person@chromium.org</owner>
  <summary>
    Summary 2
  </summary>
</histogram>

<histogram name="Test.EnumHistogram" enum="TestEnum" expires_after="M81">
  <owner>uma@chromium.org</owner>
  <obsolete>
    Obsolete message
  </obsolete>
  <summary>
    A enum histogram.
  </summary>
</histogram>

</histograms><histogram_suffixes_list>

<histogram_suffixes name="Test.HistogramSuffixes" separator=".">
  <suffix label="A histogram_suffixes" name="TestSuffix"/>
  <affected-histogram name="Test.Histogram"/>
</histogram_suffixes>

</histogram_suffixes_list></histogram-configuration>
""")
    self.maxDiff = None
    self.assertEqual(expected_merged.toxml(), merged.toxml())

  def testMergeFiles_WithXmlEvents(self):
    """Checks that the UkmEventNameHash enum is populated correctly.

    If ukm.xml is provided, populate a list of ints to the UkmEventNameHash enum
    where where each value is a xml event name hash truncated to 31 bits and
    each label is the corresponding event name.
    """
    merged = merge_xml.MergeFiles(histogram_paths.ALL_TEST_XMLS)
    expected_merged = xml.dom.minidom.parseString("""
<histogram-configuration><enums>

<enum name="Enum1">
  <int label="Value0" value="0"/>
  <int label="Value1" value="1"/>
</enum>

<enum name="TestEnum">
  <int label="Value0" value="0"/>
  <int label="Value1" value="1"/>
</enum>

<enum name="UkmEventNameHash">
<!-- Placeholder enum. The values are UKM event name hashes truncated to 31
     bits. This gets populated by the GetEnumsNodes function in merge_xml.py
     when producing the merged XML file. -->

<int label="AbusiveExperienceHeuristic.WindowOpen" value="324605288"/>\
<int label="AbusiveExperienceHeuristic.TabUnder" value="1621538456"/>\
<int label="Autofill.SelectedMaskedServerCard (Obsolete)" value="1913876024"/>\
</enum>

</enums><histograms>

<histogram name="Foo.Bar" units="xxxxxxxxxxxxxxxxxxyyyyyyyyyyyyyyyyyyyyyyzzzz">
  <summary>Foo</summary>
  <owner>person@chromium.org</owner>
  <component>Component</component>
</histogram>

<histogram name="Test.Histogram" units="microseconds">
  <obsolete>
    Removed 6/2020.
  </obsolete>
  <owner>person@chromium.org</owner>
  <summary>
    Summary 2
  </summary>
</histogram>

<histogram name="Test.EnumHistogram" enum="TestEnum" expires_after="M81">
  <owner>uma@chromium.org</owner>
  <obsolete>
    Obsolete message
  </obsolete>
  <summary>
    A enum histogram.
  </summary>
</histogram>

</histograms><histogram_suffixes_list>

<histogram_suffixes name="Test.HistogramSuffixes" separator=".">
  <suffix label="A histogram_suffixes" name="TestSuffix"/>
  <affected-histogram name="Test.Histogram"/>
</histogram_suffixes>

</histogram_suffixes_list></histogram-configuration>
""")
    self.maxDiff = None
    self.assertEqual(expected_merged.toxml(), merged.toxml())


if __name__ == '__main__':
  unittest.main()
