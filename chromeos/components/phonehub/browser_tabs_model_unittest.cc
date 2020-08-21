// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/browser_tabs_model.h"

#include "chromeos/components/phonehub/phone_model_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {

TEST(BrowserTabsModelTest, Initialization) {
  BrowserTabsModel success(/*is_tab_sync_enabled=*/true,
                           CreateFakeBrowserTabMetadata());
  EXPECT_TRUE(success.is_tab_sync_enabled());
  EXPECT_EQ(CreateFakeBrowserTabMetadata(), *success.most_recent_tab());
  EXPECT_FALSE(success.second_most_recent_tab().has_value());

  // If tab metadata is provided by tab sync is disabled, the data should be
  // cleared.
  BrowserTabsModel invalid_metadata(/*is_tab_sync_enabled=*/false,
                                    CreateFakeBrowserTabMetadata());
  EXPECT_FALSE(invalid_metadata.most_recent_tab().has_value());
}

}  // namespace phonehub
}  // namespace chromeos
