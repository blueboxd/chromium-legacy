// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_TEST_BASE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_TEST_BASE_H_

#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/ui/browser.h"

// Base class for child user browser tests.
// Inherit from this class if logging in as child user is required for tests.
// This class inherits from LoginPolicyTestBase because signing in a child
// account requires fetching user policies.
class SupervisedUserTestBase : public policy::LoginPolicyTestBase {
 public:
  enum class LogInType { kRegular, kChild };

  static Profile* GetPrimaryUserProfile();

 protected:
  void LogInUser(LogInType type);

  // Returns the first browser in the active browser list.
  // Hides InProcessBrowserTest::browser() because the browser is only created
  // after LogInUser() is called and by then it's too late to set
  // InProcessBrowserTest::browser_ because it's a private member.
  // Will return null if called before LogInUser().
  static Browser* browser();

  static SupervisedUserService* supervised_user_service();
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_TEST_BASE_H_
