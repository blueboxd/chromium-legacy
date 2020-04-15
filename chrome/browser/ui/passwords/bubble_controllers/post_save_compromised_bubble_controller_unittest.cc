// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/post_save_compromised_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PostSaveCompromisedBubbleControllerTest : public ::testing::Test {
 public:
  PostSaveCompromisedBubbleControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ = std::make_unique<PostSaveCompromisedBubbleController>(
        mock_delegate_->AsWeakPtr());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }
  ~PostSaveCompromisedBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  PostSaveCompromisedBubbleController* controller() {
    return controller_.get();
  }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<PostSaveCompromisedBubbleController> controller_;
};

TEST_F(PostSaveCompromisedBubbleControllerTest, CloseExplicictly) {
  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
}

}  // namespace
