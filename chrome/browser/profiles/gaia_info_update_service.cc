// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

GAIAInfoUpdateService::GAIAInfoUpdateService(
    signin::IdentityManager* identity_manager,
    ProfileAttributesStorage* profile_attributes_storage,
    const base::FilePath& profile_path,
    PrefService* profile_prefs)
    : identity_manager_(identity_manager),
      profile_attributes_storage_(profile_attributes_storage),
      profile_path_(profile_path),
      profile_prefs_(profile_prefs) {
  identity_manager_->AddObserver(this);

  if (!ShouldUpdate()) {
    ClearProfileEntry();
    return;
  }
  ProfileAttributesEntry* entry;
  if (!profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_,
                                                                 &entry)) {
    return;
  }

  gaia_id_of_profile_attribute_entry_ = entry->GetGAIAId();
}

GAIAInfoUpdateService::~GAIAInfoUpdateService() = default;

void GAIAInfoUpdateService::Update() {
  if (!ShouldUpdate())
    return;

  auto unconsented_primary_account_info =
      identity_manager_->GetUnconsentedPrimaryAccountInfo();

  if (!gaia_id_of_profile_attribute_entry_.empty() &&
      unconsented_primary_account_info.gaia !=
          gaia_id_of_profile_attribute_entry_) {
    ClearProfileEntry();
  }

  auto maybe_account_info =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              identity_manager_->GetUnconsentedPrimaryAccountInfo().account_id);
  if (maybe_account_info.has_value())
    Update(maybe_account_info.value());
}

void GAIAInfoUpdateService::Update(const AccountInfo& info) {
  if (!info.IsValid())
    return;

  ProfileAttributesEntry* entry;
  if (!profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_,
                                                                 &entry)) {
    return;
  }
  gaia_id_of_profile_attribute_entry_ = info.gaia;
  entry->SetGAIAGivenName(base::UTF8ToUTF16(info.given_name));
  entry->SetGAIAName(base::UTF8ToUTF16(info.full_name));

  const base::string16 hosted_domain = base::UTF8ToUTF16(info.hosted_domain);
  profile_prefs_->SetString(prefs::kGoogleServicesHostedDomain,
                            base::UTF16ToUTF8(hosted_domain));

  if (info.picture_url == kNoPictureURLFound) {
    entry->SetGAIAPicture(gfx::Image());
  } else if (!info.account_image.IsEmpty()) {
    if (info.account_image.ToSkBitmap()->width() !=
        signin::kAccountInfoImageSize) {
      // All newly downloaded images should be of
      // |signin::kAccountInfoImageSize| size.
      return;
    }
    // Only set the image if it is not empty, to avoid clearing the image if we
    // fail to download it on one of the 24 hours interval to refresh the data.
    entry->SetGAIAPicture(info.account_image);
  }
}

// static
bool GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(Profile* profile) {
#if defined(OS_CHROMEOS)
  return false;
#endif
  return true;
}

void GAIAInfoUpdateService::ClearProfileEntry() {
  ProfileAttributesEntry* entry;
  if (!profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_,
                                                                 &entry)) {
    return;
  }
  gaia_id_of_profile_attribute_entry_ = "";
  entry->SetGAIAName(base::string16());
  entry->SetGAIAGivenName(base::string16());
  entry->SetGAIAPicture(gfx::Image());
  // Unset the cached URL.
  profile_prefs_->ClearPref(prefs::kGoogleServicesHostedDomain);
}

void GAIAInfoUpdateService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

void GAIAInfoUpdateService::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  Update();
}

void GAIAInfoUpdateService::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  ClearProfileEntry();
}

void GAIAInfoUpdateService::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
  if (identity_manager_->HasPrimaryAccount())
    return;

  if (unconsented_primary_account_info.gaia.empty()) {
    ClearProfileEntry();
  } else {
    Update();
  }
}

void GAIAInfoUpdateService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (!ShouldUpdate())
    return;

  CoreAccountInfo account_info =
      identity_manager_->GetUnconsentedPrimaryAccountInfo();

  if (info.account_id != account_info.account_id)
    return;

  Update(info);
}

bool GAIAInfoUpdateService::ShouldUpdate() {
  return identity_manager_->HasPrimaryAccount() ||
         (identity_manager_->HasUnconsentedPrimaryAccount() &&
          base::FeatureList::IsEnabled(kPersistUPAInProfileInfoCache));
}
