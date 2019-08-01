// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_

#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"

namespace password_manager {

// Performs a leak-check for {username, password} for Chrome signed-in users.
class AuthenticatedLeakCheck : public LeakDetectionCheck {
 public:
  AuthenticatedLeakCheck();
  ~AuthenticatedLeakCheck() override;

  void Start(const GURL& url,
             base::StringPiece16 username,
             base::StringPiece16 password) override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_
