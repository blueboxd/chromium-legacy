# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import expand_owners
import xml.dom.minidom


class ExpandOwnersTest(unittest.TestCase):

  def testExpandOwnersWithBasicOWNERSFilePath(self):
    """Checks that OWNERS files are expanded."""
    histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>src/tools/metrics/histograms/test_files/basic_OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

<histogram name="Maple.Syrup" units="mL">
  <owner>joe@chromium.org</owner>
  <owner>src/tools/metrics/histograms/test_files/basic_OWNERS</owner>
  <summary>I like maple syrup, too.</summary>
</histogram>

</histograms>
""")
    expected_histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>marypoppins@chromium.org</owner>
  <owner>bert@google.com</owner>
  <summary>I like coffee.</summary>
</histogram>

<histogram name="Maple.Syrup" units="mL">
  <owner>joe@chromium.org</owner>
  <owner>marypoppins@chromium.org</owner>
  <owner>bert@google.com</owner>
  <summary>I like maple syrup, too.</summary>
</histogram>

</histograms>
""")
    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertMultiLineEqual(histograms.toxml(), expected_histograms.toxml())

  def testExpandOwnersWithFileDirectiveOWNERSFilePath(self):
    """Checks that OWNERS files with file directives are expanded."""
    histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>src/tools/metrics/histograms/test_files/file_directive_OWNERS</owner>
  <owner>bert@google.com</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")
    expected_histograms = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <owner>marypoppins@chromium.org</owner>
  <owner>ellen@google.com</owner>
  <owner>bert@google.com</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")
    expand_owners.ExpandHistogramsOWNERS(histograms)
    self.assertEqual(histograms.toxml(), expected_histograms.toxml())

  def testExpandOwnersWithoutOWNERSFilePath(self):
    """Checks that histograms without OWNERS file paths are unchanged."""
    histograms_without_file_paths = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chromium.org</owner>
  <summary>I like coffee.</summary>

</histogram>

</histograms>
""")
    expected_histograms = histograms_without_file_paths
    expand_owners.ExpandHistogramsOWNERS(histograms_without_file_paths)
    self.assertEqual(histograms_without_file_paths, expected_histograms)

  def testExpandOwnersWithoutPrimaryOwnerFirst(self):
    """Checks that an error is raised when a primary owner is not listed first.

    A primary owner is an individual's email address, e.g. rose@chromium.org.
    """
    histograms_without_primary_owner = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>src/tools/metrics/histograms/test_files/basic_OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")
    with self.assertRaisesRegexp(
        expand_owners.Error,
        'The histogram Caffeination must have a primary owner, i.e. an '
        'individual\'s email address.'):
        expand_owners.ExpandHistogramsOWNERS(histograms_without_primary_owner)

  def testExpandOwnersWithFakeFilePath(self):
    """Checks that an error is raised with a fake OWNERS file path."""
    histograms_with_fake_file_path = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chormium.org</owner>
  <owner>src/medium/medium/roast/OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")
    with self.assertRaisesRegexp(
        expand_owners.Error,
        r'The path .*src/medium/medium/roast/OWNERS does not exist\.'):
        expand_owners.ExpandHistogramsOWNERS(histograms_with_fake_file_path)


  def testExpandOwnersWithoutOwnersFromFile(self):
    """Checks that an error is raised when no owners can be derived."""
    histograms_without_owners_from_file = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chormium.org</owner>
  <owner>src/tools/metrics/histograms/test_files/empty_OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")
    with self.assertRaisesRegexp(
        expand_owners.Error,
        r'No email addresses could be derived from .*empty_OWNERS\.'):
        expand_owners.ExpandHistogramsOWNERS(
            histograms_without_owners_from_file)

  def testExpandOwnersWithoutOWNERSPathPrefix(self):
    """Checks that an error is raised when the path is not well-formatted."""
    histograms_without_src_prefix = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chormium.org</owner>
  <owner>latte/OWNERS</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")
    with self.assertRaisesRegexp(
        expand_owners.Error,
        r'The given path latte/OWNERS is not well-formatted.*\.'):
        expand_owners.ExpandHistogramsOWNERS(histograms_without_src_prefix)

  def testExpandOwnersWithoutOWNERSPathSuffix(self):
    """Checks that an error is raised when the path is not well-formatted."""
    histograms_without_owners_suffix = xml.dom.minidom.parseString("""
<histograms>

<histogram name="Caffeination" units="mg">
  <owner>joe@chormium.org</owner>
  <owner>src/latte/file</owner>
  <summary>I like coffee.</summary>
</histogram>

</histograms>
""")
    with self.assertRaisesRegexp(
        expand_owners.Error,
        r'The given path src/latte/file is not well-formatted.*\.'):
        expand_owners.ExpandHistogramsOWNERS(histograms_without_owners_suffix)


class ExtractEmailAddressesTest(unittest.TestCase):

  def testExtractEmailAddressesUnsupportedSymbolsIgnored(self):
    """Checks that unsupported OWNERS files symbols are ignored.

    The unsupported symbols that may appear at the beginning of a line are as
    follows:
      (i) per-file
      (ii) *
      (iii) #
      (iv) set noparent
      (v) white space, e.g. a space or a blank line
    """
    path = expand_owners._GetOwnersFilePath(
      'src/tools/metrics/histograms/test_files/ignored_symbols_OWNERS')
    self.assertEqual(
      expand_owners._ExtractEmailAddressesFromOWNERS(path),
      ['joe@chromium.org'])

  def testExtractEmailAddressesLoopRaisesError(self):
    """Checks that an error is raised if OWNERS file path results in a loop."""
    path = expand_owners._GetOwnersFilePath(
      'src/tools/metrics/histograms/test_files/loop_OWNERS')

    with self.assertRaisesRegexp(
      expand_owners.Error,
      r'.*The path.*loop_OWNERS may be part of an OWNERS loop\.'):
      expand_owners._ExtractEmailAddressesFromOWNERS(path)


if __name__ == '__main__':
    unittest.main()
