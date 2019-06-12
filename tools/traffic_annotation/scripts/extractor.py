#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Extracts network traffic annotation definitions from C++ source code.
"""

import argparse
import os
import re
import sys

from annotation_tools import NetworkTrafficAnnotationTools
from annotation_tokenizer import Tokenizer

ANNOTATION_TYPES = {
    'DefineNetworkTrafficAnnotation': 'Definition',
    'DefinePartialNetworkTrafficAnnotation': 'Partial',
    'CompleteNetworkTrafficAnnotation': 'Completing',
    'BranchedCompleteNetworkTrafficAnnotation': 'BranchedCompleting',
    # TODO(crbug/966883): Add 'Mutable' type.
}


# Regex that matches an annotation definition.
CALL_DETECTION_REGEX = re.compile(r'''
  \b
  # Look for one of the tracked function names.
  # Capture group 1.
  (
    ''' + ('|'.join(ANNOTATION_TYPES.keys())) + r'''
  )
  # Followed by a left-paren.
  \s*
  \(
''', re.VERBOSE | re.DOTALL)


class Annotation:
  """A network annotation definition in C++ code."""

  def __init__(self, file_path, re_match):
    """Parses the annotation and populates object fields.

    Args:
      file_path: Path to the file that contains this annotation.
      re_match: A MatchObject obtained from CALL_DETECTION_REGEX.
    """
    self.file_path = file_path
    # TODO(crbug/966883): Either find a way to detect parent function name, or
    # (more likely) remove function_name-related code from the auditor.
    self.function_name = "XXX_UNIMPLEMENTED_XXX"
    self.line_number = get_line_number_at(re_match.string, re_match.start())

    definition_function = re_match.group(1)
    self.type_name = ANNOTATION_TYPES[definition_function]

    # These are populated by _parse_body().
    self.unique_id = ''
    self.extra_id = ''
    self.text = ''

    # Parse the arguments given to the definition function, populating
    # |unique_id|, |text| and (possibly) |extra_id|.
    body = re_match.string[re_match.end():]
    self._parse_body(body)

  def clang_tool_output_string(self):
    """Returns a string formatted for clang-tool-style output."""
    return "\n".join(map(str, [
        "==== NEW ANNOTATION ====",
        self.file_path,
        self.function_name,
        self.line_number,
        self.type_name,
        self.unique_id,
        self.extra_id,
        self.text,
        "==== ANNOTATION ENDS ====",
    ]))

  def _parse_body(self, body):
    """Tokenizes and parses the arguments given to the definition function."""
    tokenizer = Tokenizer(body, self.file_path, self.line_number)

    # unique_id
    self.unique_id = tokenizer.advance('string_literal')
    tokenizer.advance('comma')

    # extra_id (Partial/BranchedCompleting)
    if self.type_name == 'Partial' or self.type_name == 'BranchedCompleting':
      self.extra_id = tokenizer.advance('string_literal')
      tokenizer.advance('comma')

    # partial_annotation (Completing/BranchedCompleting)
    if self.type_name == 'Completing' or self.type_name == 'BranchedCompleting':
      # Skip the |partial_annotation| argument. It can be a variable_name, or a
      # FunctionName(), so skip the parentheses if they're there.
      tokenizer.advance('symbol')
      if tokenizer.maybe_advance('left_paren'):
        tokenizer.advance('right_paren')
      tokenizer.advance('comma')

    # proto text
    self.text = tokenizer.advance('string_literal')

    # The function call should end here without any more arguments.
    assert tokenizer.advance('right_paren')


def get_line_number_at(string, pos):
  """Find the line number for the char at position |pos|. 1-indexed."""
  # This is inefficient: O(n). But we only run it once for each annotation
  # definition, so the effect on total runtime is negligible.
  return 1 + len(re.compile(r'\n').findall(string[:pos]))


def is_inside_comment(string, pos):
  """Checks if the position |pos| within string seems to be inside a comment.

  This is a bit naive. Only checks for single-line comments (// ...), not block
  comments (/* ...  */).

  Args:
    string: string to scan.
    pos: position within the string.

  Returns:
    True if |string[pos]| looks like it's inside a C++ comment.
  """
  # Look for "//" on the same line in the reversed string.
  return bool(re.match(r'[^\n]*//', string[pos::-1]))
  # TODO(crbug/966883): Add multi-line comment support.


def extract_annotations(file_path):
  """Extracts and returns annotations from the file at |file_path|."""
  with open(file_path) as f:
    contents = f.read()

  defs = []
  for re_match in CALL_DETECTION_REGEX.finditer(contents):
    if is_inside_comment(re_match.string, re_match.start()):
      continue
    defs.append(Annotation(file_path, re_match))
  return defs


def main():
  parser = argparse.ArgumentParser(
      description="Network Traffic Annotation Extractor.")
  parser.add_argument(
      '--build-path',
      help='Specifies a compiled build directory, e.g. out/Debug.')
  parser.add_argument(
      '--no-filter', action='store_true',
      help='Do not filter files based on compdb entries')
  parser.add_argument(
      'file_paths', nargs='+', help='List of files to process.')

  args = parser.parse_args()

  tools = NetworkTrafficAnnotationTools(args.build_path)
  compdb_files = tools.GetCompDBFiles()

  annotation_definitions = []

  # Parse all the files.
  # TODO(crbug/966883): Do this in parallel.
  for file_path in args.file_paths:
    if not args.no_filter and os.path.abspath(file_path) not in compdb_files:
      continue
    annotation_definitions.extend(extract_annotations(file_path))

  # Print output.
  for annotation in annotation_definitions:
    print(annotation.clang_tool_output_string())

  return 0


if '__main__' == __name__:
  sys.exit(main())
