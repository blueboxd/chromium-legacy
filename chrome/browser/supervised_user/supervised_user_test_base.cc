// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_test_base.h"

#include "chrome/browser/chromeos/child_accounts/child_account_test_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/user_manager/user_manager.h"

void SupervisedUserTestBase::LogInUser(LogInType type) {
  SkipToLoginScreen();
  switch (type) {
    case LogInType::kChild:
      LogIn(kAccountId, kAccountPassword,
            chromeos::test::kChildAccountServiceFlags);
      break;
    case LogInType::kRegular:
      LogIn(kAccountId, kAccountPassword, kEmptyServices);
      break;
  }
}

Browser* SupervisedUserTestBase::browser() {
  const BrowserList* active_browser_list = BrowserList::GetInstance();
  if (active_browser_list->empty())
    return nullptr;
  Browser* browser = active_browser_list->get(0);
  return browser;
}

Profile* SupervisedUserTestBase::GetPrimaryUserProfile() {
  return chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetPrimaryUser());
}

SupervisedUserService* SupervisedUserTestBase::supervised_user_service() {
  return SupervisedUserServiceFactory::GetForProfile(GetPrimaryUserProfile());
}
