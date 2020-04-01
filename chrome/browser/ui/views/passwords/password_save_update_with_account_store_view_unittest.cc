// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_update_with_account_store_view.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/combobox/combobox.h"

using ::testing::Return;
using ::testing::ReturnRef;

class PasswordSaveUpdateWithAccountStoreViewTest
    : public PasswordBubbleViewTestBase {
 public:
  PasswordSaveUpdateWithAccountStoreViewTest();
  ~PasswordSaveUpdateWithAccountStoreViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override {
    view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);

    PasswordBubbleViewTestBase::TearDown();
  }

  PasswordSaveUpdateWithAccountStoreView* view() { return view_; }
  views::Combobox* account_picker() {
    return view_->DestinationDropdownForTesting();
  }

 private:
  PasswordSaveUpdateWithAccountStoreView* view_;
  autofill::PasswordForm pending_password_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> current_forms_;
};

PasswordSaveUpdateWithAccountStoreViewTest::
    PasswordSaveUpdateWithAccountStoreViewTest() {
  ON_CALL(*feature_manager_mock(), GetDefaultPasswordStore)
      .WillByDefault(Return(autofill::PasswordForm::Store::kAccountStore));
  ON_CALL(*model_delegate_mock(), GetOrigin)
      .WillByDefault(ReturnRef(pending_password_.origin));
  ON_CALL(*model_delegate_mock(), GetState)
      .WillByDefault(Return(password_manager::ui::PENDING_PASSWORD_STATE));
  ON_CALL(*model_delegate_mock(), GetPendingPassword)
      .WillByDefault(ReturnRef(pending_password_));
  ON_CALL(*model_delegate_mock(), GetCurrentForms)
      .WillByDefault(ReturnRef(current_forms_));

  PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(
          &password_manager::BuildPasswordStore<
              content::BrowserContext,
              testing::NiceMock<password_manager::MockPasswordStore>>));
}

void PasswordSaveUpdateWithAccountStoreViewTest::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new PasswordSaveUpdateWithAccountStoreView(
      web_contents(), anchor_view(), LocationBarBubbleDelegateView::AUTOMATIC);
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest, HasTitleAndTwoButtons) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
  EXPECT_TRUE(view()->GetOkButton());
  EXPECT_TRUE(view()->GetCancelButton());
}

// TODO(crbug.com/1054629): Flakily times out on all platforms.
TEST_F(PasswordSaveUpdateWithAccountStoreViewTest,
       DISABLED_ShouldNotShowAccountPicker) {
  ON_CALL(*feature_manager_mock(), ShouldShowPasswordStorePicker)
      .WillByDefault(Return(false));
  CreateViewAndShow();
  EXPECT_FALSE(account_picker());
}

// TODO(crbug.com/1054629): Flakily times out on all platforms.
TEST_F(PasswordSaveUpdateWithAccountStoreViewTest,
       DISABLED_ShouldShowAccountPicker) {
  ON_CALL(*feature_manager_mock(), ShouldShowPasswordStorePicker)
      .WillByDefault(Return(true));
  CreateViewAndShow();
  ASSERT_TRUE(account_picker());
  EXPECT_EQ(0, account_picker()->GetSelectedIndex());
}

// TODO(crbug.com/1054629): Flakily times out on all platforms.
TEST_F(PasswordSaveUpdateWithAccountStoreViewTest,
       DISABLED_ShouldSelectAccountStoreByDefault) {
  ON_CALL(*feature_manager_mock(), ShouldShowPasswordStorePicker)
      .WillByDefault(Return(true));
  ON_CALL(*feature_manager_mock(), GetDefaultPasswordStore)
      .WillByDefault(Return(autofill::PasswordForm::Store::kAccountStore));

  CoreAccountInfo account_info =
      identity_test_env()->SetUnconsentedPrimaryAccount("test@email.com");

  CreateViewAndShow();

  ASSERT_TRUE(account_picker());
  EXPECT_EQ(0, account_picker()->GetSelectedIndex());
  EXPECT_EQ(
      base::ASCIIToUTF16(account_info.email),
      account_picker()->GetTextForRow(account_picker()->GetSelectedIndex()));
}

// TODO(crbug.com/1054629): Flakily times out on all platforms.
TEST_F(PasswordSaveUpdateWithAccountStoreViewTest,
       DISABLED_ShouldSelectProfileStoreByDefault) {
  ON_CALL(*feature_manager_mock(), ShouldShowPasswordStorePicker)
      .WillByDefault(Return(true));
  ON_CALL(*feature_manager_mock(), GetDefaultPasswordStore)
      .WillByDefault(Return(autofill::PasswordForm::Store::kProfileStore));
  CreateViewAndShow();
  ASSERT_TRUE(account_picker());
  EXPECT_EQ(1, account_picker()->GetSelectedIndex());
  // TODO(crbug.com/1044038): Use an internationalized string instead.
  EXPECT_EQ(
      base::ASCIIToUTF16("Local"),
      account_picker()->GetTextForRow(account_picker()->GetSelectedIndex()));
}
