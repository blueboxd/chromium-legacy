# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from absl import flags

from chrome_ent_test.ent_tests import ChromeEnterpriseTestCase
from chrome_ent_test.infra.core import environment, before_all, test

FLAGS = flags.FLAGS


@environment(file="../policy_test.asset.textpb")
class BookmarkBarEnabledTest(ChromeEnterpriseTestCase):
  """Test the BookmarkBarEnabled

    https://www.chromium.org/administrators/policy-list-3#BookmarkBarEnabled.

    If this setting is left not set the user can decide to use this function
    or not.
    """

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.EnableUITest('client2012')

  def _getUIStructure(self, instance_name):
    local_dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunUITest(instance_name,
                            os.path.join(local_dir, 'bookmarkbar_webdriver.py'))
    return output

  @test
  def test_bookmark_bar_enabled(self):
    # Enable bookmark bar
    self.SetPolicy('win2012-dc', r'BookmarkBarEnabled', 1, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')
    logging.info('Enabled bookmark bar')

    output = self._getUIStructure('client2012')
    self.assertIn('Bookmarkbar is found', output)

  @test
  def test_bookmark_bar_disabled(self):
    # Disable bookmark bar
    self.SetPolicy('win2012-dc', r'BookmarkBarEnabled', 0, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')
    logging.info('Disabled bookmark bar')

    output = self._getUIStructure('client2012')
    self.assertIn('Bookmarkbar is missing', output)
