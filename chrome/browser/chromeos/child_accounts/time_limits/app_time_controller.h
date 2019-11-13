// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_

namespace chromeos {

// Coordinates per-app time limit for child user.
class AppTimeController {
 public:
  static bool ArePerAppTimeLimitsEnabled();

  AppTimeController();
  AppTimeController(const AppTimeController&) = delete;
  AppTimeController& operator=(const AppTimeController&) = delete;
  ~AppTimeController();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_
