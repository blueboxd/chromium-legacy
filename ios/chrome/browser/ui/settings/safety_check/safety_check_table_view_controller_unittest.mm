// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_table_view_controller.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class SafetyCheckTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  ChromeTableViewController* InstantiateController() override {
    return [[SafetyCheckTableViewController alloc]
        initWithStyle:UITableViewStylePlain];
  }
};

// Tests PrivacyTableViewController is set up with all appropriate items
// and sections.
TEST_F(SafetyCheckTableViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  EXPECT_EQ(1, NumberOfSections());

  EXPECT_EQ(3, NumberOfItemsInSection(0));
}

}  // namespace
