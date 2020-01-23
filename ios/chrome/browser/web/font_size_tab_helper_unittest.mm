// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/font_size_tab_helper.h"

#import <UIKit/UIKit.h>

#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using sync_preferences::PrefServiceMockFactory;
using sync_preferences::PrefServiceSyncable;
using user_prefs::PrefRegistrySyncable;

// Test fixture for FontSizeTabHelper class.
class FontSizeTabHelperTest : public PlatformTest {
 public:
  FontSizeTabHelperTest()
      : application_(OCMPartialMock([UIApplication sharedApplication])) {
    OCMStub([application_ preferredContentSizeCategory])
        .andDo(^(NSInvocation* invocation) {
          [invocation setReturnValue:&preferred_content_size_category_];
        });
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(CreatePrefService());
    chrome_browser_state_ = test_cbs_builder.Build();
    web_state_.SetBrowserState(chrome_browser_state_.get());
    FontSizeTabHelper::CreateForWebState(&web_state_);
  }
  ~FontSizeTabHelperTest() override { [application_ stopMocking]; }

  void SetPreferredContentSizeCategory(UIContentSizeCategory category) {
    preferred_content_size_category_ = category;
  }

  void SendUIContentSizeCategoryDidChangeNotification() {
    [NSNotificationCenter.defaultCenter
        postNotificationName:UIContentSizeCategoryDidChangeNotification
                      object:nil
                    userInfo:nil];
  }

  std::string ZoomMultiplierPrefKey(UIContentSizeCategory category, GURL url) {
    return base::StringPrintf(
        "%s.%s", base::SysNSStringToUTF8(category).c_str(), url.host().c_str());
  }

  std::unique_ptr<PrefServiceSyncable> CreatePrefService() {
    scoped_refptr<PrefRegistrySyncable> registry =
        base::MakeRefCounted<PrefRegistrySyncable>();
    // Registers Translate and Language related prefs.
    RegisterBrowserStatePrefs(registry.get());
    PrefServiceMockFactory factory;
    return factory.CreateSyncable(registry.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::TestWebState web_state_;
  UIContentSizeCategory preferred_content_size_category_ =
      UIContentSizeCategoryLarge;
  id application_ = nil;

  DISALLOW_COPY_AND_ASSIGN(FontSizeTabHelperTest);
};

// Tests that a web page's font size is set properly in a procedure started
// with default |UIApplication.sharedApplication.preferredContentSizeCategory|.
TEST_F(FontSizeTabHelperTest, PageLoadedWithDefaultFontSize) {
  std::string last_executed_js;

  // Load web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("", last_executed_js);

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(112)", last_executed_js);
  web_state_.ClearLastExecutedJavascript();

  // Reload web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(112)", last_executed_js);
}

// Tests that a web page's font size is set properly in a procedure started
// with special |UIApplication.sharedApplication.preferredContentSizeCategory|.
TEST_F(FontSizeTabHelperTest, PageLoadedWithExtraLargeFontSize) {
  std::string last_executed_js;
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;

  // Load web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(112)", last_executed_js);
  web_state_.ClearLastExecutedJavascript();

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(124)", last_executed_js);
  web_state_.ClearLastExecutedJavascript();

  // Reload web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(124)", last_executed_js);
}

// Tests that UMA log is sent when
// |UIApplication.sharedApplication.preferredContentSizeCategory| returns an
// unrecognizable category.
TEST_F(FontSizeTabHelperTest, PageLoadedWithUnrecognizableFontSize) {
  std::string last_executed_js;
  preferred_content_size_category_ = @"This is a new Category";

  // Load web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("", last_executed_js);

  // Change PreferredContentSizeCategory and send
  // UIContentSizeCategoryDidChangeNotification.
  preferred_content_size_category_ = UIContentSizeCategoryExtraExtraLarge;
  SendUIContentSizeCategoryDidChangeNotification();
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(124)", last_executed_js);
  web_state_.ClearLastExecutedJavascript();

  // Reload web page.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  last_executed_js = base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(124)", last_executed_js);
}

// Tests that the user can zoom in, and that after zooming in, the correct
// Javascript has been executed, and the zoom value is stored in the user pref
// under the correct key.
TEST_F(FontSizeTabHelperTest, ZoomIn) {
  GURL test_url("https://test.url/");
  web_state_.SetCurrentURL(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);
  font_size_tab_helper->UserZoom(ZOOM_IN);

  EXPECT_TRUE(font_size_tab_helper->CanUserZoomIn());

  std::string last_executed_js =
      base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(110)", last_executed_js);

  // Check that new zoom value is also saved in the pref under the right key.
  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(pref->FindDoublePath(pref_key), 1.1);
}

// Tests that the user can zoom out, and that after zooming out, the correct
// Javascript has been executed, and the zoom value is stored in the user pref
// under the correct key.
TEST_F(FontSizeTabHelperTest, ZoomOut) {
  GURL test_url("https://test.url/");
  web_state_.SetCurrentURL(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);
  font_size_tab_helper->UserZoom(ZOOM_OUT);

  EXPECT_TRUE(font_size_tab_helper->CanUserZoomOut());

  std::string last_executed_js =
      base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(90)", last_executed_js);

  // Check that new zoom value is also saved in the pref under the right key.
  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(pref->FindDoublePath(pref_key), 0.9);
}

// Tests after resetting the zoom level, the correct Javascript has been
// executed, and the value in the user prefs for the key has ben cleared.
TEST_F(FontSizeTabHelperTest, ResetZoom) {
  GURL test_url("https://test.url/");
  web_state_.SetCurrentURL(test_url);

  // First zoom in to setup the reset.
  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);
  font_size_tab_helper->UserZoom(ZOOM_IN);
  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(pref->FindDoublePath(pref_key), 1.1);

  // Then reset. The pref key should be removed from the dictionary.
  font_size_tab_helper->UserZoom(ZOOM_RESET);
  std::string last_executed_js =
      base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(100)", last_executed_js);
  EXPECT_FALSE(pref->FindDoublePath(pref_key));
}

// Tests that when the user changes the accessibility content size category and
// zooms, the resulting zoom level is the multiplication of both parts.
TEST_F(FontSizeTabHelperTest, ZoomAndAccessibilityTextSize) {
  preferred_content_size_category_ = UIContentSizeCategoryExtraLarge;

  GURL test_url("https://test.url/");
  web_state_.SetCurrentURL(test_url);

  FontSizeTabHelper* font_size_tab_helper =
      FontSizeTabHelper::FromWebState(&web_state_);
  font_size_tab_helper->UserZoom(ZOOM_IN);
  std::string pref_key =
      ZoomMultiplierPrefKey(preferred_content_size_category_, test_url);
  std::string last_executed_js =
      base::UTF16ToUTF8(web_state_.GetLastExecutedJavascript());
  // 1.12 from accessibility * 1.1 from zoom
  EXPECT_EQ("__gCrWeb.accessibility.adjustFontSize(123)", last_executed_js);
  // Only the user zoom portion is stored in the preferences.
  const base::Value* pref =
      chrome_browser_state_->GetPrefs()->Get(prefs::kIosUserZoomMultipliers);
  EXPECT_EQ(pref->FindDoublePath(pref_key), 1.1);
}
