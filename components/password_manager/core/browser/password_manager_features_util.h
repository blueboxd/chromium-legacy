// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_FEATURES_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_FEATURES_UTIL_H_

#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"

namespace syncer {
class SyncService;
}

class PrefService;

namespace password_manager {
namespace features_util {

// Whether the current signed-in user (aka unconsented primary account) has
// opted in to use the Google account storage for passwords (as opposed to
// local/profile storage).
// |pref_service| must not be null.
// |sync_service| may be null (commonly the case in incognito mode), in which
// case this will simply return false.
// See PasswordFeatureManager::IsOptedInForAccountStorage.
bool IsOptedInForAccountStorage(const PrefService* pref_service,
                                const syncer::SyncService* sync_service);

// Whether it makes sense to ask the user to opt-in for account-based
// password storage. This is true if the opt-in doesn't exist yet, but all
// other requirements are met (i.e. there is a signed-in user, Sync-the-feature
// is not enabled, etc).
// |pref_service| must not be null.
// |sync_service| may be null (commonly the case in incognito mode), in which
// case this will simply return false.
// See PasswordFeatureManager::ShouldShowAccountStorageOptIn.
bool ShouldShowAccountStorageOptIn(const PrefService* pref_service,
                                   const syncer::SyncService* sync_service);

// Whether it makes sense to ask the user to signin again to access the
// account-based password storage. This is true if a user on this device
// previously opted into using the account store but is signed-out now.
// See PasswordFeatureManager::ShouldShowAccountStorageReSignin.
bool ShouldShowAccountStorageReSignin(const PrefService* pref_service,
                                      const syncer::SyncService* sync_service);

// Sets opt-in to using account storage for passwords for the current
// signed-in user (unconsented primary account).
// |pref_service| and |sync_service| must not be null.
// See PasswordFeatureManager::OptInToAccountStorage.
void OptInToAccountStorage(PrefService* pref_service,
                           const syncer::SyncService* sync_service);

// Clears the opt-in to using account storage for passwords for the
// current signed-in user (unconsented primary account), as well as all other
// associated settings (e.g. default store choice).
// |pref_service| and |sync_service| must not be null.
// See PasswordFeatureManager::OptOutOfAccountStorageAndClearSettings.
void OptOutOfAccountStorageAndClearSettings(
    PrefService* pref_service,
    const syncer::SyncService* sync_service);

// Whether it makes sense to ask the user about the store when saving a
// password (i.e. profile or account store). This is true if the user has
// opted in already, or hasn't opted in but all other requirements are met (i.e.
// there is a signed-in user, Sync-the-feature is not enabled, etc).
// |pref_service| must not be null.
// |sync_service| may be null (commonly the case in incognito mode), in which
// case this will simply return false.
// See PasswordFeatureManager::ShouldShowPasswordStorePicker.
bool ShouldShowPasswordStorePicker(const PrefService* pref_service,
                                   const syncer::SyncService* sync_service);

// Sets the default storage location for signed-in but non-syncing users. This
// store is used for saving new credentials and adding blacking listing entries.
// |pref_service| and |sync_service| must not be null.
// See PasswordFeatureManager::SetDefaultPasswordStore.
void SetDefaultPasswordStore(PrefService* pref_service,
                             const syncer::SyncService* sync_service,
                             autofill::PasswordForm::Store default_store);

// Returns the default storage location for signed-in but non-syncing users
// (i.e. will new passwords be saved to locally or to the account by default).
// Always returns an actual value, never kNotSet.
// |pref_service| must not be null.
// |sync_service| may be null (commonly the case in incognito mode), in which
// case this will return kProfileStore.
// See PasswordFeatureManager::GetDefaultPasswordStore.
autofill::PasswordForm::Store GetDefaultPasswordStore(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service);

// Clears all account-storage-related settings for all users. Most notably, this
// includes the opt-in, but also all other related settings like the default
// password store. Meant to be called when account cookies were cleared.
// |pref_service| must not be null.
void ClearAccountStorageSettingsForAllUsers(PrefService* pref_service);

// See definition of PasswordAccountStorageUserState.
metrics_util::PasswordAccountStorageUserState
ComputePasswordAccountStorageUserState(const PrefService* pref_service,
                                       const syncer::SyncService* sync_service);

// Returns the "usage level" of the account-scoped password storage. See
// definition of PasswordAccountStorageUsageLevel.
// See PasswordFeatureManager::ComputePasswordAccountStorageUsageLevel.
password_manager::metrics_util::PasswordAccountStorageUsageLevel
ComputePasswordAccountStorageUsageLevel(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service);

}  // namespace features_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_FEATURES_UTIL_H_
