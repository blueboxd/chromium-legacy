// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dark_mode_handler.h"

#include "base/test/scoped_feature_list.h"
#include "base/token.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/test_native_theme.h"

class TestDarkModeHandler : public DarkModeHandler {
 public:
  using DarkModeHandler::InitializeInternal;
};

class DarkModeHandlerTest : public testing::Test {
 public:
  DarkModeHandlerTest()
      : source_(content::TestWebUIDataSource::Create(
            base::Token::CreateRandom().ToString())) {
    // We use a random source_name here as calling Add() can replace existing
    // sources with the same name (which might destroy the memory addressed by
    // |source()->GetWebUIDataSource()|.
    content::WebUIDataSource::Add(&profile_, source()->GetWebUIDataSource());
  }

  content::TestBrowserThreadBundle* bundle() { return &bundle_; }
  content::TestWebUI* web_ui() { return &web_ui_; }
  ui::TestNativeTheme* theme() { return &theme_; }
  base::test::ScopedFeatureList* features() { return &features_; }
  content::TestWebUIDataSource* source() { return source_.get(); }

  void InitializeHandler() {
    TestDarkModeHandler::InitializeInternal(
        web_ui(), source()->GetWebUIDataSource(), theme(), &profile_);
  }

  bool IsSourceDark() {
    const auto* replacements = source()->GetReplacements();
    const auto dark_it = replacements->find("dark");

    if (dark_it == replacements->end()) {
      ADD_FAILURE();
      return false;
    }

    return dark_it->second == "dark";
  }

 private:
  content::TestBrowserThreadBundle bundle_;
  TestingProfile profile_;
  base::test::ScopedFeatureList features_;
  content::TestWebUI web_ui_;
  ui::TestNativeTheme theme_;
  std::unique_ptr<content::TestWebUIDataSource> source_;
};

TEST_F(DarkModeHandlerTest, WebUIDarkModeDisabledLightMode) {
  features()->InitAndDisableFeature(features::kWebUIDarkMode);
  theme()->SetDarkMode(false);

  InitializeHandler();

  EXPECT_EQ(web_ui()->GetHandlersForTesting()->size(), 1u);
  EXPECT_EQ(web_ui()->call_data().size(), 0u);

  EXPECT_FALSE(IsSourceDark());
}

TEST_F(DarkModeHandlerTest, WebUIDarkModeDisabledDarkMode) {
  features()->InitAndDisableFeature(features::kWebUIDarkMode);
  theme()->SetDarkMode(true);

  InitializeHandler();

  EXPECT_EQ(web_ui()->GetHandlersForTesting()->size(), 1u);
  EXPECT_EQ(web_ui()->call_data().size(), 0u);

  // Even if in dark mode, if the feature's disabled we shouldn't be telling the
  // page to be dark.
  EXPECT_FALSE(IsSourceDark());
}

TEST_F(DarkModeHandlerTest, WebUIDarkModeDisabledNoNotifications) {
  features()->InitAndDisableFeature(features::kWebUIDarkMode);
  theme()->SetDarkMode(false);

  InitializeHandler();

  EXPECT_EQ(web_ui()->GetHandlersForTesting()->size(), 1u);
  EXPECT_EQ(web_ui()->call_data().size(), 0u);
  EXPECT_FALSE(IsSourceDark());

  theme()->SetDarkMode(true);
  theme()->NotifyObservers();

  EXPECT_EQ(web_ui()->call_data().size(), 0u);
  EXPECT_FALSE(IsSourceDark());
}

TEST_F(DarkModeHandlerTest, WebUIDarkModeEnabledInLightMode) {
  features()->InitAndEnableFeature(features::kWebUIDarkMode);
  theme()->SetDarkMode(false);

  InitializeHandler();

  EXPECT_EQ(web_ui()->GetHandlersForTesting()->size(), 1u);
  EXPECT_EQ(web_ui()->call_data().size(), 0u);

  EXPECT_FALSE(IsSourceDark());
}

TEST_F(DarkModeHandlerTest, WebUIDarkModeEnabledInDarkMode) {
  features()->InitAndEnableFeature(features::kWebUIDarkMode);
  theme()->SetDarkMode(true);

  InitializeHandler();

  EXPECT_EQ(web_ui()->GetHandlersForTesting()->size(), 1u);
  EXPECT_EQ(web_ui()->call_data().size(), 0u);

  EXPECT_TRUE(IsSourceDark());
}

TEST_F(DarkModeHandlerTest, NotifiesAndUpdatesOnChange) {
  features()->InitAndEnableFeature(features::kWebUIDarkMode);
  theme()->SetDarkMode(false);

  InitializeHandler();

  ASSERT_EQ(web_ui()->GetHandlersForTesting()->size(), 1u);
  EXPECT_EQ(web_ui()->call_data().size(), 0u);

  EXPECT_FALSE(IsSourceDark());

  web_ui()->HandleReceivedMessage("observeDarkMode", /*args=*/nullptr);
  EXPECT_EQ(web_ui()->call_data().size(), 0u);

  // Theme changes unrelated to dark mode should not notify.
  theme()->NotifyObservers();
  EXPECT_EQ(web_ui()->call_data().size(), 0u);

  theme()->SetDarkMode(true);
  theme()->NotifyObservers();
  bundle()->RunUntilIdle();

  EXPECT_TRUE(IsSourceDark());
  ASSERT_EQ(web_ui()->call_data().size(), 1u);

  EXPECT_STREQ(web_ui()->call_data()[0]->function_name().c_str(),
               "cr.webUIListenerCallback");

  ASSERT_TRUE(web_ui()->call_data()[0]->arg1());
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->is_string());
  EXPECT_STREQ(web_ui()->call_data()[0]->arg1()->GetString().c_str(),
               "dark-mode-changed");

  ASSERT_TRUE(web_ui()->call_data()[0]->arg2());
  ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->is_bool());
  EXPECT_TRUE(web_ui()->call_data()[0]->arg2()->GetBool());
}

TEST_F(DarkModeHandlerTest, DarkModeChangeBeforeJsAllowed) {
  features()->InitAndEnableFeature(features::kWebUIDarkMode);
  theme()->SetDarkMode(false);

  InitializeHandler();

  EXPECT_EQ(web_ui()->GetHandlersForTesting()->size(), 1u);
  EXPECT_EQ(web_ui()->call_data().size(), 0u);
  EXPECT_FALSE(IsSourceDark());

  theme()->SetDarkMode(true);
  theme()->NotifyObservers();

  EXPECT_EQ(web_ui()->call_data().size(), 0u);

  bundle()->RunUntilIdle();
  EXPECT_TRUE(IsSourceDark());
}

TEST_F(DarkModeHandlerTest, DarkModeChangeWhileJsDisallowed) {
  features()->InitAndEnableFeature(features::kWebUIDarkMode);
  theme()->SetDarkMode(false);

  InitializeHandler();

  EXPECT_EQ(web_ui()->GetHandlersForTesting()->size(), 1u);
  EXPECT_EQ(web_ui()->call_data().size(), 0u);
  EXPECT_FALSE(IsSourceDark());

  web_ui()->HandleReceivedMessage("observeDarkMode", /*args=*/nullptr);
  web_ui()->GetHandlersForTesting()->front()->DisallowJavascript();

  theme()->SetDarkMode(true);
  theme()->NotifyObservers();

  EXPECT_EQ(web_ui()->call_data().size(), 0u);

  bundle()->RunUntilIdle();
  EXPECT_TRUE(IsSourceDark());
}
