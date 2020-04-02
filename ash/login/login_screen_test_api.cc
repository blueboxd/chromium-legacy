// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

namespace {

LoginShelfView* GetLoginShelfView() {
  if (!Shell::HasInstance())
    return nullptr;

  return Shelf::ForWindow(Shell::GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view();
}

bool IsLoginShelfViewButtonShown(int button_view_id) {
  LoginShelfView* shelf_view = GetLoginShelfView();
  if (!shelf_view)
    return false;

  views::View* button_view = shelf_view->GetViewByID(button_view_id);

  return button_view && button_view->GetVisible();
}

views::View* GetShutDownButton() {
  LoginShelfView* shelf_view = GetLoginShelfView();
  if (!shelf_view)
    return nullptr;

  return shelf_view->GetViewByID(LoginShelfView::kShutdown);
}

}  // anonymous namespace

class ShelfTestUiUpdateDelegate : public LoginShelfView::TestUiUpdateDelegate {
 public:
  // Returns instance owned by LoginShelfView. Installs instance of
  // ShelfTestUiUpdateDelegate when needed.
  static ShelfTestUiUpdateDelegate* Get(LoginShelfView* shelf) {
    if (!shelf->test_ui_update_delegate()) {
      shelf->InstallTestUiUpdateDelegate(
          std::make_unique<ShelfTestUiUpdateDelegate>());
    }
    return static_cast<ShelfTestUiUpdateDelegate*>(
        shelf->test_ui_update_delegate());
  }

  ShelfTestUiUpdateDelegate() = default;
  ~ShelfTestUiUpdateDelegate() override {
    for (PendingCallback& entry : heap_)
      std::move(entry.callback).Run();
  }

  // Returns UI update count.
  int64_t ui_update_count() const { return ui_update_count_; }

  // Add a callback to be invoked when ui update count is greater than
  // |previous_update_count|. Note |callback| could be invoked synchronously
  // when the current ui update count is already greater than
  // |previous_update_count|.
  void AddCallback(int64_t previous_update_count, base::OnceClosure callback) {
    if (previous_update_count < ui_update_count_) {
      std::move(callback).Run();
    } else {
      heap_.emplace_back(previous_update_count, std::move(callback));
      std::push_heap(heap_.begin(), heap_.end());
    }
  }

  // LoginShelfView::TestUiUpdateDelegate
  void OnUiUpdate() override {
    ++ui_update_count_;
    while (!heap_.empty() && heap_.front().old_count < ui_update_count_) {
      std::move(heap_.front().callback).Run();
      std::pop_heap(heap_.begin(), heap_.end());
      heap_.pop_back();
    }
  }

 private:
  struct PendingCallback {
    PendingCallback(int64_t old_count, base::OnceClosure callback)
        : old_count(old_count), callback(std::move(callback)) {}

    bool operator<(const PendingCallback& right) const {
      // We need min_heap, therefore this returns true when another element on
      // the right is less than this count. (regular heap is max_heap).
      return old_count > right.old_count;
    }

    int64_t old_count = 0;
    base::OnceClosure callback;
  };

  std::vector<PendingCallback> heap_;

  int64_t ui_update_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ShelfTestUiUpdateDelegate);
};

// static
bool LoginScreenTestApi::IsLockShown() {
  return LockScreen::HasInstance() && LockScreen::Get()->is_shown() &&
         LockScreen::Get()->screen_type() == LockScreen::ScreenType::kLock;
}

// static
bool LoginScreenTestApi::IsLoginShelfShown() {
  LoginShelfView* view = GetLoginShelfView();
  return view && view->GetVisible();
}

// static
bool LoginScreenTestApi::IsRestartButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kRestart);
}

// static
bool LoginScreenTestApi::IsShutdownButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kShutdown);
}

// static
bool LoginScreenTestApi::IsAuthErrorBubbleShown() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  return lock_contents_test.auth_error_bubble()->GetVisible();
}

// static
bool LoginScreenTestApi::IsGuestButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kBrowseAsGuest);
}

// static
bool LoginScreenTestApi::IsAddUserButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kAddUser);
}

// static
bool LoginScreenTestApi::IsCancelButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kCancel);
}

// static
bool LoginScreenTestApi::IsParentAccessButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kParentAccess);
}

// static
bool LoginScreenTestApi::IsWarningBubbleShown() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  return lock_contents_test.warning_banner_bubble()->GetVisible();
}

// static
void LoginScreenTestApi::SubmitPassword(const AccountId& account_id,
                                        const std::string& password,
                                        bool check_if_submittable) {
  // It'd be better to generate keyevents dynamically and dispatch them instead
  // of reaching into the views structure, but at the time of writing I could
  // not find a good way to do this. If you know of a way feel free to change
  // this code.
  ASSERT_TRUE(FocusUser(account_id));
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  LoginBigUserView* big_user_view = lock_contents_test.FindUser(account_id);
  ASSERT_TRUE(big_user_view);
  ASSERT_TRUE(big_user_view->IsAuthEnabled());
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  if (check_if_submittable)
    ASSERT_TRUE(auth_test.HasAuthMethod(LoginAuthUserView::AUTH_PASSWORD));
  LoginPasswordView::TestApi password_test(auth_test.password_view());
  ASSERT_EQ(account_id,
            auth_test.user_view()->current_user().basic_user_info.account_id);
  password_test.SubmitPassword(password);
}

// static
int64_t LoginScreenTestApi::GetUiUpdateCount() {
  LoginShelfView* view = GetLoginShelfView();
  return view ? ShelfTestUiUpdateDelegate::Get(view)->ui_update_count() : 0;
}

// static
bool LoginScreenTestApi::LaunchApp(const std::string& app_id) {
  LoginShelfView* view = GetLoginShelfView();
  return view && view->LaunchAppForTesting(app_id);
}

// static
bool LoginScreenTestApi::ClickAddUserButton() {
  LoginShelfView* view = GetLoginShelfView();
  return view &&
         view->SimulateButtonPressedForTesting(LoginShelfView::kAddUser);
}

// static
bool LoginScreenTestApi::ClickCancelButton() {
  LoginShelfView* view = GetLoginShelfView();
  return view && view->SimulateButtonPressedForTesting(LoginShelfView::kCancel);
}

// static
bool LoginScreenTestApi::ClickGuestButton() {
  LoginShelfView* view = GetLoginShelfView();
  return view &&
         view->SimulateButtonPressedForTesting(LoginShelfView::kBrowseAsGuest);
}

// static
bool LoginScreenTestApi::WaitForUiUpdate(int64_t previous_update_count) {
  LoginShelfView* view = GetLoginShelfView();
  if (view) {
    base::RunLoop run_loop;
    ShelfTestUiUpdateDelegate::Get(view)->AddCallback(previous_update_count,
                                                      run_loop.QuitClosure());
    run_loop.Run();
    return true;
  }

  return false;
}

int LoginScreenTestApi::GetUsersCount() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  return lock_contents_test.users().size();
}

// static
bool LoginScreenTestApi::FocusUser(const AccountId& account_id) {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  LoginBigUserView* big_user_view = lock_contents_test.FindUser(account_id);
  if (!big_user_view)
    return false;
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  LoginUserView::TestApi user_test(auth_test.user_view());
  user_test.OnTap();
  return GetFocusedUser() == account_id;
}

// static
AccountId LoginScreenTestApi::GetFocusedUser() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  return lock_contents_test.focused_user();
}

// static
bool LoginScreenTestApi::RemoveUser(const AccountId& account_id) {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  return lock_contents_test.RemoveUser(account_id);
}

// static
bool LoginScreenTestApi::IsOobeDialogVisible() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsView::TestApi lock_contents_test(
      lock_screen_test.contents_view());
  return lock_contents_test.IsOobeDialogVisible();
}

// static
base::string16 LoginScreenTestApi::GetShutDownButtonLabel() {
  views::View* button = GetShutDownButton();
  if (!button)
    return base::string16();

  return static_cast<views::LabelButton*>(button)->GetText();
}

// static
gfx::Rect LoginScreenTestApi::GetShutDownButtonTargetBounds() {
  views::View* button = GetShutDownButton();
  if (!button)
    return gfx::Rect();

  return button->layer()->GetTargetBounds();
}

// static
gfx::Rect LoginScreenTestApi::GetShutDownButtonMirroredBounds() {
  views::View* button = GetShutDownButton();
  if (!button)
    return gfx::Rect();

  return button->GetMirroredBounds();
}
}  // namespace ash
