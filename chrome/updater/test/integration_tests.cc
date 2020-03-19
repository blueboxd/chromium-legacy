// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_tests.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace test {

class IntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Clean();
    ExpectClean();
  }

  void TearDown() override {
    ExpectClean();
    Clean();
  }
};

// TODO(crbug.com/1063064): Fix the test on Windows.
#if defined(OS_WIN)
#define MAYBE_InstallUninstall DISABLED_InstallUninstall
#else
#define MAYBE_InstallUninstall InstallUninstall
#endif
TEST_F(IntegrationTest, MAYBE_InstallUninstall) {
  Install();
  ExpectInstalled();
  Uninstall();
}

}  // namespace test

}  // namespace updater
