// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_TYPE_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_TYPE_H_

// Badge types.
enum class BadgeType {
  // Badge type for the confirm Infobar.
  kBadgeTypeConfirm = 0,
  // Badge type for the Save Passwords Infobar.
  kBadgeTypePasswordSave = 1,
  // Badge type for the Update Passwords Infobar.
  kBadgeTypePasswordUpdate = 2,
};

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_TYPE_H_
