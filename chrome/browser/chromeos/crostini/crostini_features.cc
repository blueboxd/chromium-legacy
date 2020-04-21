// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_features.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"

namespace {

bool IsUnaffiliatedCrostiniAllowedByPolicy() {
  bool unaffiliated_crostini_allowed;
  if (chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kDeviceUnaffiliatedCrostiniAllowed,
          &unaffiliated_crostini_allowed)) {
    return unaffiliated_crostini_allowed;
  }
  // If device policy is not set, allow Crostini.
  return true;
}

bool IsAllowedImpl(Profile* profile) {
  if (!profile || profile->IsChild() || profile->IsLegacySupervised() ||
      profile->IsOffTheRecord() ||
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    VLOG(1) << "Profile is not allowed to run crostini.";
    return false;
  }
  if (!crostini::CrostiniManager::IsDevKvmPresent()) {
    // Hardware is physically incapable, no matter what the user wants.
    VLOG(1) << "Cannot run crostini because /dev/kvm is not present.";
    return false;
  }

  bool kernelnext = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kKernelnextRestrictVMs);
  bool kernelnext_override =
      base::FeatureList::IsEnabled(features::kKernelnextVMs);
  if (kernelnext && !kernelnext_override) {
    // The host kernel is on an experimental version. In future updates this
    // device may not have VM support, so we allow enabling VMs, but guard them
    // on a chrome://flags switch (enable-experimental-kernel-vm-support).
    VLOG(1) << "Cannot run crostini on experimental kernel without "
            << "--enable-experimental-kernel-vm-support.";
    return false;
  }

  if (!base::FeatureList::IsEnabled(features::kCrostini)) {
    VLOG(1) << "Crostini is not enabled in feature list.";
    return false;
  }
  return true;
}

bool IsArcManagedAdbSideloadingSupported(bool is_device_enterprise_managed,
                                         bool is_profile_enterprise_managed,
                                         bool is_owner_profile) {
  DCHECK(is_device_enterprise_managed || is_profile_enterprise_managed);

  if (!base::FeatureList::IsEnabled(
          chromeos::features::kArcManagedAdbSideloadingSupport)) {
    DVLOG(1) << "adb sideloading is disabled by a feature flag";
    return false;
  }

  if (is_device_enterprise_managed) {
    // TODO(janagrill): Add check for device policy
    if (is_profile_enterprise_managed) {
      // TODO(janagrill): Add check for affiliated user
      // TODO(janagrill): Add check for user policy
      return true;
    }

    DVLOG(1) << "adb sideloading is unsupported for this managed device";
    return false;
  }

  if (is_owner_profile) {
    // We know here that the profile is enterprise-managed so no need to check
    // TODO(janagrill): Add check for user policy
    return true;
  }

  DVLOG(1) << "Only the owner can change adb sideloading status";
  return false;
}

}  // namespace

namespace crostini {

static CrostiniFeatures* g_crostini_features = nullptr;

CrostiniFeatures* CrostiniFeatures::Get() {
  if (!g_crostini_features) {
    g_crostini_features = new CrostiniFeatures();
  }
  return g_crostini_features;
}

void CrostiniFeatures::SetForTesting(CrostiniFeatures* features) {
  g_crostini_features = features;
}

CrostiniFeatures::CrostiniFeatures() = default;

CrostiniFeatures::~CrostiniFeatures() = default;

bool CrostiniFeatures::IsAllowed(Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!IsUnaffiliatedCrostiniAllowedByPolicy() && !user->IsAffiliated()) {
    VLOG(1) << "Policy blocks unaffiliated user from running Crostini.";
    return false;
  }
  if (!profile->GetPrefs()->GetBoolean(
          crostini::prefs::kUserCrostiniAllowedByPolicy)) {
    VLOG(1) << "kUserCrostiniAllowedByPolicy preference is false.";
    return false;
  }
  if (!virtual_machines::AreVirtualMachinesAllowedByPolicy()) {
    VLOG(1)
        << "Crostini cannot run as virtual machines are not allowed by policy.";
    return false;
  }
  return IsAllowedImpl(profile);
}

bool CrostiniFeatures::IsUIAllowed(Profile* profile, bool check_policy) {
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    VLOG(1) << "Crostini UI is not allowed on non-primary profiles.";
    return false;
  }
  if (check_policy) {
    return g_crostini_features->IsAllowed(profile);
  }
  return IsAllowedImpl(profile);
}

bool CrostiniFeatures::IsEnabled(Profile* profile) {
  return g_crostini_features->IsUIAllowed(profile) &&
         profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled);
}

bool CrostiniFeatures::IsExportImportUIAllowed(Profile* profile) {
  return g_crostini_features->IsUIAllowed(profile, true) &&
         profile->GetPrefs()->GetBoolean(
             crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy);
}

bool CrostiniFeatures::IsRootAccessAllowed(Profile* profile) {
  if (base::FeatureList::IsEnabled(features::kCrostiniAdvancedAccessControls)) {
    return profile->GetPrefs()->GetBoolean(
        crostini::prefs::kUserCrostiniRootAccessAllowedByPolicy);
  }
  return true;
}

bool CrostiniFeatures::IsContainerUpgradeUIAllowed(Profile* profile) {
  return g_crostini_features->IsUIAllowed(profile, true) &&
         base::FeatureList::IsEnabled(
             chromeos::features::kCrostiniWebUIUpgrader);
}

bool CrostiniFeatures::CanChangeAdbSideloading(Profile* profile) {
  // First rule out a child account as it is a special case - a child can be an
  // owner, but ADB sideloading is currently not supported for this case
  if (profile->IsChild()) {
    DVLOG(1) << "adb sideloading is currently unsupported for child accounts";
    return false;
  }

  // Check the managed device and/or user case
  auto* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool is_device_enterprise_managed = connector->IsEnterpriseManaged();
  bool is_profile_enterprise_managed =
      profile->GetProfilePolicyConnector()->IsManaged();
  bool is_owner_profile = chromeos::ProfileHelper::IsOwnerProfile(profile);
  if (is_device_enterprise_managed || is_profile_enterprise_managed) {
    return IsArcManagedAdbSideloadingSupported(is_device_enterprise_managed,
                                               is_profile_enterprise_managed,
                                               is_owner_profile);
  }

  // Here we are sure that the user is not enterprise-managed and we therefore
  // only check whether the user is the owner
  if (!is_owner_profile) {
    DVLOG(1) << "Only the owner can change adb sideloading status";
    return false;
  }

  return true;
}

}  // namespace crostini
