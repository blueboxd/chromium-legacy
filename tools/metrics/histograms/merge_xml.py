#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script to merge multiple source xml files into a single histograms.xml."""

import argparse
import os
import sys
import xml.dom.minidom

import expand_owners
import extract_histograms
import histograms_print_style
import populate_enums


def GetElementsByTagName(trees, tag):
  """Gets all elements with the specified tag from a set of DOM trees.

  Args:
    trees: A list of DOM trees.
    tag: The tag of the elements to find.
  Returns:
    A list of DOM nodes with the specified tag.
  """
  iterator = extract_histograms.IterElementsWithTag
  return list(e for t in trees for e in iterator(t, tag, 2))


def GetEnumsNodes(doc, trees):
  """Gets all enums from a set of DOM trees.

  If trees contain ukm events, populates a list of ints to the
  "UkmEventNameHash" enum where each value is a ukm event name hash truncated
  to 31 bits and each label is the corresponding event name.

  Args:
    doc: The document to create the node in.
    trees: A list of DOM trees.
  Returns:
    A list of enums DOM nodes.
  """
  enums_list = GetElementsByTagName(trees, 'enums')
  ukm_events = GetElementsByTagName(
      GetElementsByTagName(trees, 'ukm-configuration'), 'event')
  # Early return if there are not ukm events provided. MergeFiles have callers
  # that do not pass ukm events so, in that case, we don't need to iterate
  # the enum list.
  if not ukm_events:
    return enums_list
  for enums in enums_list:
    populate_enums.PopulateEnumsWithUkmEvents(doc, enums, ukm_events)
  return enums_list


def MakeNodeWithChildren(doc, tag, children):
  """Creates a DOM node with specified tag and child nodes.

  Args:
    doc: The document to create the node in.
    tag: The tag to create the node with.
    children: A list of DOM nodes to add as children.
  Returns:
    A DOM node.
  """
  node = doc.createElement(tag)
  for child in children:
    if child.tagName == 'histograms':
      expand_owners.ExpandHistogramsOWNERS(child)
    node.appendChild(child)
  return node


def MergeTrees(trees):
  """Merges a list of histograms.xml DOM trees.

  Args:
    trees: A list of histograms.xml DOM trees.
  Returns:
    A merged DOM tree.
  """
  doc = xml.dom.minidom.Document()
  doc.appendChild(MakeNodeWithChildren(doc, 'histogram-configuration',
    # This can result in the merged document having multiple <enums> and
    # similar sections, but scripts ignore these anyway.
    GetEnumsNodes(doc, trees) +
    GetElementsByTagName(trees, 'histograms') +
    GetElementsByTagName(trees, 'histogram_suffixes_list')))
  return doc


def MergeFiles(filenames=[], files=[]):
  """Merges a list of histograms.xml files.

  Args:
    filenames: A list of histograms.xml filenames.
    files: A list of histograms.xml file-like objects.
  Returns:
    A merged DOM tree.
  """
  all_files = files + [open(f) for f in filenames]
  trees = [xml.dom.minidom.parse(f) for f in all_files]
  return MergeTrees(trees)


def PrettyPrintMergedFiles(filenames=[], files=[]):
  return histograms_print_style.GetPrintStyle().PrettyPrintXml(
      MergeFiles(filenames=filenames, files=files))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('inputs', nargs="+")
  parser.add_argument('--output', required=True)
  args = parser.parse_args()
  with open(args.output, 'w') as f:
    f.write(PrettyPrintMergedFiles(args.inputs))


if __name__ == '__main__':
  main()
