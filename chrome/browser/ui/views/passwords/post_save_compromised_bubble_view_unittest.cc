// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/post_save_compromised_bubble_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"

namespace {
using testing::Return;

class PostSaveCompromisedBubbleViewTest : public PasswordBubbleViewTestBase {
 public:
  PostSaveCompromisedBubbleViewTest() = default;
  ~PostSaveCompromisedBubbleViewTest() override = default;

  void CreateViewAndShow(password_manager::ui::State state);

  void TearDown() override;

 protected:
  PostSaveCompromisedBubbleView* view_;
};

void PostSaveCompromisedBubbleViewTest::CreateViewAndShow(
    password_manager::ui::State state) {
  CreateAnchorViewAndShow();

  EXPECT_CALL(*model_delegate_mock(), GetState).WillOnce(Return(state));
  view_ = new PostSaveCompromisedBubbleView(web_contents(), anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void PostSaveCompromisedBubbleViewTest::TearDown() {
  view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);

  PasswordBubbleViewTestBase::TearDown();
}

TEST_F(PostSaveCompromisedBubbleViewTest, SafeState) {
  CreateViewAndShow(password_manager::ui::PASSWORD_UPDATED_SAFE_STATE);
  EXPECT_FALSE(view_->GetOkButton());
  EXPECT_FALSE(view_->GetCancelButton());
}

// Flaky on Windows due to http://crbug.com/968222
#if defined(OS_WIN)
#define MAYBE_MoreToFixState DISABLED_MoreToFixState
#else
#define MAYBE_MoreToFixState MoreToFixState
#endif
TEST_F(PostSaveCompromisedBubbleViewTest, MAYBE_MoreToFixState) {
  CreateViewAndShow(password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX);
  EXPECT_TRUE(view_->GetOkButton());
  EXPECT_FALSE(view_->GetCancelButton());

  EXPECT_CALL(*model_delegate_mock(), NavigateToPasswordCheckup);
  view_->AcceptDialog();
}

// Flaky on Windows due to http://crbug.com/968222
#if defined(OS_WIN)
#define MAYBE_UnsafeState DISABLED_UnsafeState
#else
#define MAYBE_UnsafeState UnsafeState
#endif
TEST_F(PostSaveCompromisedBubbleViewTest, MAYBE_UnsafeState) {
  CreateViewAndShow(password_manager::ui::PASSWORD_UPDATED_UNSAFE_STATE);
  EXPECT_TRUE(view_->GetOkButton());
  EXPECT_FALSE(view_->GetCancelButton());

  EXPECT_CALL(*model_delegate_mock(), NavigateToPasswordCheckup);
  view_->AcceptDialog();
}

}  // namespace
