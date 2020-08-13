// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_MOCK_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_MOCK_H_

#include "chrome/browser/chromeos/borealis/borealis_installer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {
namespace test {

class BorealisInstallerMock : public BorealisInstaller {
 public:
  BorealisInstallerMock();
  ~BorealisInstallerMock();
  BorealisInstallerMock(const BorealisInstallerMock&) = delete;
  BorealisInstallerMock& operator=(const BorealisInstallerMock&) = delete;

  MOCK_METHOD(bool, IsProcessing, (), ());
  MOCK_METHOD(void, Start, (), ());
  MOCK_METHOD(void, Cancel, (), ());
};

}  // namespace test
}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_INSTALLER_MOCK_H_
