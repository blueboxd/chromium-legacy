# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for extracting email addresses from OWNERS files."""

import histogram_ownership
import os
import re

_EMAIL_PATTERN = r'^[\w\-\+\%\.]+\@[\w\-\+\%\.]+$'
_SRC = 'src/'


class Error(Exception):
  pass


def _IsWellFormattedFilePath(path):
  """Returns True if the given path begins with 'src/' and ends with 'OWNERS'.

  Args:
    path: The path to an OWNERS file, e.g. 'src/gin/OWNERS'.
  """
  return path.startswith(_SRC) and path.endswith('OWNERS')


def _GetOwnersFilePath(path):
  """Returns an absolute path that can be opened.

  Args:
    path: A well-formatted path to an OWNERS file, e.g. 'src/courgette/OWNERS'.

  Raises:
    Error: Raised if the given path is not well-formatted.
  """
  if _IsWellFormattedFilePath(path):
    # Four '..' are used because calling dirname() yields the path to this
    # module's directory, histograms, and the chromium directory is up four
    # directory levels from the histograms directory.
    path_to_chromium_directory = [
        os.path.dirname(__file__), '..', '..', '..', '..']
    return os.path.abspath(
        os.path.join(*(path_to_chromium_directory + path.split('/'))))
  else:
    raise Error('The given path {} is not well-formatted.'
                'Well-formatted paths begin with "src/" and end with "OWNERS"'
                .format(path))


def _ExtractEmailAddressesFromOWNERS(path, depth=0):
  """Returns a list of email addresses in the given file.

  Args:
    path: The path to an OWNERS file.
    depth: The depth of the recursion, which is used to fail fast in the rare
      case that the OWNERS file path results in a loop.

  Raises:
    Error: Raised in two situations. First, raised if (A) the OWNERS file with
      the given path has a file directive and (B) the OWNERS file indicated by
      the directive does not exist. Second, raised if the depth reaches a
      certain limit.
  """
  # It is unlikely that any chain of OWNERS files will exceed 10 redirections
  # via file:// directives.
  limit = 10
  if (depth > limit):
    raise Error('_ExtractEmailAddressesFromOWNERS has been called {} times. The'
                ' path {} may be part of an OWNERS loop.'.format(limit, path))

  directive = 'file://'
  email_pattern = re.compile(_EMAIL_PATTERN)
  extracted_emails = []

  with open(path, 'r') as owners_file:
    for line in [line.lstrip()
                 for line in owners_file.read().splitlines() if line]:
      index = line.find(' ')
      first_word = line[:index] if index != -1 else line

      if email_pattern.match(first_word):
        extracted_emails.append(first_word)

      elif first_word.startswith(directive):
        next_path = _GetOwnersFilePath(
          os.path.join(_SRC, first_word[len(directive):]))

        if os.path.exists(next_path) and os.path.isfile(next_path):
          extracted_emails.extend(
              _ExtractEmailAddressesFromOWNERS(next_path, depth + 1))
        else:
          raise Error('The path derived from {} does not exist. '
                      'Derived path: {}'.format(first_word, next_path))

  return extracted_emails


def _MakeOwners(document, path, emails_with_dom_elements):
  """Makes DOM Elements for owners and returns the elements.

  The owners are extracted from the OWNERS file with the given path and
  deduped using the given set emails_with_dom_elements. This set has email
  addresses that were explicitly listed as histogram owners, e.g.
  <owner>liz@chromium.org</owner>. If a histogram has multiple OWNERS file
  paths, e.g. <owner>src/cc/OWNERS</owner> and <owner>src/ui/OWNERS</owner>,
  then the given set also contains any email addresses that have already been
  extracted from OWNERS files.

  New owners that are extracted from the given file are also added to
  emails_with_dom_elements.

  Args:
    document: The Document to which the new owners elements will belong.
    path: The absolute path to an OWNERS file.
    emails_with_dom_elements: The set of email addresses that already have
      corresponding DOM Elements.

  Returns:
    A collection of DOM Elements made from owners in the given OWNERS file.
  """
  owner_elements = []
  # TODO(crbug.com/987709): An OWNERS file API would be ideal.
  emails_from_owners_file = _ExtractEmailAddressesFromOWNERS(path)

  # A list is used to respect the order of email addresses in the OWNERS file.
  deduped_emails_from_owners_file = []
  for email in emails_from_owners_file:
    if email not in emails_with_dom_elements:
      deduped_emails_from_owners_file.append(email)
      emails_with_dom_elements.add(email)

  for email in deduped_emails_from_owners_file:
    owner_element = document.createElement('owner')
    owner_element.appendChild(document.createTextNode(email))
    owner_elements.append(owner_element)
  return owner_elements


def _UpdateHistogram(histogram, owner_to_replace, owners_to_add):
  """Updates the histogram by replacing owner_to_replace with owners_to_add.

  Args:
    histogram: The DOM Element to update.
    owner: The DOM Element to be replaced. This is a child node of histogram,
      and its text is a file path to an OWNERS file, e.g. 'src/mojo/OWNERS'
    owners_to_add: A collection of DOM Elements with which to replace
      owner_to_replace.
  """
  node_after_owners_file = owner_to_replace.nextSibling
  replacement_done = False
  new_line_plus_indent = '\n  '

  for owner_to_add in owners_to_add:
    if not replacement_done:
      histogram.replaceChild(owner_to_add, owner_to_replace)
      replacement_done = True
    else:
      histogram.insertBefore(
          histogram.ownerDocument.createTextNode(new_line_plus_indent),
          node_after_owners_file)
      histogram.insertBefore(owner_to_add, node_after_owners_file)


def ExpandHistogramsOWNERS(histograms):
  """Updates the given DOM Element's descendants, if necessary.

  The owner nodes associated with a single histogram need to be updated when
  the text of an owner node is the path to an OWNERS file rather than an email
  address, e.g. <owner>src/base/android/OWNERS</owner> instead of
  <owner>joy@chromium.org</owner>.

  If the text of an owner node is an OWNERS file path, then this node is
  replaced by owner nodes for the emails derived from the OWNERS file.

  Args:
    histograms: The DOM Element whose descendants may be updated.

  Raises:
    Error: Raised if the OWNERS file with the given path does not exist.
  """
  email_pattern = re.compile(_EMAIL_PATTERN)

  for histogram in histograms.getElementsByTagName('histogram'):
    owners = histogram.getElementsByTagName('owner')

    # owner is a DOM Element with a single child, which is a DOM Text node.
    emails_with_dom_elements = set([
        owner.childNodes[0].data
        for owner in owners
        if email_pattern.match(owner.childNodes[0].data)])

    for index in range(len(owners)):
      owner = owners[index]
      owner_text = owner.childNodes[0].data
      is_email = email_pattern.match(owner_text)

      is_primary_owner = (is_email or
          owner_text == histogram_ownership.DUMMY_OWNER)
      if index == 0 and not is_primary_owner:
        raise Error('The histogram {} must have a primary owner, i.e. an '
                    'individual\'s email address.'
                    .format(histogram.getAttribute('name')))

      if not is_primary_owner:
        path = _GetOwnersFilePath(owner_text)
        if os.path.exists(path) and os.path.isfile(path):
          owners_to_add = _MakeOwners(
              owner.ownerDocument, path, emails_with_dom_elements)
          if owners_to_add:
            _UpdateHistogram(histogram, owner, owners_to_add)
          else:
            raise Error('No email addresses could be derived from {}.'
                        .format(path))
        else:
          raise Error('The path {} does not exist.'.format(path))
