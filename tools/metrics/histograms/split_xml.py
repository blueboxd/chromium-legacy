# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Splits a XML file into smaller XMLs in subfolders.

Splits nodes according to the first camelcase part of their name attribute.
Intended to be used to split up the large histograms.xml or enums.xml file.
"""

import logging
import os
import sys
from xml.dom import minidom

import histogram_configuration_model

# The default name of the histograms XML file.
HISTOGRAMS_XML = 'histograms.xml'
# Number of times that splitting of histograms will be carried out.
TARGET_DEPTH = 1
# The number of histograms below which they will be aggregated into
# the histograms.xml in 'others'.
AGGREGATE_THRESHOLD = 20


def _ParseHistogramsXMLFile(base_dir):
  """Parses the |HISTOGRAMS_XML| in |base_dir|.

  Args:
    base_dir: The directory that contains the histograms.xml to be split.

  Returns:
    comments: A list of top-level comment nodes.
    histogram_nodes: A DOM NodeList object containing <histogram> nodes
    histogram_suffixes_nodes: A DOM NodeList object containing
        <histogram_suffixes> nodes.

  Raises:
    FileNotFoundError if histograms.xml not found in |base_dir|.
  """
  if HISTOGRAMS_XML not in os.listdir(base_dir):
    raise ValueError(HISTOGRAMS_XML + 'is not in %s' % base_dir)

  dom = minidom.parse(os.path.join(base_dir, HISTOGRAMS_XML))
  comments = []

  # Get top-level comments. It is assumed that all comments are placed before
  # tags. Therefore the loop will stop if it encounters a non-comment node.
  for node in dom.childNodes:
    if node.nodeType == minidom.Node.COMMENT_NODE:
      comments.append(node)
    else:
      break

  histogram_nodes = dom.getElementsByTagName('histogram')
  histogram_suffixes_nodes = dom.getElementsByTagName('histogram_suffixes')
  return comments, histogram_nodes, histogram_suffixes_nodes


def _CreateXMLFile(comments, parent_node_string, nodes, output_dir, filename):
  """Creates XML file for given type of XML nodes.

  This function also creates a |parent_node_string| tag as the parent node, e.g.
  <histograms> or <histogram_suffixes_list>, that wraps all the |nodes| in the
  output XML.

  Args:
    comments: Top level comment nodes from the original histograms.xml file.
    parent_node_string: The name of the the second-level parent node, e.g.
        <histograms> or <histogram_suffixes_list>.
    nodes: A DOM NodeList object or a list containing <histogram> or
        <histogram_suffixes> that will be inserted under the parent node.
    output_dir: The output directory.
    filename: The output filename.
  """
  doc = minidom.Document()

  # Attach top-level comments.
  for comment_node in comments:
    doc.appendChild(comment_node)

  # Create the <histogram-configuration> node for the new histograms.xml file.
  histogram_config_element = doc.createElement('histogram-configuration')
  doc.appendChild(histogram_config_element)
  parent_element = doc.createElement(parent_node_string)
  histogram_config_element.appendChild(parent_element)

  # Under the parent node, append the children nodes.
  for node in nodes:
    parent_element.appendChild(node)

  # Use the model to get pretty-printed XML string and write into file.
  with open(os.path.join(output_dir, filename), 'w') as output_file:
    pretty_xml_string = histogram_configuration_model.PrettifyTree(doc)
    output_file.write(pretty_xml_string)


def GetCamelName(histogram_node, depth=0):
  """Returns the first camelcase name part of the histogram node.

  Args:
    histogram_node: The histogram node.
    depth: The depth that specifies which name part will be returned.
        e.g. For a histogram of name
        'CustomTabs.DynamicModule.CreatePackageContextTime'
        The returned camelname for depth 0 is 'Custom';
        The returned camelname for depth 1 is 'Dynamic';
        The returned camelname for depth 2 is 'Create'.

        Default depth is set to 0 as this function is imported and
        used in other files, where depth used is 0.

  Returns:
    The camelcase namepart at specified depth.
    If the number of name parts is less than the depth, return 'others'.
  """
  histogram_name = histogram_node.getAttribute('name')
  split_string_list = histogram_name.split('.')
  if len(split_string_list) <= depth:
    return 'others'
  else:
    name_part = split_string_list[depth]
    start_index = 0
    # |all_upper| is used to identify the case where the name is ABCDelta, in
    # which case the camelname of depth 0 should be ABC, instead of A.
    all_upper = True
    for index, letter in enumerate(name_part):
      if letter.islower() or letter.isnumeric():
        all_upper = False
      if letter.isupper() and all_upper == False:
        start_index = index
        break

  if start_index == 0:
    return name_part
  else:
    return name_part[0:start_index]


def _AddHistogramsToDict(histogram_nodes, depth):
  """Adds histogram nodes to the corresponding keys of a dictionary.

  Args:
    histogram_nodes: A NodeList object containing <histogram> nodes.
    depth: The depth at which to get the histogram key by.

  Returns:
    document_dict: A dictionary where the key is the prefix and the value is a
        list of histogram nodes.
  """
  document_dict = {}
  for histogram in histogram_nodes:
    name_part = GetCamelName(histogram, depth)
    if name_part not in document_dict:
      document_dict[name_part] = []
    document_dict[name_part].append(histogram)

  return document_dict


def _OutputToFolderAndXML(histogram_node_list, output_dir, key, comments):
  """Creates new folder and XML file for separated histograms.

  Args:
    histogram_node_list: A of histograms of a prefix.
    output_dir: The output directory.
    key: The prefix of the histograms, also the name of the new folder.
    comments: The omments from the initial histograms.xml that should be added
        to the resulting histograms.xml files.
  """
  new_folder = os.path.join(output_dir, key)
  if not os.path.exists(new_folder):
    os.mkdir(new_folder)
  _CreateXMLFile(comments, 'histograms', histogram_node_list, new_folder,
                 HISTOGRAMS_XML)


def _AggregateAndOutput(document_dict, output_dir, comments):
  """Aggregates groups of histograms below threshold number into 'others'.

  Args:
    document_dict: A dictionary where the key is the prefix of the histogram and
        value is a list of histogram nodes.
    output_dir: The output directory of the resulting folders.
    comments: The comments from the initial histograms.xml that should be added
        to the resulting histograms.xml files.
  """
  if 'others' not in document_dict:
    document_dict['others'] = []

  for key in document_dict.keys():
    # As histograms might still be added to 'others', only create XML for
    # 'others' key at the end.
    if key == 'others':
      continue

    # For a prefix, if the number of histograms is fewer than threshold,
    # aggregate into others.
    if len(document_dict[key]) < AGGREGATE_THRESHOLD:
      document_dict['others'] += document_dict[key]
    else:
      _OutputToFolderAndXML(document_dict[key], output_dir, key, comments)

  _OutputToFolderAndXML(document_dict['others'], output_dir, 'others', comments)


def _SplitHistograms(output_dir, depth):
  """Splits the histograms node at given depth, aggregates and outputs."""
  comments, histogram_nodes, _ = _ParseHistogramsXMLFile(output_dir)
  document_dict = _AddHistogramsToDict(histogram_nodes, depth)
  _AggregateAndOutput(document_dict, output_dir, comments)


def _SplitHistogramsRecursive(output_base_dir, current_depth):
  """Calls itself recursively on newly output directories until target depth."""
  _SplitHistograms(output_base_dir, current_depth)
  histogram_file = os.path.join(output_base_dir, HISTOGRAMS_XML)

  # Remove original histograms.xml file after splitting.
  if os.path.exists(histogram_file):
    os.remove(histogram_file)

  current_depth += 1
  if current_depth < TARGET_DEPTH:
    # For each subfolder, split the histograms.xml file inside.
    for subfolder in [
        os.path.join(output_base_dir, subdir)
        for subdir in os.listdir(output_base_dir)
        if os.path.isdir(os.path.join(output_base_dir, subdir))
    ]:
      _SplitHistogramsRecursive(subfolder, current_depth)


def _SeparateObsoleteHistogram(histogram_nodes):
  """Separates a NodeList of histograms into obsolete and non-obsolete.

  Args:
    histogram_nodes: A NodeList object containing histogram nodes.

  Returns:
    obsolete_nodes: A list of obsolete nodes.
    non_obsolete_nodes: A list of non-obsolete nodes.
  """
  obsolete_nodes = []
  non_obsolete_nodes = []
  for histogram in histogram_nodes:
    obsolete_tag_nodelist = histogram.getElementsByTagName('obsolete')
    if len(obsolete_tag_nodelist) > 0:
      obsolete_nodes.append(histogram)
    else:
      non_obsolete_nodes.append(histogram)
  return obsolete_nodes, non_obsolete_nodes


def SplitIntoMultipleHistogramXMLs(base_dir, output_base_dir):
  """Splits a large histograms.xml and writes out the split xmls.

  Args:
    base_dir: The directory that contains the histograms.xml to be split.
    output_base_dir: The output base directory.
  """
  if not os.path.exists(output_base_dir):
    os.mkdir(output_base_dir)


  comments, histogram_nodes, histogram_suffixes_nodes = _ParseHistogramsXMLFile(
      base_dir)
  # Create separate XML file for histogram suffixes.
  _CreateXMLFile(comments,
                 'histogram_suffixes_list',
                 histogram_suffixes_nodes,
                 output_base_dir,
                 filename='histogram_suffixes.xml')

  obsolete_nodes, non_obsolete_nodes = _SeparateObsoleteHistogram(
      histogram_nodes)
  # Create separate XML file for obsolete histograms.
  _CreateXMLFile(comments,
                 'histograms',
                 obsolete_nodes,
                 output_base_dir,
                 filename='obsolete_histograms.xml')

  # Create separate XML file for non-obsolete histograms, which will be then be
  # recursively split up.
  # TODO: crbug/1112879 Improve design by not writing and reading XML files
  # every level, but rather pass nodes on in a nested dictionary.
  _CreateXMLFile(comments,
                 'histograms',
                 non_obsolete_nodes,
                 output_base_dir,
                 filename=HISTOGRAMS_XML)

  # Recursively splits the histograms.xml file found in the |output_base_dir|.
  _SplitHistogramsRecursive(output_base_dir, 0)


if __name__ == '__main__':
  SplitIntoMultipleHistogramXMLs(base_dir='.',
                                 output_base_dir='./histograms_xml/')
