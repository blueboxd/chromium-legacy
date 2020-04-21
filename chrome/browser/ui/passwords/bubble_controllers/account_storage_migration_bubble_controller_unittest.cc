// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/account_storage_migration_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AccountStorageMigrationBubbleControllerTest : public ::testing::Test {
 public:
  AccountStorageMigrationBubbleControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ = std::make_unique<AccountStorageMigrationBubbleController>(
        mock_delegate_->AsWeakPtr());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }
  ~AccountStorageMigrationBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  AccountStorageMigrationBubbleController* controller() {
    return controller_.get();
  }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<AccountStorageMigrationBubbleController> controller_;
};

TEST_F(AccountStorageMigrationBubbleControllerTest, CloseExplicictly) {
  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
}

}  // namespace
