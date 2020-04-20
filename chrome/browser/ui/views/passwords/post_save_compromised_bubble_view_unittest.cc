// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/post_save_compromised_bubble_view.h"

#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"

class PostSaveCompromisedBubbleViewTest : public PasswordBubbleViewTestBase {
 public:
  PostSaveCompromisedBubbleViewTest() = default;
  ~PostSaveCompromisedBubbleViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override;

 protected:
  PostSaveCompromisedBubbleView* view_;
};

void PostSaveCompromisedBubbleViewTest::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new PostSaveCompromisedBubbleView(web_contents(), anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void PostSaveCompromisedBubbleViewTest::TearDown() {
  view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);

  PasswordBubbleViewTestBase::TearDown();
}

TEST_F(PostSaveCompromisedBubbleViewTest, HasTwoButtons) {
  CreateViewAndShow();
  EXPECT_TRUE(view_->GetOkButton());
  EXPECT_TRUE(view_->GetCancelButton());
}
