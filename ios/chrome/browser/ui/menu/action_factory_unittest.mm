// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/action_factory.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/test/ios/ui_image_test_utils.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(__IPHONE_13_0)

namespace {
MenuScenario kTestMenuScenario = MenuScenario::HistoryEntry;
}  // namespace

// Test fixture for the ActionFactory.
class ActionFactoryTest : public PlatformTest {
 protected:
  ActionFactoryTest() : test_title_(@"SomeTitle") {}

  // Creates a blue square.
  UIImage* CreateMockImage() {
    return ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(10, 10), [UIColor blueColor]);
  }

  web::WebTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  NSString* test_title_;
};

// Tests the creation of an action using the parameterized method, and verifies
// that the action has the right title and image.
TEST_F(ActionFactoryTest, CreateActionWithParameters) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

    UIImage* mockImage = CreateMockImage();

    UIAction* action = [factory actionWithTitle:test_title_
                                          image:mockImage
                                           type:MenuActionType::Copy
                                          block:^{
                                          }];

    EXPECT_TRUE([test_title_ isEqualToString:action.title]);
    EXPECT_EQ(mockImage, action.image);
  }
}

// Tests that the copy action has the right title and image.
TEST_F(ActionFactoryTest, CopyAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage systemImageNamed:@"doc.on.doc"];
    NSString* expectedTitle = l10n_util::GetNSString(IDS_IOS_COPY_ACTION_TITLE);

    GURL testURL = GURL("https://example.com");

    UIAction* action = [factory actionToCopyURL:testURL];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the delete action has the right title and image.
TEST_F(ActionFactoryTest, DeleteAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithScenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage systemImageNamed:@"trash"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE);

    UIAction* action = [factory actionToDeleteWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

#endif  // defined(__IPHONE_13_0)
