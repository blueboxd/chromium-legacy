// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_CONSTANTS_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_CONSTANTS_H_

#include "base/time/time.h"

// Time limit for click to call message expiration.
constexpr base::TimeDelta kSharingClickToCallMessageTTL =
    base::TimeDelta::FromSeconds(10);

// Maximum number of devices to be shown in dialog and context menu.
constexpr int kMaxDevicesShown = 10;

// Command id for first device shown in submenu.
constexpr int kSubMenuFirstDeviceCommandId = 2150;

// Command id for last device shown in submenu.
constexpr int kSubMenuLastDeviceCommandId =
    kSubMenuFirstDeviceCommandId + kMaxDevicesShown - 1;

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_CONSTANTS_H_
