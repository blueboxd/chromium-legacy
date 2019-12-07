// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_sticky_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

class PrintPreviewStickySettingsUnittest : public testing::Test {
 public:
  PrintPreviewStickySettingsUnittest() = default;
  ~PrintPreviewStickySettingsUnittest() override = default;

  PrintPreviewStickySettingsUnittest(
      const PrintPreviewStickySettingsUnittest&) = delete;
  PrintPreviewStickySettingsUnittest& operator=(
      const PrintPreviewStickySettingsUnittest&) = delete;

 protected:
  PrintPreviewStickySettings sticky_settings_;
};

TEST_F(PrintPreviewStickySettingsUnittest, GetPrinterRecentlyUsedRanks) {
  std::string recently_used_ranks_str = R"({
    "version": 2,
    "recentDestinations": [
      {
        "id":"id1",
        "capabilities": {}
      },
      {
        "id": "id2",
        "origin": "chrome_os"
      }
    ]
  })";
  sticky_settings_.StoreAppState(recently_used_ranks_str);
  base::flat_map<std::string, int> recently_used_ranks =
      sticky_settings_.GetPrinterRecentlyUsedRanks();
  base::flat_map<std::string, int> expected_recently_used_ranks(
      {{"id1", 0}, {"id2", 1}});
  EXPECT_EQ(expected_recently_used_ranks, recently_used_ranks);
}

TEST_F(PrintPreviewStickySettingsUnittest,
       GetPrinterRecentlyUsedRanks_NoRecentDestinations) {
  std::string recently_used_ranks_str = R"({"version": 2})";
  sticky_settings_.StoreAppState(recently_used_ranks_str);
  EXPECT_TRUE(sticky_settings_.GetPrinterRecentlyUsedRanks().empty());
}

}  // namespace printing
