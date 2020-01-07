// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_legacy_view_controller.h"

#include "components/omnibox/browser/autocomplete_match.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class OmniboxPopupViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    popup_view_controller_ = [[OmniboxPopupLegacyViewController alloc] init];
  }

  OmniboxPopupLegacyViewController* popup_view_controller_;
};

TEST_F(OmniboxPopupViewControllerTest, HasTabMatch) {
  EXPECT_TRUE([popup_view_controller_
      conformsToProtocol:@protocol(UITableViewDataSource)]);
  id<UITableViewDataSource> datasource =
      (id<UITableViewDataSource>)popup_view_controller_;
  UITableView* table_view = [[UITableView alloc] init];

  // Check that if the match has a tab match, the cell's trailing button is
  // visible.
  AutocompleteMatch match;
  match.has_tab_match = true;
  AutocompleteMatchFormatter* formatter =
      [[AutocompleteMatchFormatter alloc] initWithMatch:match];
  [popup_view_controller_ updateMatches:@[ formatter ] withAnimation:NO];

  EXPECT_EQ([datasource tableView:table_view numberOfRowsInSection:0], 1);
  OmniboxPopupRow* cell = static_cast<OmniboxPopupRow*>([datasource
                  tableView:table_view
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]]);

  EXPECT_FALSE(cell.trailingButton.hidden);

  // Check that it is not the case if the tab match isn't visible.
  match.has_tab_match = false;
  formatter = [[AutocompleteMatchFormatter alloc] initWithMatch:match];
  [popup_view_controller_ updateMatches:@[ formatter ] withAnimation:NO];

  EXPECT_EQ([datasource tableView:table_view numberOfRowsInSection:0], 1);
  cell = static_cast<OmniboxPopupRow*>([datasource
                  tableView:table_view
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]]);

  EXPECT_TRUE(cell.trailingButton.hidden);
}

}  // namespace
