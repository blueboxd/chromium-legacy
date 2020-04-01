// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/permissions/test/mock_permission_request.h"

using PermissionPromptBubbleViewTest = ChromeViewsTestBase;

namespace {

class TestDelegate : public permissions::PermissionPrompt::Delegate {
 public:
  TestDelegate(const GURL& origin, const std::vector<std::string> names) {
    std::transform(
        names.begin(), names.end(), std::back_inserter(requests_),
        [&](auto& name) {
          return std::make_unique<permissions::MockPermissionRequest>(
              name, permissions::PermissionRequestType::UNKNOWN, origin);
        });
    std::transform(requests_.begin(), requests_.end(),
                   std::back_inserter(raw_requests_),
                   [](auto& req) { return req.get(); });
  }

  const std::vector<permissions::PermissionRequest*>& Requests() override {
    return raw_requests_;
  }

  void Accept() override {}
  void Deny() override {}
  void Closing() override {}

 private:
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests_;
  std::vector<permissions::PermissionRequest*> raw_requests_;
};

TEST_F(PermissionPromptBubbleViewTest, AccessibleTitleMentionsPermissions) {
  TestDelegate delegate(GURL("https://test.origin"), {"foo", "bar"});
  auto bubble =
      std::make_unique<PermissionPromptBubbleView>(nullptr, &delegate);

  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "foo",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "bar",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
}

TEST_F(PermissionPromptBubbleViewTest, AccessibleTitleMentionsOrigin) {
  TestDelegate delegate(GURL("https://test.origin"), {"foo", "bar"});
  auto bubble =
      std::make_unique<PermissionPromptBubbleView>(nullptr, &delegate);

  // Note that the scheme is not usually included.
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "test.origin",
                      base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle()));
}

TEST_F(PermissionPromptBubbleViewTest,
       AccessibleTitleDoesNotMentionTooManyPermissions) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {"foo", "bar", "baz", "quxx"});
  auto bubble =
      std::make_unique<PermissionPromptBubbleView>(nullptr, &delegate);

  const auto title = base::UTF16ToUTF8(bubble->GetAccessibleWindowTitle());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "foo", title);
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "bar", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "baz", title);
  EXPECT_PRED_FORMAT2(::testing::IsNotSubstring, "quxx", title);
}
}  // namespace
