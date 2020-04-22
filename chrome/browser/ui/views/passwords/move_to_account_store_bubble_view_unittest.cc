// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/move_to_account_store_bubble_view.h"

#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"

class MoveToAccountStoreBubbleViewTest : public PasswordBubbleViewTestBase {
 public:
  MoveToAccountStoreBubbleViewTest() = default;
  ~MoveToAccountStoreBubbleViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override;

 protected:
  MoveToAccountStoreBubbleView* view_;
};

void MoveToAccountStoreBubbleViewTest::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new MoveToAccountStoreBubbleView(web_contents(), anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void MoveToAccountStoreBubbleViewTest::TearDown() {
  view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);

  PasswordBubbleViewTestBase::TearDown();
}

TEST_F(MoveToAccountStoreBubbleViewTest, HasTwoButtons) {
  CreateViewAndShow();
  EXPECT_TRUE(view_->GetOkButton());
  EXPECT_TRUE(view_->GetCancelButton());
}
