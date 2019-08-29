// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DIALOG_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DIALOG_UTILS_H_

#include <type_traits>

#include "base/util/type_safety/strong_alias.h"

namespace password_manager {

// Defines possible scenarios for leaked credentials.
enum CredentialLeakFlags {
  // Password is saved for current site.
  kPasswordSaved = 1 << 0,
  // Password is reused on other sites.
  kPasswordUsedOnOtherSites = 1 << 1,
  // User is syncing passwords with normal encryption.
  kSyncingPasswordsNormally = 1 << 2,
};

// Contains combination of CredentialLeakFlags values.
using CredentialLeakType = std::underlying_type_t<CredentialLeakFlags>;

using IsSaved = util::StrongAlias<class IsSavedTag, bool>;
using IsReused = util::StrongAlias<class IsReusedTag, bool>;
using IsSyncing = util::StrongAlias<class IsSyncingTag, bool>;

// Creates CredentialLeakType from strong booleans.
CredentialLeakType CreateLeakType(IsSaved is_saved,
                                  IsReused is_reused,
                                  IsSyncing is_syncing);

// Checks whether password is saved in chrome.
bool IsPasswordSaved(CredentialLeakType leak_type);

// Checks whether password is reused on other sites.
bool IsPasswordUsedOnOtherSites(CredentialLeakType leak_type);

// Checks whether user is syncing passwords with normal encryption.
bool IsSyncingPasswordsNormally(CredentialLeakType leak_type);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DIALOG_UTILS_H_
