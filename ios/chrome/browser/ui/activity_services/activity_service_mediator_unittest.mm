// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_mediator.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/ui/activity_services/activities/bookmark_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/copy_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/find_in_page_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/generate_qr_code_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/print_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/reading_list_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/request_desktop_or_mobile_site_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/send_tab_to_self_activity.h"
#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"
#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_item_thumbnail_generator.h"
#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_url_source.h"
#import "ios/chrome/browser/ui/activity_services/data/share_image_data.h"
#import "ios/chrome/browser/ui/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#include "ios/web/common/user_agent.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@protocol
    HandlerProtocols <BrowserCommands, FindInPageCommands, QRGenerationCommands>
@end

class ActivityServiceMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    mocked_handler_ = OCMStrictProtocolMock(@protocol(HandlerProtocols));
    mocked_thumbnail_generator_ =
        OCMStrictClassMock([ChromeActivityItemThumbnailGenerator class]);

    mediator_ =
        [[ActivityServiceMediator alloc] initWithHandler:mocked_handler_
                                             prefService:pref_service_.get()
                                           bookmarkModel:nil];
  }

  void VerifyTypes(NSArray* activities, NSArray* expected_types) {
    EXPECT_EQ([expected_types count], [activities count]);
    for (unsigned int i = 0U; i < [activities count]; i++) {
      EXPECT_TRUE([activities[i] isKindOfClass:expected_types[i]]);
    }
  }

  id mocked_handler_;
  id mocked_thumbnail_generator_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  ActivityServiceMediator* mediator_;
};

// Tests that only one ChromeActivityURLSource is initialized from a ShareToData
// instance.
TEST_F(ActivityServiceMediatorTest, ActivityItemsForData_Success) {
  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("https://www.google.com/")
                                 visibleURL:GURL("https://google.com/")
                                      title:@"Some Title"
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_];

  NSArray<ChromeActivityURLSource*>* activityItems =
      [mediator_ activityItemsForData:data];

  EXPECT_EQ(1U, [activityItems count]);
}

// Tests that only the CopyActivity and PrintActivity get added for a page that
// is not HTTP or HTTPS.
TEST_F(ActivityServiceMediatorTest, ActivitiesForData_NotHTTPOrHTTPS) {
  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("chrome://chromium.org/")
                                 visibleURL:GURL("chrome://chromium.org/")
                                      title:@"baz"
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_];

  NSArray* activities = [mediator_ applicationActivitiesForData:data];

  // Verify activities' types.
  VerifyTypes(activities, @[ [CopyActivity class], [PrintActivity class] ]);
}

// Tests that the right activities are added in order for an HTTP page.
TEST_F(ActivityServiceMediatorTest, ActivitiesForData_HTTP) {
  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("http://example.com")
                                 visibleURL:GURL("http://example.com")
                                      title:@"baz"
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_];

  NSArray* activities = [mediator_ applicationActivitiesForData:data];

  // Verify activities' types.
  VerifyTypes(activities, @[
    [CopyActivity class], [SendTabToSelfActivity class],
    [ReadingListActivity class], [BookmarkActivity class],
    [GenerateQrCodeActivity class], [FindInPageActivity class],
    [RequestDesktopOrMobileSiteActivity class], [PrintActivity class]
  ]);
}

// Tests that the right activities are added in order for an HTTPS page.
TEST_F(ActivityServiceMediatorTest, ActivitiesForData_HTTPS) {
  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("https://example.com")
                                 visibleURL:GURL("https://example.com")
                                      title:@"baz"
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_];

  NSArray* activities = [mediator_ applicationActivitiesForData:data];

  // Verify activities' types.
  VerifyTypes(activities, @[
    [CopyActivity class], [SendTabToSelfActivity class],
    [ReadingListActivity class], [BookmarkActivity class],
    [GenerateQrCodeActivity class], [FindInPageActivity class],
    [RequestDesktopOrMobileSiteActivity class], [PrintActivity class]
  ]);
}

// Tests that only one ChromeActivityImageSource is initialized from a
// ShareIamgeData instance.
TEST_F(ActivityServiceMediatorTest, ActivityItemsForImageData_Success) {
  ShareImageData* data =
      [[ShareImageData alloc] initWithImage:[[UIImage alloc] init]
                                      title:@"some title"];

  NSArray<ChromeActivityImageSource*>* activityItems =
      [mediator_ activityItemsForImageData:data];

  EXPECT_EQ(1U, [activityItems count]);
}

// Tests that the right activities are added in order for an image.
TEST_F(ActivityServiceMediatorTest, ActivitiesForImageData) {
  ShareImageData* data =
      [[ShareImageData alloc] initWithImage:[[UIImage alloc] init]
                                      title:@"some title"];

  NSArray* activities = [mediator_ applicationActivitiesForImageData:data];

  // For now, we don't have any custom activities.
  EXPECT_EQ(0U, [activities count]);
}

// Tests that computing the list of excluded activities works for one item.
TEST_F(ActivityServiceMediatorTest, ExcludedActivityTypes) {
  ChromeActivityURLSource* activityURLSource = [[ChromeActivityURLSource alloc]
        initWithShareURL:[NSURL URLWithString:@"https://example.com"]
                 subject:@"Does not matter"
      thumbnailGenerator:mocked_thumbnail_generator_];

  NSSet* computedExclusion =
      [mediator_ excludedActivityTypesForItems:@[ activityURLSource ]];

  EXPECT_TRUE(
      [activityURLSource.excludedActivityTypes isEqualToSet:computedExclusion]);
}

TEST_F(ActivityServiceMediatorTest, ShareFinished_Success) {
  // Since mocked_handler_ is a strict mock, any call to its methods would make
  // the test fail. That is our success condition.
  NSString* copyActivityString = @"com.google.chrome.copyActivity";
  [mediator_ shareFinishedWithActivityType:copyActivityString
                                 completed:YES
                             returnedItems:@[]
                                     error:nil];
}

TEST_F(ActivityServiceMediatorTest, ShareFinished_Cancel) {
  // Since mocked_handler_ is a strict mock, any call to its methods would make
  // the test fail. That is our success condition.
  NSString* copyActivityString = @"com.google.chrome.copyActivity";
  [mediator_ shareFinishedWithActivityType:copyActivityString
                                 completed:NO
                             returnedItems:@[]
                                     error:nil];
}

TEST_F(ActivityServiceMediatorTest, ShareCancelled) {
  // Since mocked_handler_ is a strict mock, any call to its methods would make
  // the test fail. That is our success condition.
  [mediator_ shareFinishedWithActivityType:nil
                                 completed:NO
                             returnedItems:@[]
                                     error:nil];
}
