// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_UNITTEST_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_UNITTEST_H_

#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

class DevOverrideTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
};

}  // namespace updater

#endif  // CHROME_UPDATER_EXTERNAL_CONSTANTS_UNITTEST_H_
