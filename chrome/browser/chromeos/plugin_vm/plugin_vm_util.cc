// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/observer_list.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_drive_image_download_service.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/exo/shell_surface_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "google_apis/drive/drive_api_error_codes.h"

namespace plugin_vm {

namespace {

static std::string& GetFakeLicenseKey() {
  static base::NoDestructor<std::string> license_key;
  return *license_key;
}

static base::CallbackList<void(void)>& GetFakeLicenceKeyListeners() {
  static base::NoDestructor<base::CallbackList<void(void)>> instance;
  return *instance;
}

static std::string& GetFakeUserId() {
  static base::NoDestructor<std::string> user_id;
  return *user_id;
}

}  // namespace

// For PluginVm to be allowed:
// * Profile should be eligible.
// * PluginVm feature should be enabled.
// * Device should be enterprise enrolled:
//   * User should be affiliated.
//   * PluginVmAllowed should be set.
// * At least one of the following should be set:
//   * PluginVmLicenseKey policy.
//   * PluginVmUserId policy.
bool IsPluginVmAllowedForProfile(const Profile* profile) {
  // Check that the profile is eligible.
  if (!profile || profile->IsChild() || profile->IsLegacySupervised() ||
      profile->IsOffTheRecord() ||
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile) ||
      !chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    return false;
  }

  // Check that PluginVm feature is enabled.
  if (!base::FeatureList::IsEnabled(features::kPluginVm))
    return false;

  // Bypass other checks when a fake policy is set
  if (FakeLicenseKeyIsSet())
    return true;

  // Check that the device is enterprise enrolled.
  if (!chromeos::InstallAttributes::Get()->IsEnterpriseManaged())
    return false;

  // Check that the user is affiliated.
  const user_manager::User* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user == nullptr || !user->IsAffiliated())
    return false;

  // Check that PluginVm is allowed to run by policy.
  bool plugin_vm_allowed_for_device;
  if (!chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kPluginVmAllowed, &plugin_vm_allowed_for_device)) {
    return false;
  }
  if (!plugin_vm_allowed_for_device)
    return false;

  if (GetPluginVmLicenseKey().empty() && GetPluginVmUserId().empty())
    return false;

  return true;
}

bool IsPluginVmConfigured(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists);
}

bool IsPluginVmEnabled(const Profile* profile) {
  return IsPluginVmAllowedForProfile(profile) && IsPluginVmConfigured(profile);
}

bool IsPluginVmRunning(Profile* profile) {
  return plugin_vm::PluginVmManagerFactory::GetForProfile(profile)
                 ->vm_state() ==
             vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING &&
         ChromeLauncherController::instance()->IsOpen(
             ash::ShelfID(kPluginVmAppId));
}

bool IsPluginVmWindow(const aura::Window* window) {
  const std::string* app_id = exo::GetShellApplicationId(window);
  if (!app_id)
    return false;
  return *app_id == "org.chromium.plugin_vm_ui";
}

std::string GetPluginVmLicenseKey() {
  if (FakeLicenseKeyIsSet())
    return GetFakeLicenseKey();
  std::string plugin_vm_license_key;
  if (!chromeos::CrosSettings::Get()->GetString(chromeos::kPluginVmLicenseKey,
                                                &plugin_vm_license_key)) {
    return std::string();
  }
  return plugin_vm_license_key;
}

std::string GetPluginVmUserId() {
  if (FakeUserIdIsSet()) {
    return GetFakeUserId();
  }

  const user_manager::User* primary_user_ref =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (primary_user_ref == nullptr) {
    return std::string();
  }

  Profile* primary_profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user_ref);
  if (primary_profile == nullptr) {
    return std::string();
  }

  return primary_profile->GetPrefs()->GetString(
      plugin_vm::prefs::kPluginVmUserId);
}

void SetFakePluginVmPolicy(Profile* profile,
                           const std::string& image_url,
                           const std::string& image_hash,
                           const std::string& license_key) {
  DictionaryPrefUpdate update(profile->GetPrefs(),
                              plugin_vm::prefs::kPluginVmImage);
  base::DictionaryValue* dict = update.Get();
  dict->SetPath("url", base::Value(image_url));
  dict->SetPath("hash", base::Value(image_hash));

  GetFakeLicenseKey() = license_key;

  GetFakeLicenceKeyListeners().Notify();
  GetFakeUserId() = "FAKE_USER_ID";
}

bool FakeLicenseKeyIsSet() {
  return !GetFakeLicenseKey().empty();
}

bool FakeUserIdIsSet() {
  return !GetFakeUserId().empty();
}

void RemoveDriveDownloadDirectoryIfExists() {
  auto log_file_deletion_if_failed = [](bool success) {
    if (!success) {
      LOG(ERROR) << "PluginVM failed to delete download directory";
    }
  };

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&base::DeleteFileRecursively,
                     base::FilePath(kPluginVmDriveDownloadDirectory)),
      base::BindOnce(std::move(log_file_deletion_if_failed)));
}

// TODO(muhamedp): Update if a different url format is ultimately chosen.
bool IsDriveUrl(const GURL& url) {
  const std::string url_base = "https://drive.google.com/open";
  const std::string& spec = url.spec();
  return spec.find(url_base) == 0 && spec.find("id=") < (spec.length() - 3);
}

// TODO(muhamedp): Update if a different url format is ultimately chosen.
std::string GetIdFromDriveUrl(const GURL& url) {
  const std::string& spec = url.spec();

  const size_t id_start = spec.find("id=") + 3;
  // In case there are other GET parameters after the id.
  const size_t first_ampersand = spec.find('&', id_start);

  if (first_ampersand == std::string::npos)
    return spec.substr(id_start);
  else
    return spec.substr(id_start, first_ampersand - id_start);
}

dlcservice::DlcModuleList GetPluginVmDlcModuleList() {
  dlcservice::DlcModuleList dlc_module_list;
  auto* dlc_module_info = dlc_module_list.add_dlc_module_infos();
  dlc_module_info->set_dlc_id("pita");
  return dlc_module_list;
}

PluginVmPolicySubscription::PluginVmPolicySubscription(
    Profile* profile,
    base::RepeatingCallback<void(bool)> callback)
    : profile_(profile), callback_(callback) {
  DCHECK(chromeos::CrosSettings::IsInitialized());
  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
  // Subscriptions are automatically removed when this object is destroyed.
  allowed_subscription_ = cros_settings->AddSettingsObserver(
      chromeos::kPluginVmAllowed,
      base::BindRepeating(&PluginVmPolicySubscription::OnPolicyChanged,
                          base::Unretained(this)));
  license_subscription_ = cros_settings->AddSettingsObserver(
      chromeos::kPluginVmLicenseKey,
      base::BindRepeating(&PluginVmPolicySubscription::OnPolicyChanged,
                          base::Unretained(this)));
  fake_license_subscription_ = GetFakeLicenceKeyListeners().Add(
      base::BindRepeating(&PluginVmPolicySubscription::OnPolicyChanged,
                          base::Unretained(this)));

  is_allowed_ = IsPluginVmAllowedForProfile(profile);
}

void PluginVmPolicySubscription::OnPolicyChanged() {
  bool allowed = IsPluginVmAllowedForProfile(profile_);
  if (allowed != is_allowed_) {
    is_allowed_ = allowed;
    callback_.Run(allowed);
  }
}

PluginVmPolicySubscription::~PluginVmPolicySubscription() = default;

}  // namespace plugin_vm
