// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"
#include "chrome/browser/ui/views/payments/test_secure_payment_confirmation_payment_request_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"

namespace payments {

class SecurePaymentConfirmationDialogViewTest
    : public InProcessBrowserTest,
      public SecurePaymentConfirmationDialogView::ObserverForTest {
 public:
  enum DialogEvent : int {
    DIALOG_OPENED,
    DIALOG_CLOSED,
  };

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void CreateModel() {
    model_.set_verify_button_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_BUTTON_LABEL));
    model_.set_cancel_button_label(l10n_util::GetStringUTF16(IDS_CANCEL));
  }

  void InvokeSecurePaymentConfirmationUI() {
    content::WebContents* web_contents = GetActiveWebContents();

    test_delegate_ =
        std::make_unique<TestSecurePaymentConfirmationPaymentRequestDelegate>(
            web_contents, model_.GetWeakPtr(), this);

    ResetEventWaiter(DialogEvent::DIALOG_OPENED);
    test_delegate_->ShowDialog(nullptr);
    event_waiter_->Wait();

    // The web-modal dialog should be open.
    web_modal::WebContentsModalDialogManager*
        web_contents_modal_dialog_manager =
            web_modal::WebContentsModalDialogManager::FromWebContents(
                web_contents);
    EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());
  }

  void ExpectViewMatchesModel() {
    ASSERT_NE(test_delegate_->dialog_view(), nullptr);

    EXPECT_EQ(model_.verify_button_label(),
              test_delegate_->dialog_view()->GetDialogButtonLabel(
                  ui::DIALOG_BUTTON_OK));

    EXPECT_EQ(model_.cancel_button_label(),
              test_delegate_->dialog_view()->GetDialogButtonLabel(
                  ui::DIALOG_BUTTON_CANCEL));

    EXPECT_TRUE(test_delegate_->dialog_view()->GetViewByID(
        SecurePaymentConfirmationDialogView::DialogViewID::HEADER_ICON));

    EXPECT_EQ(
        model_.progress_bar_visible(),
        test_delegate_->dialog_view()
            ->GetViewByID(
                SecurePaymentConfirmationDialogView::DialogViewID::PROGRESS_BAR)
            ->GetVisible());
  }

  void ClickAcceptAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    test_delegate_->dialog_view()->AcceptDialog();
    event_waiter_->Wait();

    EXPECT_TRUE(confirm_pressed_);
    EXPECT_FALSE(cancel_pressed_);
  }

  void ClickCancelAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    test_delegate_->dialog_view()->CancelDialog();
    event_waiter_->Wait();

    EXPECT_TRUE(cancel_pressed_);
    EXPECT_FALSE(confirm_pressed_);
  }

  void CloseDialogAndWait() {
    ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

    test_delegate_->CloseDialog();
    event_waiter_->Wait();

    EXPECT_FALSE(cancel_pressed_);
    EXPECT_FALSE(confirm_pressed_);
  }

  void ResetEventWaiter(DialogEvent event) {
    event_waiter_ = std::make_unique<autofill::EventWaiter<DialogEvent>>(
        std::list<DialogEvent>{event});
  }

  // SecurePaymentConfirmationDialogView::ObserverForTest:
  void OnDialogOpened() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::DIALOG_OPENED);
  }

  void OnDialogClosed() override {
    if (event_waiter_)
      event_waiter_->OnEvent(DialogEvent::DIALOG_CLOSED);
  }

  void OnConfirmButtonPressed() override { confirm_pressed_ = true; }

  void OnCancelButtonPressed() override { cancel_pressed_ = true; }

 protected:
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;

  SecurePaymentConfirmationModel model_;
  std::unique_ptr<TestSecurePaymentConfirmationPaymentRequestDelegate>
      test_delegate_;

  bool confirm_pressed_ = false;
  bool cancel_pressed_ = false;
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       AcceptButtonTest) {
  CreateModel();

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  ClickAcceptAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       CancelButtonTest) {
  CreateModel();

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  ClickCancelAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       CloseDialogTest) {
  CreateModel();

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       ProgressBarVisible) {
  CreateModel();
  model_.set_progress_bar_visible(true);

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       ShowProgressBar) {
  CreateModel();

  ASSERT_FALSE(model_.progress_bar_visible());

  InvokeSecurePaymentConfirmationUI();

  ExpectViewMatchesModel();

  model_.set_progress_bar_visible(true);
  test_delegate_->dialog_view()->OnModelUpdated();

  ExpectViewMatchesModel();

  CloseDialogAndWait();
}

}  // namespace payments
