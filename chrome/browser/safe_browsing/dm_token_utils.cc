// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/dm_token_utils.h"

#include "chrome/browser/profiles/profile.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/user_manager/user.h"
#else
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_controller.h"
#endif

namespace safe_browsing {

namespace {

policy::DMToken* GetTestingDMTokenStorage() {
  static policy::DMToken dm_token =
      policy::DMToken::CreateEmptyTokenForTesting();
  return &dm_token;
}

}  // namespace

policy::DMToken GetDMToken(Profile* profile) {
  policy::DMToken dm_token = *GetTestingDMTokenStorage();

#if defined(OS_CHROMEOS)
  if (!profile)
    return dm_token;
  auto* policy_manager = profile->GetUserCloudPolicyManagerChromeOS();
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (dm_token.is_empty() && user && user->IsAffiliated() && policy_manager &&
      policy_manager->IsClientRegistered()) {
    dm_token = policy::DMToken(policy::DMToken::Status::kValid,
                               policy_manager->core()->client()->dm_token());
  }
#else
  if (dm_token.is_empty() &&
      policy::ChromeBrowserCloudManagementController::IsEnabled()) {
    dm_token = policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  }
#endif

  return dm_token;
}

void SetDMTokenForTesting(const policy::DMToken& dm_token) {
  *GetTestingDMTokenStorage() = dm_token;
}

}  // namespace safe_browsing
