// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace {

class TestDelegate : public ui::ProfileSigninConfirmationDelegate {
 public:
  TestDelegate(int* cancels, int* continues, int* signins)
      : cancels_(cancels), continues_(continues), signins_(signins) {}
  ~TestDelegate() override = default;

  void OnCancelSignin() override { (*cancels_)++; }
  void OnContinueSignin() override { (*continues_)++; }
  void OnSigninWithNewProfile() override { (*signins_)++; }

  int* cancels_;
  int* continues_;
  int* signins_;
};

}  // namespace

using ProfileSigninConfirmationDialogTest = ChromeViewsTestBase;

// Regression test for https://crbug.com/1054866
TEST_F(ProfileSigninConfirmationDialogTest, CloseButtonOnlyCallsDelegateOnce) {
  int cancels = 0;
  int continues = 0;
  int signins = 0;

  auto delegate =
      std::make_unique<TestDelegate>(&cancels, &continues, &signins);
  auto dialog = std::make_unique<ProfileSigninConfirmationDialogViews>(
      nullptr, "foo@bar.com", std::move(delegate), true);
  ProfileSigninConfirmationDialogViews* weak_dialog = dialog.get();

  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      dialog.release(), GetContext(), nullptr);
  views::test::WidgetDestroyedWaiter destroy_waiter(widget);

  widget->Show();

  // Press the "continue signin" button.
  views::Button* button =
      static_cast<views::Button*>(weak_dialog->GetExtraView());
  // Synthesize both press & release - different platforms have different
  // notions about whether buttons activate on press or on release.
  button->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE));
  button->OnKeyReleased(
      ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_SPACE, ui::EF_NONE));
  destroy_waiter.Wait();

  // The delegate should *not* have gotten a call back to OnCancelSignin. If the
  // fix for https://crbug.com/1054866 regresses, either it will, or we'll have
  // crashed above.
  EXPECT_EQ(cancels, 0);
  EXPECT_EQ(continues, 1);
  EXPECT_EQ(signins, 0);
}
