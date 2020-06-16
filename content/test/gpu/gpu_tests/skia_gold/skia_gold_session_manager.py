# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class for managing multiple SkiaGoldSessions.

This is vased heavily off Android's Skia Gold implementation in
//build/android/pylib/utils/gold_utils.py. If you need to make a change to this
file, check to see if the same change needs to be made there.
"""

import json
import tempfile

from gpu_tests.skia_gold import skia_gold_session

DEFAULT_INSTANCE = 'chrome-gpu'


class SkiaGoldSessionManager(object):
  def __init__(self, working_dir, gold_properties):
    """Class to manage one or more skia_gold_session.SkiaGoldSessions.

    A separate session is required for each instance/corpus/keys_file
    combination, so this class will lazily create them as necessary.

    Args:
      working_dir: The working directory under which each individual
          SkiaGoldSessions' working directory will be created.
      gold_properties: A SkiaGoldProperties instance that will be used to create
          any SkiaGoldSessions.
    """
    self._working_dir = working_dir
    self._gold_properties = gold_properties
    self._sessions = {}

  def GetSkiaGoldSession(self,
                         keys_dict,
                         corpus=None,
                         instance=DEFAULT_INSTANCE):
    """Gets a SkiaGoldSession for the given arguments.

    Lazily creates one if necessary.

    Args:
      keys_dict: A dictionary containing various comparison config data such as
          corpus and debug information like the hardware/software configuration
          the image was produced on.
      corpus: The corpus the session is for. If None, the corpus will be
          determined using available information.
      instance: The name of the Skia Gold instance to interact with.
    """
    keys_string = json.dumps(keys_dict, sort_keys=True)
    if corpus is None:
      corpus = keys_dict.get('source_type', instance)
    # Use the string representation of the keys JSON as a proxy for a hash since
    # dicts themselves are not hashable.
    session = self._sessions.setdefault(instance,
                                        {}).setdefault(corpus, {}).setdefault(
                                            keys_string, None)
    if not session:
      working_dir = tempfile.mkdtemp(dir=self._working_dir)
      keys_file = tempfile.NamedTemporaryFile(suffix='.json',
                                              dir=working_dir,
                                              delete=False).name
      with open(keys_file, 'w') as f:
        json.dump(keys_dict, f)
      session = skia_gold_session.SkiaGoldSession(working_dir,
                                                  self._gold_properties,
                                                  keys_file, corpus, instance)
      self._sessions[instance][corpus][keys_string] = session
    return session
