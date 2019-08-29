// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager_delegate_win.h"

#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/taskbar/taskbar_decorator_win.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace badging {

BadgeManagerDelegateWin::BadgeManagerDelegateWin(Profile* profile)
    : BadgeManagerDelegate(profile) {}

void BadgeManagerDelegateWin::OnBadgeSet(const std::string& app_id,
                                         base::Optional<uint64_t> contents) {
  auto badge_string = badging::GetBadgeString(contents);
  // There are 3 different cases:
  // 1. |contents| is between 1 and 99 inclusive => Set the accessibility text
  //    to a pluralized notification count (e.g. 4 Unread Notifications).
  // 2. |contents| is greater than 99 => Set the accessibility text to
  //    More than |kMaxBadgeContent| unread notifications, so the
  //    accessibility text matches what is displayed on the badge (e.g. More
  //    than 99 notifications).
  // 3. |contents| doesn't have a value (i.e. the badge is set to 'flag') => Set
  //    the accessibility text to something less specific (e.g. Unread
  //    Notifications).
  std::string badge_alt_string;
  if (contents) {
    uint64_t value = contents.value();
    badge_alt_string = value <= badging::kMaxBadgeContent
                           // Case 1.
                           ? l10n_util::GetPluralStringFUTF8(
                                 IDS_BADGE_UNREAD_NOTIFICATIONS, value)
                           // Case 2.
                           : l10n_util::GetPluralStringFUTF8(
                                 IDS_BADGE_UNREAD_NOTIFICATIONS_SATURATED,
                                 badging::kMaxBadgeContent);
  } else {
    // Case 3.
    badge_alt_string =
        l10n_util::GetStringUTF8(IDS_BADGE_UNREAD_NOTIFICATIONS_UNSPECIFIED);
  }

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!IsAppBrowser(browser, app_id))
      continue;

    auto* window = browser->window()->GetNativeWindow();
    taskbar::DrawTaskbarDecorationString(window, badge_string,
                                         badge_alt_string);
  }
}

void BadgeManagerDelegateWin::OnBadgeCleared(const std::string& app_id) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!IsAppBrowser(browser, app_id))
      continue;

    // Restore the decoration to whatever it is naturally (either nothing or a
    // profile picture badge).
    taskbar::UpdateTaskbarDecoration(browser->profile(),
                                     browser->window()->GetNativeWindow());
  }
}

bool BadgeManagerDelegateWin::IsAppBrowser(Browser* browser,
                                           const std::string& app_id) {
  return browser->app_controller() &&
         browser->app_controller()->GetAppId() == app_id &&
         browser->profile() == profile_;
}

}  // namespace badging
