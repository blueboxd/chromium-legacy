// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_impl.h"

#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::StrictMock;

class MockCredentialLeakPrompt : public CredentialLeakPrompt {
 public:
  MockCredentialLeakPrompt() = default;

  MOCK_METHOD0(ShowCredentialLeakPrompt, void());
  MOCK_METHOD0(ControllerGone, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCredentialLeakPrompt);
};

class CredentialLeakDialogControllerTest : public testing::Test {
 public:
  CredentialLeakDialogControllerTest()
      : controller_(&ui_controller_mock_,
                    password_manager::CreateLeakTypeFromBools(true, true, true),
                    GURL("https://example.com")) {}

  PasswordsLeakDialogDelegateMock& ui_controller_mock() {
    return ui_controller_mock_;
  }

  CredentialLeakDialogControllerImpl& controller() { return controller_; }

 private:
  StrictMock<PasswordsLeakDialogDelegateMock> ui_controller_mock_;
  CredentialLeakDialogControllerImpl controller_;
};

TEST_F(CredentialLeakDialogControllerTest, CredentialLeakDialogClose) {
  StrictMock<MockCredentialLeakPrompt> prompt;
  EXPECT_CALL(prompt, ShowCredentialLeakPrompt());
  controller().ShowCredentialLeakPrompt(&prompt);

  EXPECT_CALL(ui_controller_mock(), OnLeakDialogHidden());
  controller().OnCloseDialog();
}

TEST_F(CredentialLeakDialogControllerTest, CredentialLeakDialogCheckPasswords) {
  StrictMock<MockCredentialLeakPrompt> prompt;
  EXPECT_CALL(prompt, ShowCredentialLeakPrompt());
  controller().ShowCredentialLeakPrompt(&prompt);

  EXPECT_CALL(prompt, ControllerGone());
  EXPECT_CALL(ui_controller_mock(), NavigateToPasswordCheckup());
  EXPECT_CALL(ui_controller_mock(), OnLeakDialogHidden());
  controller().OnCheckPasswords();
}

}  // namespace
