// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MoveToAccountStoreBubbleControllerTest : public ::testing::Test {
 public:
  MoveToAccountStoreBubbleControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ = std::make_unique<MoveToAccountStoreBubbleController>(
        mock_delegate_->AsWeakPtr());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }
  ~MoveToAccountStoreBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  MoveToAccountStoreBubbleController* controller() { return controller_.get(); }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<MoveToAccountStoreBubbleController> controller_;
};

TEST_F(MoveToAccountStoreBubbleControllerTest, CloseExplicitly) {
  EXPECT_CALL(*delegate(), OnBubbleHidden);
  controller()->OnBubbleClosing();
}

TEST_F(MoveToAccountStoreBubbleControllerTest, AcceptMove) {
  EXPECT_CALL(*delegate(), MovePasswordToAccountStore);
  controller()->AcceptMove();
}

}  // namespace
