// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/privacy_container_view.h"

#include <memory>

#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/views/assistant/assistant_privacy_info_view.h"
#include "ash/app_list/views/suggested_content_info_view.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace test {

class PrivacyContainerViewTest : public views::ViewsTestBase {
 public:
  PrivacyContainerViewTest() = default;
  ~PrivacyContainerViewTest() override = default;
  PrivacyContainerViewTest(const PrivacyContainerViewTest&) = delete;
  PrivacyContainerViewTest& operator=(const PrivacyContainerViewTest&) = delete;

 protected:
  AppListTestViewDelegate* view_delegate() { return &view_delegate_; }
  PrivacyContainerView* view() { return view_.get(); }

  AssistantPrivacyInfoView* assistant_view() {
    return view_->assistant_privacy_info_view_;
  }
  SuggestedContentInfoView* suggested_content_view() {
    return view_->suggested_content_info_view_;
  }

  void CreateView() {
    view_ = std::make_unique<PrivacyContainerView>(&view_delegate_);
    view_->Update();
  }

 private:
  AppListTestViewDelegate view_delegate_;
  std::unique_ptr<PrivacyContainerView> view_;
};

TEST_F(PrivacyContainerViewTest, ShowAssistantPrivacyInfo) {
  view_delegate()->SetShouldShowAssistantPrivacyInfo(true);
  view_delegate()->SetShouldShowSuggestedContentInfo(false);
  CreateView();

  // Only Assistant privacy info should be visible.
  ASSERT_TRUE(assistant_view());
  EXPECT_TRUE(assistant_view()->GetVisible());
  EXPECT_FALSE(suggested_content_view() &&
               suggested_content_view()->GetVisible());
  EXPECT_EQ(view()->GetResultViewAt(0), assistant_view());

  // Disable Assistant privacy info.
  view_delegate()->SetShouldShowAssistantPrivacyInfo(false);
  view()->Update();

  EXPECT_FALSE(assistant_view()->GetVisible());
  EXPECT_FALSE(view()->GetResultViewAt(0));
}

TEST_F(PrivacyContainerViewTest, ShowSuggestedContentInfo) {
  view_delegate()->SetShouldShowAssistantPrivacyInfo(false);
  view_delegate()->SetShouldShowSuggestedContentInfo(true);
  CreateView();

  // Only Suggested Content info should be visible.
  ASSERT_TRUE(suggested_content_view());
  EXPECT_TRUE(suggested_content_view()->GetVisible());
  EXPECT_FALSE(assistant_view() && assistant_view()->GetVisible());
  EXPECT_EQ(view()->GetResultViewAt(0), suggested_content_view());

  // Disable Suggested Content info.
  view_delegate()->SetShouldShowSuggestedContentInfo(false);
  view()->Update();

  EXPECT_FALSE(suggested_content_view()->GetVisible());
  EXPECT_FALSE(view()->GetResultViewAt(0));
}

TEST_F(PrivacyContainerViewTest, AssistantInfoTakesPriority) {
  view_delegate()->SetShouldShowAssistantPrivacyInfo(true);
  view_delegate()->SetShouldShowSuggestedContentInfo(true);
  CreateView();

  // Only Assistant privacy info should be visible.
  ASSERT_TRUE(assistant_view());
  EXPECT_TRUE(assistant_view()->GetVisible());
  EXPECT_FALSE(suggested_content_view() &&
               suggested_content_view()->GetVisible());
  EXPECT_EQ(view()->GetResultViewAt(0), assistant_view());

  // If Assistant info is disabled, Suggested Content info should become
  // visible.
  view_delegate()->SetShouldShowAssistantPrivacyInfo(false);
  view()->Update();

  EXPECT_FALSE(assistant_view()->GetVisible());
  EXPECT_TRUE(suggested_content_view()->GetVisible());
  EXPECT_EQ(view()->GetResultViewAt(0), suggested_content_view());
}

}  // namespace test
}  // namespace ash
