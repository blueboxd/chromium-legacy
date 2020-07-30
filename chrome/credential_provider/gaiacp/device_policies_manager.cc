// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/device_policies_manager.h"

#include <limits>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/user_policies_manager.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {

// static
DevicePoliciesManager* DevicePoliciesManager::Get() {
  return *GetInstanceStorage();
}

// static
DevicePoliciesManager** DevicePoliciesManager::GetInstanceStorage() {
  static DevicePoliciesManager instance;
  static DevicePoliciesManager* instance_storage = &instance;
  return &instance_storage;
}

DevicePoliciesManager::DevicePoliciesManager() {}

DevicePoliciesManager::~DevicePoliciesManager() = default;

bool DevicePoliciesManager::CloudPoliciesEnabled() const {
  return UserPoliciesManager::Get()->CloudPoliciesEnabled();
}

void DevicePoliciesManager::GetDevicePolicies(DevicePolicies* device_policies) {
  bool found_existing_user = false;
  UserPoliciesManager* user_policies_manager = UserPoliciesManager::Get();
  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName);
  for (; iter.Valid(); ++iter) {
    base::string16 sid(iter.Name());
    // Check if this account with this sid exists on device.
    HRESULT hr = OSUserManager::Get()->FindUserBySID(sid.c_str(), nullptr, 0,
                                                     nullptr, 0);
    if (hr != S_OK) {
      if (hr != HRESULT_FROM_WIN32(ERROR_NONE_MAPPED)) {
        LOGFN(ERROR) << "FindUserBySID hr=" << putHR(hr);
      } else {
        LOGFN(WARNING) << sid << " is not a valid sid";
      }
      continue;
    }

    UserPolicies user_policies;
    if (!user_policies_manager->GetUserPolicies(sid, &user_policies)) {
      LOGFN(ERROR) << "Failed to read user policies for " << sid;
      continue;
    }

    // We need to first find a single active user whose policies we start with
    // and then merge with the policies of other users.
    if (!found_existing_user) {
      *device_policies = DevicePolicies::FromUserPolicies(user_policies);
      found_existing_user = true;
    } else {
      DevicePolicies other_policies =
          DevicePolicies::FromUserPolicies(user_policies);
      device_policies->MergeWith(other_policies);
    }
  }
}

}  // namespace credential_provider
