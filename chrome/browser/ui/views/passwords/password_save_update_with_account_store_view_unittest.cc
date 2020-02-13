// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_update_with_account_store_view.h"

#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/views/controls/combobox/combobox.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class TestManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  explicit TestManagePasswordsUIController(
      content::WebContents* web_contents,
      password_manager::PasswordFeatureManager* feature_manager);

  base::WeakPtr<PasswordsModelDelegate> GetModelDelegateProxy() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  NiceMock<PasswordsModelDelegateMock> model_delegate_mock_;
  base::WeakPtrFactory<PasswordsModelDelegate> weak_ptr_factory_;
  autofill::PasswordForm pending_password_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> current_forms_;
};

TestManagePasswordsUIController::TestManagePasswordsUIController(
    content::WebContents* web_contents,
    password_manager::PasswordFeatureManager* feature_manager)
    : ManagePasswordsUIController(web_contents),
      weak_ptr_factory_(&model_delegate_mock_) {
  // Do not silently replace an existing ManagePasswordsUIController
  // because it unregisters itself in WebContentsDestroyed().
  EXPECT_FALSE(web_contents->GetUserData(UserDataKey()));
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(this));

  ON_CALL(model_delegate_mock_, GetOrigin)
      .WillByDefault(ReturnRef(pending_password_.origin));
  ON_CALL(model_delegate_mock_, GetState)
      .WillByDefault(Return(password_manager::ui::PENDING_PASSWORD_STATE));
  ON_CALL(model_delegate_mock_, GetPendingPassword)
      .WillByDefault(ReturnRef(pending_password_));
  ON_CALL(model_delegate_mock_, GetCurrentForms)
      .WillByDefault(ReturnRef(current_forms_));
  ON_CALL(model_delegate_mock_, GetWebContents)
      .WillByDefault(Return(web_contents));
  ON_CALL(model_delegate_mock_, GetPasswordFeatureManager)
      .WillByDefault(Return(feature_manager));
}

class PasswordSaveUpdateWithAccountStoreViewTest : public ChromeViewsTestBase {
 public:
  PasswordSaveUpdateWithAccountStoreViewTest();
  ~PasswordSaveUpdateWithAccountStoreViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override {
    view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
    anchor_widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  PasswordSaveUpdateWithAccountStoreView* view() { return view_; }
  views::Combobox* account_picker() {
    return view_->DestinationDropdownForTesting();
  }
  password_manager::MockPasswordFeatureManager* feature_manager() {
    return &feature_manager_;
  }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 private:
  NiceMock<password_manager::MockPasswordFeatureManager> feature_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  PasswordSaveUpdateWithAccountStoreView* view_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
};

PasswordSaveUpdateWithAccountStoreViewTest::
    PasswordSaveUpdateWithAccountStoreViewTest() {
  ON_CALL(feature_manager_, GetDefaultPasswordStore)
      .WillByDefault(Return(autofill::PasswordForm::Store::kAccountStore));

  profile_ = IdentityTestEnvironmentProfileAdaptor::
      CreateProfileForIdentityTestEnvironment({});
  identity_test_env_profile_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

  PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(),
      base::BindRepeating(
          &password_manager::BuildPasswordStore<
              content::BrowserContext,
              testing::NiceMock<password_manager::MockPasswordStore>>));
  test_web_contents_ = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), nullptr);
  // Create the test UIController here so that it's bound to
  // |test_web_contents_|, and will be retrieved correctly via
  // ManagePasswordsUIController::FromWebContents in
  // PasswordsModelDelegateFromWebContents().
  new TestManagePasswordsUIController(test_web_contents_.get(),
                                      &feature_manager_);
}

void PasswordSaveUpdateWithAccountStoreViewTest::CreateViewAndShow() {
  // The bubble needs the parent as an anchor.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

  anchor_widget_ = std::make_unique<views::Widget>();
  anchor_widget_->Init(std::move(params));
  anchor_widget_->Show();

  view_ = new PasswordSaveUpdateWithAccountStoreView(
      test_web_contents_.get(), anchor_widget_->GetContentsView(),
      LocationBarBubbleDelegateView::AUTOMATIC);
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest, HasTitleAndTwoButtons) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
  EXPECT_TRUE(view()->GetOkButton());
  EXPECT_TRUE(view()->GetCancelButton());
}

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest, ShouldNotShowAccountPicker) {
  ON_CALL(*feature_manager(), ShouldShowPasswordStorePicker)
      .WillByDefault(Return(false));
  CreateViewAndShow();
  EXPECT_FALSE(account_picker());
}

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest, ShouldShowAccountPicker) {
  ON_CALL(*feature_manager(), ShouldShowPasswordStorePicker)
      .WillByDefault(Return(true));
  CreateViewAndShow();
  ASSERT_TRUE(account_picker());
  EXPECT_EQ(0, account_picker()->GetSelectedIndex());
}

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest,
       ShouldSelectAccountStoreByDefault) {
  ON_CALL(*feature_manager(), ShouldShowPasswordStorePicker)
      .WillByDefault(Return(true));
  ON_CALL(*feature_manager(), GetDefaultPasswordStore)
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

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest,
       ShouldSelectProfileStoreByDefault) {
  ON_CALL(*feature_manager(), ShouldShowPasswordStorePicker)
      .WillByDefault(Return(true));
  ON_CALL(*feature_manager(), GetDefaultPasswordStore)
      .WillByDefault(Return(autofill::PasswordForm::Store::kProfileStore));
  CreateViewAndShow();
  ASSERT_TRUE(account_picker());
  EXPECT_EQ(1, account_picker()->GetSelectedIndex());
  // TODO(crbug.com/1044038): Use an internationalized string instead.
  EXPECT_EQ(
      base::ASCIIToUTF16("Local"),
      account_picker()->GetTextForRow(account_picker()->GetSelectedIndex()));
}
