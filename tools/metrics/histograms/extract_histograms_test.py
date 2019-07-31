# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import unittest
import xml.dom.minidom

import extract_histograms


class ExtractHistogramsTest(unittest.TestCase):

  def testNewHistogramWithoutSummary(self):
    histogram_without_summary = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
   <owner>person@chromium.org</owner>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_without_summary, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithEmptyOwnerTag(self):
    histogram_with_empty_owner_tag = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner></owner>
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_empty_owner_tag, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithoutOwnerTag(self):
    histogram_without_owner_tag = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_without_owner_tag, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithCommaSeparatedOwners(self):
    histogram_with_comma_separated_owners = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner>cait@chromium.org, paul@chromium.org</owner>
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_comma_separated_owners, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithInvalidOwner(self):
    histogram_with_invalid_owner = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner>sarah</owner>
  <summary> This is a summary </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_invalid_owner, {})
    self.assertTrue(have_errors)

  def testNewHistogramWithOwnerPlaceHolder(self):
    histogram_with_owner_placeholder = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner> Please list the metric's owners. Add more owner tags as needed.
  </owner>
  <summary>
    <!-- Comments are fine -->
    This is a summary
    <!-- Comments are fine -->
  </summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    _, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_owner_placeholder, {})
    self.assertFalse(have_errors)

  def testHistogramWithEscapeCharacters(self):
    histogram_with_owner_placeholder = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
 <histogram name="Test.Histogram" units="things">
  <owner> Please list the metric's owners. Add more owner tags as needed.
  </owner>
  <summary>This is a summary with &amp; and &quot; and &apos;</summary>
 </histogram>
</histograms>
</histogram-configuration>
""")
    hists, have_errors = extract_histograms._ExtractHistogramsFromXmlTree(
        histogram_with_owner_placeholder, {})
    self.assertFalse(have_errors)
    self.assertIn('Test.Histogram', hists)
    self.assertIn('summary', hists['Test.Histogram'])
    self.assertEquals('This is a summary with & and " and \'',
                      hists['Test.Histogram']['summary'])

if __name__ == "__main__":
  logging.basicConfig(level=logging.ERROR + 1)
  unittest.main()
