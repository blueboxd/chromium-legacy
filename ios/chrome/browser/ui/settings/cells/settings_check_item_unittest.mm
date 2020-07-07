// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using SettingsCheckItemTest = PlatformTest;

// Tests that the text and detail text are honoured after a call to
// |configureCell:|.
TEST_F(SettingsCheckItemTest, ConfigureCell) {
  SettingsCheckItem* item = [[SettingsCheckItem alloc] initWithType:0];
  NSString* text = @"Test Text";
  NSString* detailText = @"Test Detail Text that can span multiple lines. For "
                         @"example, this line probably fits on three or four "
                         @"lines.";

  item.text = text;
  item.detailText = detailText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[SettingsCheckCell class]]);

  SettingsCheckCell* CheckCell =
      base::mac::ObjCCastStrict<SettingsCheckCell>(cell);
  EXPECT_FALSE(CheckCell.textLabel.text);
  EXPECT_FALSE(CheckCell.detailTextLabel.text);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, CheckCell.textLabel.text);
  EXPECT_NSEQ(detailText, CheckCell.detailTextLabel.text);
}

}  // namespace
