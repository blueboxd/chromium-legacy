// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/test_permissions_client.h"

namespace permissions {
namespace {

scoped_refptr<HostContentSettingsMap> CreateSettingsMap(
    sync_preferences::TestingPrefServiceSyncable* prefs) {
  HostContentSettingsMap::RegisterProfilePrefs(prefs->registry());
  return base::MakeRefCounted<HostContentSettingsMap>(prefs, false, false,
                                                      false);
}

}  // namespace

TestPermissionsClient::TestPermissionsClient()
    : settings_map_(CreateSettingsMap(&prefs_)),
      autoblocker_(settings_map_.get()) {}

TestPermissionsClient::~TestPermissionsClient() {
  settings_map_->ShutdownOnUIThread();
}

HostContentSettingsMap* TestPermissionsClient::GetSettingsMap(
    content::BrowserContext* browser_context) {
  return settings_map_.get();
}

PermissionDecisionAutoBlocker*
TestPermissionsClient::GetPermissionDecisionAutoBlocker(
    content::BrowserContext* browser_context) {
  return &autoblocker_;
}

}  // namespace permissions
