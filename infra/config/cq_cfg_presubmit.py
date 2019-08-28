#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import difflib
import re
import os
import string
import sys


# Path to the root of the current chromium checkout.
CHROMIUM_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..',))


MD_HEADER = """# List of CQ builders

This page is auto generated using the script
//infra/config/cq_cfg_presubmit.py. Do not manually edit.

[TOC]

Each builder name links to that builder on Milo. The "Backing builders" links
point to the file used to determine which configurations a builder should copy
when running. These links might 404 or error; they are hard-coded right now,
using common assumptions about how builders are configured.
"""


REQUIRED_HEADER = """
These builders must pass before a CL may land."""


OPTIONAL_HEADER = """These builders optionally run, depending on the files in a
CL. For example, a CL which touches `//gpu/BUILD.gn` would trigger the builder
`android_optional_gpu_tests_rel`, due to the `location_regexp` values for that
builder."""


EXPERIMENTAL_HEADER = """
These builders are run on some percentage of builds. Their results are ignored
by CQ. These are often used to test new configurations before they are added
as required builders."""


BUILDER_VIEW_URL = (
    'https://ci.chromium.org/p/chromium/builders/luci.chromium.try/')


CODE_SEARCH_BASE = 'https://cs.chromium.org/'


TRYBOT_SOURCE_URL = CODE_SEARCH_BASE + 'search/?q=file:trybots.py+'


CQ_CONFIG_LOCATION_URL = (
    CODE_SEARCH_BASE + 'search/?q=package:%5Echromium$+file:commit-queue.cfg+')


REGEX_SEARCH_URL = CODE_SEARCH_BASE + 'search/?q=package:%5Echromium$+'


# Location regexps in commit-queue.cfg are expected to have this prefix.
REGEX_PREFIX = r'.+/[+]/'


def parse_text_proto_message(lines):
  """Parses a text proto. LOW QUALITY, MAY EASILY BREAK.

  If you really need to parse text protos, use the actual python library for
  protobufs. This exists because the .proto file for commit-queue.cfg lives in
  another repository.
  """
  data = {}

  linenum = 0
  # Tracks the current comment. Gets cleared if there's a blank line. Is added
  # to submessages, to allow for builders to contain comments.
  current_comment = None
  while linenum < len(lines):
    line = lines[linenum].strip()
    if not line:
      current_comment = None
      linenum += 1
    elif line.startswith('#'):
      if current_comment:
        current_comment += '\n' + line[1:]
      else:
        current_comment = line[1:]
      linenum += 1
    elif '{' in line:
      # Sub message. Put before the ':' clause so that it correctly handles one
      # line messages.
      end = linenum
      count = 0
      newlines = []
      while end < len(lines):
        inner_line = lines[end]
        if '{' in inner_line:
          count += 1
        if '}' in inner_line:
          count -= 1

        if end == linenum:
          newline = inner_line.split('{', 1)[1]
          if count == 0:
            newline = newline.split('}')[0]
          newlines.append(newline)
        elif count == 0:
          newlines.append(inner_line.split('}')[0])
        else:
          newlines.append(inner_line)
        end += 1
        if count == 0:
          break
      name = line.split('{')[0].strip()
      value = parse_text_proto_message(newlines)
      if current_comment:
        value['comment'] = current_comment
        current_comment = None

      if name in data:
        data[name].append(value)
      else:
        data[name] = [value]
      linenum = end
    elif ':' in line:
      # It's a field
      name, value = line.split(':', 1)
      value = value.strip()
      if value.startswith('"'):
        value = value.strip('"')

      if name in data:
        data[name].append(value)
      else:
        data[name] = [value]
      linenum += 1
    else:
      raise ValueError('Invalid line (number %d):\n%s' % (linenum, line))

  return data


class BuilderList(object):
  def __init__(self, builders):
    self.builders = builders

  def sort(self):
    """Sorts the builder list.

    Sorts the builders in place. Orders them into three groups: experimental,
    required, and optional."""
    self.builders.sort(key=lambda b: '%s|%s|%s' % (
        'z' if b.get('experiment_percentage') else 'a',
        'z' if b.get('location_regexp') else 'a',
        b['name']))

  def by_section(self):
    required = []
    experimental = []
    optional = []
    for b in self.builders:
      # Don't handle if something is both optional and experimental
      if b.get('location_regexp'):
        optional.append(b)
      elif b.get('experiment_percentage'):
        experimental.append(b)
      else:
        required.append(b)

    return required, optional, experimental


class CQConfig(object):
  def __init__(self, lines):
    parsed_value = parse_text_proto_message(lines)

    # Sanity check.
    assert len(parsed_value['config_groups']) == 2, (
        'Expected only two config group, found %d' % len(
            parsed_value['config_groups']))
    grp = parsed_value['config_groups'][0]
    gerrit = grp['gerrit'][0]
    name = gerrit['projects'][0]['name'][0]
    assert name == 'chromium/src', (
        'Expected first config group to be chromium src, got %s' % name)
    # The config group for chromium source refs/heads.
    self._config_group = grp

  @staticmethod
  def from_file(path):
    with open(path) as f:
      lines = f.readlines()

    return CQConfig(lines)

  def get_location_regexps(self):
    _, opt, _ = self.builder_list().by_section()
    for b in opt:
      if 'location_regexp' in b:
        for reg in b['location_regexp']:
          yield reg
      if 'location_regexp_exclude' in b:
        for reg in b['location_regexp_exclude']:
          yield reg

  def builder_list(self):
    """Returns a list of builders."""
    items = []
    for b in self._config_group['verifiers'][0]['tryjob'][0]['builders']:
      if not b['name'][0].startswith('chromium'):
        # Buildbot builders, just ignore.
        continue
      items.append(b)
    return BuilderList(items)

  def get_markdown_doc(self):
    lines = []
    for l in MD_HEADER.split('\n'):
      lines.append(l)

    bl = self.builder_list()
    req, opt, exp = bl.by_section()
    for title, header, builders in (
        ('Required builders', REQUIRED_HEADER, req),
        ('Optional builders', OPTIONAL_HEADER, opt),
        ('Experimental builders', EXPERIMENTAL_HEADER, exp),
    ):
      lines.append('## %s' % title)
      lines.append('')
      for l in header.strip().split('\n'):
        lines.append(l)
      lines.append('')
      for b in builders:
        buildername = b['name'][0].split('/')[-1]
        lines.append(
            '* [%s](%s) ([`commit-queue.cfg` entry](%s)) '
            '([matching builders](%s))' % (
            buildername, BUILDER_VIEW_URL + buildername,
            CQ_CONFIG_LOCATION_URL + b['name'][0],
            TRYBOT_SOURCE_URL + buildername))
        lines.append('')
        if 'comment' in b:
          for l in b['comment'].split('\n'):
            lines.append('  ' + l.strip())
          lines.append('')
        if 'location_regexp' in b:
          lines.append('  Path regular expressions:')
          for regex in b['location_regexp']:
            url = None
            if regex.startswith(REGEX_PREFIX):
              regex = regex[len(REGEX_PREFIX):]
            regex_title = '//' + regex.lstrip('/')
            if regex.endswith('.+'):
              regex = regex[:-len('.+')]
              if all(
                  # Equals sign and dashes used by layout tests.
                  c in string.ascii_letters + string.digits + '/-_='
                  for c in regex):
                # Assume the regex is targeting a single path, direct link to
                # it. Check to make sure we don't have weird characters, like
                # ()|, which could mean it's a regex.
                url = CODE_SEARCH_BASE + 'chromium/src/' + regex
            lines.append('    * [`%s`](%s)' % (
                regex_title, url or REGEX_SEARCH_URL + 'file:' + regex))
          lines.append('')
        if 'experiment_percentage' in b:
          lines.append('  * Experimental percentage: %s' % (
              b['experiment_percentage'][0]))
          lines.append('')
      lines.append('')

    return '\n'.join(lines)

def verify_location_regexps(regexps, verbose=True):
  # Verify that all the regexps listed in the file have files which they could
  # be triggered by. Failing this usually means they're old, and the code was
  # moved somewhere, like the webkit->blink rename.
  invalid_regexp = False
  for regexp in regexps:
    regexp = regexp.replace('\\\\', '')
    assert regexp.startswith(REGEX_PREFIX), (
        'location_regexp "%s" must start with "%s"' % (regexp, REGEX_PREFIX))
    regexp = regexp[len(REGEX_PREFIX):]
    # Split by path name, so that we don't have to run os.walk on the entire
    # source tree. commit-queue.cfg always uses '/' as the path separator.
    parts = regexp.split('/')
    # Dash and equal sign are used by layout tests.
    simple_name_re = re.compile(r'^[a-zA-Z0-9_\-=]*$')
    last_normal_path = 0
    while last_normal_path < len(parts):
      itm = parts[last_normal_path]
      if not simple_name_re.match(itm):
        break
      last_normal_path += 1
    path_to_search = (
        os.path.join(*parts[:last_normal_path]) if last_normal_path else '')
    # Simple case. Regexp is just referencing a single file. Just check if the
    # file exists.
    if path_to_search == os.path.join(*parts) and os.path.exists(
        os.path.join(CHROMIUM_DIR, path_to_search)):
      continue

    if os.path.sep != '/':
      # Regular expressions require backslashes to be escaped. Need to double
      # escape it, since the path itself has a double backslash.
      regexp = regexp.replace('/', '\\\\')
    compiled_regexp = re.compile(regexp)
    found = False
    for root, _, files in os.walk(os.path.join(CHROMIUM_DIR, path_to_search)):
      for fname in files:
        fullname = os.path.relpath(os.path.join(root, fname), CHROMIUM_DIR)
        if compiled_regexp.match(fullname):
          found = True
          break
      if found:
        break
    if not found:
      if verbose:
        print (
            'Regexp %s appears to have no valid files which could match it.' % (
                regexp))
      invalid_regexp = True

  return not invalid_regexp


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
    '-c', '--check', action='store_true', help=
    'Do consistency checks of commit-queue.cfg and generated files. Used '
    'during presubmit. Causes the tool to not generate any files.')
  args = parser.parse_args()

  exit_code = 0

  cfg = CQConfig.from_file(os.path.join(
      CHROMIUM_DIR, 'infra', 'config', 'commit-queue.cfg'))

  # Only force sorting on luci.chromium.try builders. Others should go away soon
  # anyways...
  bl = cfg.builder_list()
  assert len(bl.builders) > 0, (
      'Builders in \'luci.chromium.try\' bucket are missing somehow...')
  names = [b['name'][0] for b in bl.builders]
  bl.sort() # Changes the bl, so the next line is sorted.
  sorted_names = [b['name'][0] for b in bl.builders]
  if sorted_names != names:
    print 'ERROR: commit-queue.cfg is unsorted.',
    if args.check:
      print
    else:
      print ' Please sort as follows:'
      for line in difflib.unified_diff(
          names,
          sorted_names, fromfile='current', tofile='sorted'):
        print line
    exit_code = 1

  if args.check:
    if not verify_location_regexps(cfg.get_location_regexps()):
      exit_code = 1

    with open(os.path.join(
        CHROMIUM_DIR, 'docs', 'infra', 'cq_builders.md')) as f:
      if cfg.get_markdown_doc() != f.read():
        print (
            'Markdown file is out of date. Please run '
            '`//infra/config/cq_cfg_presubmit.py` to regenerate the '
            'docs.')
        exit_code = 1
  else:
    with open(os.path.join(
        CHROMIUM_DIR, 'docs', 'infra', 'cq_builders.md'), 'w') as f:
      f.write(cfg.get_markdown_doc())

  return exit_code


if __name__ == '__main__':
  sys.exit(main())
