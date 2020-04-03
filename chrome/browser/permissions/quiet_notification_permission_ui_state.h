// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_STATE_H_
#define CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_STATE_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class Profile;

class QuietNotificationPermissionUiState {
 public:
  // Register Profile-keyed preferences used for permission UI selection.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Whether to show a promo for the prompt indicator.
  static bool ShouldShowPromo(Profile* profile);

  // Records that the promo was shown.
  static void PromoWasShown(Profile* profile);
};

#endif  // CHROME_BROWSER_PERMISSIONS_QUIET_NOTIFICATION_PERMISSION_UI_STATE_H_:b
