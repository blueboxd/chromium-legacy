// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/minimum_version_policy_handler_delegate_impl.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"

namespace policy {

MinimumVersionPolicyHandlerDelegateImpl::
    MinimumVersionPolicyHandlerDelegateImpl() {}

bool MinimumVersionPolicyHandlerDelegateImpl::IsKioskMode() const {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp();
}

bool MinimumVersionPolicyHandlerDelegateImpl::IsEnterpriseManaged() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->IsEnterpriseManaged();
}

const base::Version&
MinimumVersionPolicyHandlerDelegateImpl::GetCurrentVersion() const {
  return version_info::GetVersion();
}

}  // namespace policy
