# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from chrome_ent_test.infra.core import environment, before_all, test
from infra import ChromeEnterpriseTestCase


@environment(file="../policy_test.asset.textpb")
class ExtensionInstallForcelistTest(ChromeEnterpriseTestCase):
  """Test the ExtensionInstallForcelist policy.

  See https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ExtensionInstallForcelist"""

  # This is the extension id of the Google Keep extension.
  ExtensionId = 'lpcaedmchfhocbbapmcbpinfpgnhiddi'

  @before_all
  def setup(self):
    self.InstallChrome('client2019')
    self.InstallWebDriver('client2019')

  def isExtensionInstalled(self, incognito=False):
    dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        'client2019',
        os.path.join(dir, 'is_extension_installed.py'),
        args=["--extension_id", ExtensionInstallForcelistTest.ExtensionId])

    if "ERROR" in output:
      raise Exception(
          "is_extension_installed.py returned an error: %s" % output)

    return "TRUE" in output

  @test
  def test_NoForcelistNoExtensionInstalled(self):
    self.SetPolicy('win2019-dc', r'ExtensionInstallForcelist\1', '""', 'String')
    self.RunCommand('client2019', 'gpupdate /force')

    installed = self.isExtensionInstalled()
    self.assertFalse(installed)

  @test
  def test_ForcelistExtensionInstalled(self):
    url = 'https://clients2.google.com/service/update2/crx'
    extension = '"%s;%s"' % (ExtensionInstallForcelistTest.ExtensionId, url)
    self.SetPolicy('win2019-dc', r'ExtensionInstallForcelist\1', extension,
                   'String')
    self.RunCommand('client2019', 'gpupdate /force')

    installed = self.isExtensionInstalled()
    self.assertTrue(installed)
