// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui_default.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestErrorUIDelegate : public extensions::ExtensionErrorUI::Delegate {
 public:
  // extensions::ExtensionErrorUI::Delegate:
  content::BrowserContext* GetContext() override { return &profile_; }
  const extensions::ExtensionSet& GetExternalExtensions() override {
    return external_;
  }
  const extensions::ExtensionSet& GetBlacklistedExtensions() override {
    return forbidden_;
  }
  void OnAlertDetails() override {}
  void OnAlertAccept() override {}
  void OnAlertClosed() override {}

  void InsertExternal(scoped_refptr<const extensions::Extension> ext) {
    external_.Insert(ext);
  }

  void InsertForbidden(scoped_refptr<const extensions::Extension> ext) {
    forbidden_.Insert(ext);
  }

 private:
  content::BrowserTaskEnvironment environment_;
  TestingProfile profile_;
  extensions::ExtensionSet external_;
  extensions::ExtensionSet forbidden_;
};

bool ContainsString(const base::string16& haystack, const std::string& needle) {
  base::string16 needle16 = base::UTF8ToUTF16(needle);
  return haystack.find(needle16) != haystack.npos;
}

}  // namespace

TEST(ExtensionErrorUIDefaultTest, BubbleMessageMentionsExtension) {
  TestErrorUIDelegate delegate;

  delegate.InsertExternal(extensions::ExtensionBuilder("Foo").Build());
  delegate.InsertForbidden(extensions::ExtensionBuilder("Bar").Build());
  delegate.InsertForbidden(extensions::ExtensionBuilder("Baz").Build());

  extensions::ExtensionErrorUIDefault ui(&delegate);
  GlobalErrorWithStandardBubble* bubble = ui.GetErrorForTesting();

  std::vector<base::string16> messages = bubble->GetBubbleViewMessages();

  ASSERT_EQ(3U, messages.size());
  EXPECT_TRUE(ContainsString(messages[0], "\"Foo\""));
  EXPECT_TRUE(ContainsString(messages[1], "\"Bar\""));
  EXPECT_TRUE(ContainsString(messages[2], "\"Baz\""));
}
