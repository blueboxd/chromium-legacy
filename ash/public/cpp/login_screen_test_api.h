// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_SCREEN_TEST_API_H_
#define ASH_PUBLIC_CPP_LOGIN_SCREEN_TEST_API_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/login_types.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"

class AccountId;

namespace ash {

class ASH_PUBLIC_EXPORT LoginScreenTestApi {
 public:
  static bool IsLockShown();
  static bool IsLoginShelfShown();
  static bool IsRestartButtonShown();
  static bool IsShutdownButtonShown();
  static bool IsAuthErrorBubbleShown();
  static bool IsGuestButtonShown();
  static bool IsAddUserButtonShown();
  static bool IsCancelButtonShown();
  static bool IsParentAccessButtonShown();
  static bool IsWarningBubbleShown();
  static bool IsDisplayPasswordButtonShown(const AccountId& account_id);
  static bool IsForcedOnlineSignin(const AccountId& account_id);
  static void SubmitPassword(const AccountId& account_id,
                             const std::string& password,
                             bool check_if_submittable);
  static int64_t GetUiUpdateCount();
  static bool LaunchApp(const std::string& app_id);
  static bool ClickAddUserButton();
  static bool ClickCancelButton();
  static bool ClickGuestButton();
  static bool WaitForUiUpdate(int64_t previous_update_count);
  static int GetUsersCount();
  static bool FocusUser(const AccountId& account_id);
  static AccountId GetFocusedUser();
  static bool RemoveUser(const AccountId& account_id);

  static std::string GetDisplayedName(const AccountId& account_id);

  static bool ExpandPublicSessionPod(const AccountId& account_id);
  static bool HidePublicSessionExpandedPod();
  static bool IsPublicSessionExpanded();
  static bool IsExpandedPublicSessionAdvanced();
  static void ClickPublicExpandedAdvancedViewButton();
  static void ClickPublicExpandedSubmitButton();
  static void SetPublicSessionLocale(const std::string& locale);
  static void SetPublicSessionKeyboard(const std::string& ime_id);
  static std::vector<ash::LocaleItem> GetPublicSessionLocales(
      const AccountId& account_id);
  static std::vector<ash::LocaleItem> GetExpandedPublicSessionLocales();
  static std::string GetExpandedPublicSessionSelectedLocale();
  static std::string GetExpandedPublicSessionSelectedKeyboard();

  static bool IsOobeDialogVisible();
  static base::string16 GetShutDownButtonLabel();
  static gfx::Rect GetShutDownButtonTargetBounds();
  static gfx::Rect GetShutDownButtonMirroredBounds();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(LoginScreenTestApi);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_SCREEN_TEST_API_H_
