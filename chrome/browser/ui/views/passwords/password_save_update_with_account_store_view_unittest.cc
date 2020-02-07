// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_update_with_account_store_view.h"

#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "content/public/test/web_contents_tester.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class TestManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  explicit TestManagePasswordsUIController(content::WebContents* web_contents);

  base::WeakPtr<PasswordsModelDelegate> GetModelDelegateProxy() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  NiceMock<PasswordsModelDelegateMock> model_delegate_mock_;
  base::WeakPtrFactory<PasswordsModelDelegate> weak_ptr_factory_;
  autofill::PasswordForm pending_password_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> current_forms_;
  NiceMock<password_manager::MockPasswordFeatureManager> feature_manager_;
};

TestManagePasswordsUIController::TestManagePasswordsUIController(
    content::WebContents* web_contents)
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
      .WillByDefault(Return(&feature_manager_));

  ON_CALL(feature_manager_, GetDefaultPasswordStore)
      .WillByDefault(Return(autofill::PasswordForm::Store::kAccountStore));
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

 private:
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  PasswordSaveUpdateWithAccountStoreView* view_;
};

PasswordSaveUpdateWithAccountStoreViewTest::
    PasswordSaveUpdateWithAccountStoreViewTest() {
  PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_,
      base::BindRepeating(
          &password_manager::BuildPasswordStore<
              content::BrowserContext,
              testing::NiceMock<password_manager::MockPasswordStore>>));
  test_web_contents_ =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  // Create the test UIController here so that it's bound to
  // |test_web_contents_|, and will be retrieved correctly via
  // ManagePasswordsUIController::FromWebContents in
  // PasswordsModelDelegateFromWebContents().
  new TestManagePasswordsUIController(test_web_contents_.get());
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
