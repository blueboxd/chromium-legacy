// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_pref.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class IntentPickerAutoDisplayServiceTest : public testing::Test {
 public:
  IntentPickerAutoDisplayServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(IntentPickerAutoDisplayServiceTest, GetPlatform) {
  GURL url1("https://www.google.com/abcde");
  GURL url2("https://www.google.com/hi");
  GURL url3("https://www.boogle.com/a");

  TestingProfile profile;
  IntentPickerAutoDisplayService* service =
      IntentPickerAutoDisplayService::Get(&profile);

  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url1));
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url2));
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url3));

  // Update platform to a value and check value has updated.
  service->UpdatePlatformForTablets(
      url1, IntentPickerAutoDisplayPref::Platform::kArc);
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kArc,
            service->GetLastUsedPlatformForTablets(url1));
  // Url with the same host should also have updated value.
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kArc,
            service->GetLastUsedPlatformForTablets(url2));

  // Url with a different host should have original value.
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url3));

  // Update platform and check value has updated.
  service->UpdatePlatformForTablets(
      url2, IntentPickerAutoDisplayPref::Platform::kChrome);
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url1));
  // Url with the same host should also have updated value.
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url2));

  // Url with a different host should have original value.
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url3));

  // Update platform and check value has updated.
  service->UpdatePlatformForTablets(
      url3, IntentPickerAutoDisplayPref::Platform::kArc);
  // Url with different hosts should have original value.
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url1));
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url2));

  // Url value should be updated.
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kArc,
            service->GetLastUsedPlatformForTablets(url3));

  service->UpdatePlatformForTablets(
      url3, IntentPickerAutoDisplayPref::Platform::kNone);
  // Url value should be updated.
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kNone,
            service->GetLastUsedPlatformForTablets(url3));
  // Url with different hosts should have original value.
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url1));
  EXPECT_EQ(IntentPickerAutoDisplayPref::Platform::kChrome,
            service->GetLastUsedPlatformForTablets(url2));
}
