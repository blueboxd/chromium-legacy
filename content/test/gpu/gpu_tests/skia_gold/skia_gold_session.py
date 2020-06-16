# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class for interacting with the Skia Gold image diffing service.

This is based heavily off Android's Skia Gold implementation in
//build/android/pylib/utils/gold_utils.py. If you need to make a change to this
file, check to see if the same change needs to be made there.
"""

import logging
import os
import subprocess
import sys
import tempfile

from gpu_tests import path_util

GOLDCTL_BINARY = os.path.join(path_util.GetChromiumSrcDir(), 'tools',
                              'skia_goldctl')
if sys.platform == 'win32':
  GOLDCTL_BINARY = os.path.join(GOLDCTL_BINARY, 'win', 'goldctl') + '.exe'
elif sys.platform == 'darwin':
  GOLDCTL_BINARY = os.path.join(GOLDCTL_BINARY, 'mac', 'goldctl')
else:
  GOLDCTL_BINARY = os.path.join(GOLDCTL_BINARY, 'linux', 'goldctl')


class SkiaGoldSession(object):
  class StatusCodes(object):
    """Status codes for RunComparison."""
    SUCCESS = 0
    AUTH_FAILURE = 1
    INIT_FAILURE = 2
    COMPARISON_FAILURE_REMOTE = 3
    COMPARISON_FAILURE_LOCAL = 4
    LOCAL_DIFF_FAILURE = 5

  class ComparisonResults(object):
    """Struct-like object for storing results of an image comparison."""

    def __init__(self):
      self.triage_link = None
      self.triage_link_omission_reason = None
      self.local_diff_given_image = None
      self.local_diff_closest_image = None
      self.local_diff_diff_image = None

  def __init__(self, working_dir, gold_properties, keys_file, corpus, instance):
    """A class to handle all aspects of an image comparison via Skia Gold.

    A single SkiaGoldSession is valid for a single instance/corpus/keys_file
    combination.

    Args:
      working_dir: The directory to store config files, etc.
      gold_properties: A skia_gold_properties.SkiaGoldProperties instance for
          the current test run.
      keys_file: A path to a JSON file containing various comparison config data
          such as corpus and debug information like the hardware/software
          configuration the images will be produced on.
      corpus: The corpus that images that will be compared belong to.
      instance: The name of the Skia Gold instance to interact with.
    """
    self._working_dir = working_dir
    self._gold_properties = gold_properties
    self._keys_file = keys_file
    self._corpus = corpus
    self._instance = instance
    self._triage_link_file = tempfile.NamedTemporaryFile(suffix='.txt',
                                                         dir=working_dir,
                                                         delete=False).name
    # A map of image name (string) to ComparisonResults for that image.
    self._comparison_results = {}
    self._authenticated = False
    self._initialized = False

  def RunComparison(self, name, png_file, use_luci=True):
    """Helper method to run all steps to compare a produced image.

    Handles authentication, itnitialization, comparison, and, if necessary,
    local diffing.

    Args:
      name: The name of the image being compared.
      png_file: A path to a PNG file containing the image to be compared.
      use_luci: If true, authentication will use the service account provided by
          the LUCI context. If false, will attempt to use whatever is set up in
          gsutil, which is only supported for local runs.

    Returns:
      A tuple (status, error). |status| is a value from
      SkiaGoldSession.StatusCodes signifying the result of the comparison.
      |error| is an error message describing the status if not successful.
    """
    auth_rc, auth_stdout = self.Authenticate(use_luci=use_luci)
    if auth_rc:
      return self.StatusCodes.AUTH_FAILURE, auth_stdout

    init_rc, init_stdout = self.Initialize()
    if init_rc:
      return self.StatusCodes.INIT_FAILURE, init_stdout

    compare_rc, compare_stdout = self.Compare(name=name, png_file=png_file)
    if not compare_rc:
      return self.StatusCodes.SUCCESS, None

    logging.error('Gold comparison failed: %s', compare_stdout)
    if not self._gold_properties.local_pixel_tests:
      return self.StatusCodes.COMPARISON_FAILURE_REMOTE, compare_stdout

    diff_rc, diff_stdout = self.Diff(name=name, png_file=png_file)
    if diff_rc:
      return self.StatusCodes.LOCAL_DIFF_FAILURE, diff_stdout
    return self.StatusCodes.COMPARISON_FAILURE_LOCAL, compare_stdout

  def Authenticate(self, use_luci=True):
    """Authenticates with Skia Gold for this session.

    Args:
      use_luci: If true, authentication will use the service account provided
          by the LUCI context. If false, will attempt to use whatever is set up
          in gsutil, which is only supported for local runs.

    Returns:
      A tuple (return_code, output). |return_code| is the return code of the
      authentication process. |output| is the stdout + stderr of the
      authentication process.
    """
    if self._authenticated:
      return 0, None
    if self._gold_properties.bypass_skia_gold_functionality:
      logging.warning('Not actually authenticating with Gold due to '
                      '--bypass-skia-gold-functionality being present.')
      return 0, None

    auth_cmd = [GOLDCTL_BINARY, 'auth', '--work-dir', self._working_dir]
    if use_luci:
      auth_cmd.append('--luci')
    elif not self._gold_properties.local_pixel_tests:
      raise RuntimeError(
          'Cannot authenticate to Skia Gold with use_luci=False unless running '
          'local pixel tests')

    rc, stdout = _RunCmdForRcAndOutput(auth_cmd)
    if rc == 0:
      self._authenticated = True
    return rc, stdout

  def Initialize(self):
    """Initializes the working directory if necessary.

    This can technically be skipped if the same information is passed to the
    command used for image comparison, but that is less efficient under the
    hood. Doing it that way effectively requires an initialization for every
    comparison (~250 ms) instead of once at the beginning.

    Returns:
      A tuple (return_code, output). |return_code| is the return code of the
      initialization process. |output| is the stdout + stderr of the
      initialization process.
    """
    if self._initialized:
      return 0, None
    if self._gold_properties.bypass_skia_gold_functionality:
      logging.warning('Not actually initializing Gold due to '
                      '--bypass-skia-gold-functionality being present.')
      return 0, None

    init_cmd = [
        GOLDCTL_BINARY,
        'imgtest',
        'init',
        '--passfail',
        '--instance',
        self._instance,
        '--corpus',
        self._corpus,
        '--keys-file',
        self._keys_file,
        '--work-dir',
        self._working_dir,
        '--failure-file',
        self._triage_link_file,
        '--commit',
        self._gold_properties.git_revision,
    ]
    if self._gold_properties.IsTryjobRun():
      init_cmd.extend([
          '--issue',
          str(self._gold_properties.issue),
          '--patchset',
          str(self._gold_properties.patchset),
          '--jobid',
          str(self._gold_properties.job_id),
          '--crs',
          str(self._gold_properties.code_review_system),
          '--cis',
          str(self._gold_properties.continuous_integration_system),
      ])

    rc, stdout = _RunCmdForRcAndOutput(init_cmd)
    if rc == 0:
      self._initialized = True
    return rc, stdout

  def Compare(self, name, png_file):
    """Compares the given image to images known to Gold.

    Triage links can later be retrieved using GetTriageLink().

    Args:
      name: The name of the image being compared.
      png_file: A path to a PNG file containing the image to be compared.

    Returns:
      A tuple (return_code, output). |return_code| is the return code of the
      comparison process. |output| is the stdout + stderr of the comparison
      process.
    """
    if self._gold_properties.bypass_skia_gold_functionality:
      logging.warning('Not actually comparing with Gold due to '
                      '--bypass-skia-gold-functionality being present.')
      return 0, None

    compare_cmd = [
        GOLDCTL_BINARY,
        'imgtest',
        'add',
        '--test-name',
        name,
        '--png-file',
        png_file,
        '--work-dir',
        self._working_dir,
    ]
    if self._gold_properties.local_pixel_tests:
      compare_cmd.append('--dryrun')

    self._ClearTriageLinkFile()
    rc, stdout = _RunCmdForRcAndOutput(compare_cmd)

    self._comparison_results[name] = self.ComparisonResults()
    if rc == 0:
      self._comparison_results[name].triage_link_omission_reason = (
          'Comparison succeeded, no triage link')
    elif self._gold_properties.IsTryjobRun():
      cl_triage_link = ('https://{instance}-gold.skia.org/search?'
                        'issue={issue}')
      cl_triage_link = cl_triage_link.format(instance=self._instance,
                                             issue=self._gold_properties.issue)
      self._comparison_results[name].triage_link = cl_triage_link
    else:
      try:
        with open(self._triage_link_file) as tlf:
          triage_link = tlf.read().strip()
        self._comparison_results[name].triage_link = triage_link
      except IOError:
        self._comparison_results[name].triage_link_omission_reason = (
            'Failed to read triage link from file')
    return rc, stdout

  def Diff(self, name, png_file):
    """Performs a local image diff against the closest known positive in Gold.

    This is used for running tests on a workstation, where uploading data to
    Gold for ingestion is not allowed, and thus the web UI is not available.

    Image links can later be retrieved using Get*ImageLink().

    Args:
      name: The name of the image being compared.
      png_file: The path to a PNG file containing the image to be diffed.

    Returns:
      A tuple (return_code, output). |return_code| is the return code of the
      diff process. |output| is the stdout + stderr of the diff process.
    """
    # Instead of returning that everything is okay and putting in dummy links,
    # just fail since this should only be called when running locally and
    # --bypass-skia-gold-functionality is only meant for use on the bots.
    if self._gold_properties.bypass_skia_gold_functionality:
      raise RuntimeError(
          '--bypass-skia-gold-functionality is not supported when running '
          'tests locally.')

    # We intentionally don't clean this up and don't put it in self._working_dir
    # since we need it to stick around after the test completes so the user
    # can look at its contents.
    output_dir = tempfile.mkdtemp()
    diff_cmd = [
        GOLDCTL_BINARY,
        'diff',
        '--corpus',
        self._corpus,
        '--instance',
        self._instance,
        '--input',
        png_file,
        '--test',
        name,
        '--work-dir',
        self._working_dir,
        '--out-dir',
        output_dir,
    ]
    rc, stdout = _RunCmdForRcAndOutput(diff_cmd)
    results = self._comparison_results.setdefault(name,
                                                  self.ComparisonResults())
    # The directory should contain "input-<hash>.png", "closest-<hash>.png",
    # and "diff.png".
    for f in os.listdir(output_dir):
      file_url = 'file://%s' % os.path.join(output_dir, f)
      if f.startswith('input-'):
        results.local_diff_given_image = file_url
      elif f.startswith('closest-'):
        results.local_diff_closest_image = file_url
      elif f == 'diff.png':
        results.local_diff_diff_image = file_url
    return rc, stdout

  def GetTriageLink(self, name):
    """Gets the triage link for the given image.

    Args:
      name: The name of the image to retrieve the triage link for.

    Returns:
      A string containing the triage link if it is available, or None if it is
      not available for some reason. The reason can be retrieved using
      GetTriageLinkOmissionReason.
    """
    return self._comparison_results.get(name,
                                        self.ComparisonResults()).triage_link

  def GetTriageLinkOmissionReason(self, name):
    """Gets the reason why a triage link is not available for an image.

    Args:
      name: The name of the image whose triage link does not exist.

    Returns:
      A string containing the reason why a triage link is not available.
    """
    if name not in self._comparison_results:
      return 'No image comparison performed for %s' % name
    results = self._comparison_results[name]
    # This method should not be called if there is a valid triage link.
    assert results.triage_link is None
    if results.triage_link_omission_reason:
      return results.triage_link_omission_reason
    if results.local_diff_given_image:
      return 'Gold only used to do a local image diff'
    raise RuntimeError(
        'Somehow have a ComparisonResults instance for %s that should not '
        'exist' % name)

  def GetGivenImageLink(self, name):
    """Gets the link to the given image used for local diffing.

    Args:
      name: The name of the image that was diffed.

    Returns:
      A string containing the link to where the image is saved, or None if it
      does not exist.
    """
    assert name in self._comparison_results
    return self._comparison_results[name].local_diff_given_image

  def GetClosestImageLink(self, name):
    """Gets the link to the closest known image used for local diffing.

    Args:
      name: The name of the image that was diffed.

    Returns:
      A string containing the link to where the image is saved, or None if it
      does not exist.
    """
    assert name in self._comparison_results
    return self._comparison_results[name].local_diff_closest_image

  def GetDiffImageLink(self, name):
    """Gets the link to the diff between the given and closest images.

    Args:
      name: The name of the image that was diffed.

    Returns:
      A string containing the link to where the image is saved, or None if it
      does not exist.
    """
    assert name in self._comparison_results
    return self._comparison_results[name].local_diff_diff_image

  def _ClearTriageLinkFile(self):
    """Clears the contents of the triage link file.

    This should be done before every comparison since goldctl appends to the
    file instead of overwriting its contents, which results in multiple triage
    links getting concatenated together if there are multiple failures.
    """
    open(self._triage_link_file, 'w').close()


def _RunCmdForRcAndOutput(cmd):
  try:
    output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
    return 0, output
  except subprocess.CalledProcessError as e:
    return e.returncode, e.output
