// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_util.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/crostini/crostini_app_launch_observer.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_terminal.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_icon.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace {

constexpr char kCrostiniAppLaunchHistogram[] = "Crostini.AppLaunch";
constexpr char kCrostiniAppNamePrefix[] = "_crostini_";
constexpr int64_t kDelayBeforeSpinnerMs = 400;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrostiniAppLaunchAppType {
  // An app which isn't in the CrostiniAppRegistry. This shouldn't happen.
  kUnknownApp = 0,

  // The main terminal app.
  kTerminal = 1,

  // An app for which there is something in the CrostiniAppRegistry.
  kRegisteredApp = 2,

  kCount
};

void RecordAppLaunchHistogram(CrostiniAppLaunchAppType app_type) {
  base::UmaHistogramEnumeration(kCrostiniAppLaunchHistogram, app_type,
                                CrostiniAppLaunchAppType::kCount);
}

void OnLaunchFailed(const std::string& app_id) {
  // Remove the spinner so it doesn't stay around forever.
  // TODO(timloh): Consider also displaying a notification of some sort.
  ChromeLauncherController* chrome_controller =
      ChromeLauncherController::instance();
  DCHECK(chrome_controller);
  chrome_controller->GetShelfSpinnerController()->CloseSpinner(app_id);
}

void OnCrostiniRestarted(Profile* profile,
                         const std::string& app_id,
                         Browser* browser,
                         base::OnceClosure callback,
                         crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    OnLaunchFailed(app_id);
    if (browser && browser->window())
      browser->window()->Close();
    if (result == crostini::CrostiniResult::OFFLINE_WHEN_UPGRADE_REQUIRED) {
      ShowCrostiniUpgradeView(profile, crostini::CrostiniUISurface::kAppList);
    }
    return;
  }
  std::move(callback).Run();
}

void OnContainerApplicationLaunched(const std::string& app_id, bool success) {
  if (!success)
    OnLaunchFailed(app_id);
}

void LaunchContainerApplication(
    Profile* profile,
    const std::string& app_id,
    crostini::CrostiniRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<std::string>& files,
    bool display_scaled) {
  ChromeLauncherController* chrome_launcher_controller =
      ChromeLauncherController::instance();
  DCHECK_NE(chrome_launcher_controller, nullptr);
  CrostiniAppLaunchObserver* observer =
      chrome_launcher_controller->crostini_app_window_shelf_controller();
  DCHECK_NE(observer, nullptr);
  observer->OnAppLaunchRequested(app_id, display_id);
  crostini::CrostiniManager::GetForProfile(profile)->LaunchContainerApplication(
      registration.VmName(), registration.ContainerName(),
      registration.DesktopFileId(), files, display_scaled,
      base::BindOnce(OnContainerApplicationLaunched, app_id));
}

// Helper class for loading icons. The callback is called when all icons have
// been loaded, or after a provided timeout, after which the object deletes
// itself.
// TODO(timloh): We should consider having a service, so multiple requests for
// the same icon won't load the same image multiple times and only the first
// request would incur the loading delay.
class IconLoadWaiter : public CrostiniAppIcon::Observer {
 public:
  static void LoadIcons(
      Profile* profile,
      const std::vector<std::string>& app_ids,
      int resource_size_in_dip,
      ui::ScaleFactor scale_factor,
      base::TimeDelta timeout,
      base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)> callback) {
    new IconLoadWaiter(profile, app_ids, resource_size_in_dip, scale_factor,
                       timeout, std::move(callback));
  }

 private:
  IconLoadWaiter(
      Profile* profile,
      const std::vector<std::string>& app_ids,
      int resource_size_in_dip,
      ui::ScaleFactor scale_factor,
      base::TimeDelta timeout,
      base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)> callback)
      : callback_(std::move(callback)) {
    for (const std::string& app_id : app_ids) {
      icons_.push_back(std::make_unique<CrostiniAppIcon>(
          profile, app_id, resource_size_in_dip, this));
      icons_.back()->LoadForScaleFactor(scale_factor);
    }

    timeout_timer_.Start(FROM_HERE, timeout, this,
                         &IconLoadWaiter::RunCallback);
  }

  // TODO(timloh): This is only called when an icon is found, so if any of the
  // requested apps are missing an icon, we'll have to wait for the timeout. We
  // should add an interface so we can avoid this.
  void OnIconUpdated(CrostiniAppIcon* icon) override {
    loaded_icons_++;
    if (loaded_icons_ != icons_.size())
      return;

    RunCallback();
  }

  void Delete() {
    DCHECK(!timeout_timer_.IsRunning());
    delete this;
  }

  void RunCallback() {
    DCHECK(callback_);
    std::vector<gfx::ImageSkia> result;
    for (const auto& icon : icons_)
      result.emplace_back(icon->image_skia());
    std::move(callback_).Run(result);

    // If we're running the callback as loading has finished, we can't delete
    // ourselves yet as it would destroy the CrostiniAppIcon which is calling
    // into us right now. If we hit the timeout, we delete immediately to avoid
    // any race with more icons finishing loading.
    if (timeout_timer_.IsRunning()) {
      timeout_timer_.AbandonAndStop();
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&IconLoadWaiter::Delete, base::Unretained(this)));
    } else {
      Delete();
    }
  }

  std::vector<std::unique_ptr<CrostiniAppIcon>> icons_;
  size_t loaded_icons_ = 0;

  base::OneShotTimer timeout_timer_;

  base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)> callback_;
};

bool IsCrostiniAllowedForProfileImpl(Profile* profile) {
  if (!profile || profile->IsChild() || profile->IsLegacySupervised() ||
      profile->IsOffTheRecord() ||
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return false;
  }
  if (!crostini::CrostiniManager::IsDevKvmPresent()) {
    // Hardware is physically incapable, no matter what the user wants.
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
    return false;
  }

  return base::FeatureList::IsEnabled(features::kCrostini);
}

}  // namespace

namespace crostini {

std::string ContainerIdToString(const ContainerId& container_id) {
  return base::StrCat(
      {"(", container_id.first, ", ", container_id.second, ")"});
}

bool IsCrostiniAllowedForProfile(Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!IsUnaffiliatedCrostiniAllowedByPolicy() && !user->IsAffiliated()) {
    return false;
  }
  if (!profile->GetPrefs()->GetBoolean(
          crostini::prefs::kUserCrostiniAllowedByPolicy)) {
    return false;
  }
  if (!virtual_machines::AreVirtualMachinesAllowedByPolicy()) {
    return false;
  }
  return IsCrostiniAllowedForProfileImpl(profile);
}

bool IsCrostiniUIAllowedForProfile(Profile* profile, bool check_policy) {
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    return false;
  }
  if (check_policy) {
    return IsCrostiniAllowedForProfile(profile);
  }
  return IsCrostiniAllowedForProfileImpl(profile);
}

bool IsCrostiniExportImportUIAllowedForProfile(Profile* profile) {
  return IsCrostiniUIAllowedForProfile(profile, true) &&
         base::FeatureList::IsEnabled(chromeos::features::kCrostiniBackup) &&
         profile->GetPrefs()->GetBoolean(
             crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy);
}

bool IsCrostiniEnabled(Profile* profile) {
  return IsCrostiniUIAllowedForProfile(profile) &&
         profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled);
}

bool IsCrostiniRunning(Profile* profile) {
  return crostini::CrostiniManager::GetForProfile(profile)->IsVmRunning(
      kCrostiniDefaultVmName);
}

bool IsCrostiniAnsibleInfrastructureEnabled() {
  return base::FeatureList::IsEnabled(features::kCrostiniAnsibleInfrastructure);
}

void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id) {
  LaunchCrostiniApp(profile, app_id, display_id, std::vector<std::string>());
}

void AddSpinner(crostini::CrostiniManager::RestartId restart_id,
                const std::string& app_id,
                Profile* profile,
                std::string vm_name,
                std::string container_name) {
  ChromeLauncherController* chrome_controller =
      ChromeLauncherController::instance();
  if (chrome_controller &&
      crostini::CrostiniManager::GetForProfile(profile)->IsRestartPending(
          restart_id)) {
    chrome_controller->GetShelfSpinnerController()->AddSpinnerToShelf(
        app_id, std::make_unique<ShelfSpinnerItemController>(app_id));
  }
}

void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       const std::vector<std::string>& files) {
  // Policies can change under us, and crostini may now be forbidden.
  if (!IsCrostiniUIAllowedForProfile(profile)) {
    return;
  }
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile);
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  base::Optional<crostini::CrostiniRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (!registration) {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kUnknownApp);
    LOG(ERROR) << "LaunchCrostiniApp called with an unknown app_id: " << app_id;
    return;
  }

  // Store these as we move |registration| into LaunchContainerApplication().
  const std::string vm_name = registration->VmName();
  const std::string container_name = registration->ContainerName();

  base::OnceClosure launch_closure;
  Browser* browser = nullptr;
  if (app_id == kCrostiniTerminalId) {
    DCHECK(files.empty());
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kTerminal);

    // At this point, we know that Crostini UI is allowed.
    if (!crostini_manager->IsCrosTerminaInstalled() ||
        !profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled)) {
      ShowCrostiniInstallerView(profile, CrostiniUISurface::kAppList);
      return;
    }

    GURL vsh_in_crosh_url = GenerateVshInCroshUrl(
        profile, vm_name, container_name, std::vector<std::string>());
    AppLaunchParams launch_params = GenerateTerminalAppLaunchParams(profile);
    // Create the terminal here so it's created in the right display. If the
    // browser creation is delayed into the callback the root window for new
    // windows setting can be changed due to the launcher or shelf dismissal.
    Browser* browser = CreateContainerTerminal(launch_params, vsh_in_crosh_url);
    launch_closure = base::BindOnce(&ShowContainerTerminal, launch_params,
                                    vsh_in_crosh_url, browser);
  } else {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kRegisteredApp);
    launch_closure = base::BindOnce(
        &LaunchContainerApplication, profile, app_id, std::move(*registration),
        display_id, std::move(files), registration->IsScaled());
  }

  // Update the last launched time and Termina version.
  registry_service->AppLaunched(app_id);
  crostini_manager->UpdateLaunchMetricsForEnterpriseReporting();

  auto restart_id = crostini_manager->RestartCrostini(
      vm_name, container_name,
      base::BindOnce(OnCrostiniRestarted, profile, app_id, browser,
                     std::move(launch_closure)));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AddSpinner, restart_id, app_id, profile, vm_name,
                     container_name),
      base::TimeDelta::FromMilliseconds(kDelayBeforeSpinnerMs));
}

void LoadIcons(Profile* profile,
               const std::vector<std::string>& app_ids,
               int resource_size_in_dip,
               ui::ScaleFactor scale_factor,
               base::TimeDelta timeout,
               base::OnceCallback<void(const std::vector<gfx::ImageSkia>&)>
                   icons_loaded_callback) {
  IconLoadWaiter::LoadIcons(profile, app_ids, resource_size_in_dip,
                            scale_factor, timeout,
                            std::move(icons_loaded_callback));
}

std::string CryptohomeIdForProfile(Profile* profile) {
  std::string id = chromeos::ProfileHelper::GetUserIdHashFromProfile(profile);
  // Empty id means we're running in a test.
  return id.empty() ? "test" : id;
}

std::string DefaultContainerUserNameForProfile(Profile* profile) {
  // Get rid of the @domain.name in the profile user name (an email address).
  std::string container_username = profile->GetProfileUserName();
  if (container_username.find('@') != std::string::npos) {
    // gaia::CanonicalizeEmail CHECKs its argument contains'@'.
    container_username = gaia::CanonicalizeEmail(container_username);
    // |container_username| may have changed, so we have to find again.
    return container_username.substr(0, container_username.find('@'));
  }
  return container_username;
}

base::FilePath ContainerChromeOSBaseDirectory() {
  return base::FilePath("/mnt/chromeos");
}

std::string AppNameFromCrostiniAppId(const std::string& id) {
  return kCrostiniAppNamePrefix + id;
}

base::Optional<std::string> CrostiniAppIdFromAppName(
    const std::string& app_name) {
  if (!base::StartsWith(app_name, kCrostiniAppNamePrefix,
                        base::CompareCase::SENSITIVE)) {
    return base::nullopt;
  }
  return app_name.substr(strlen(kCrostiniAppNamePrefix));
}

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

void AddNewLxdContainerToPrefs(Profile* profile,
                               std::string vm_name,
                               std::string container_name) {
  auto* pref_service = profile->GetPrefs();

  base::Value new_container(base::Value::Type::DICTIONARY);
  new_container.SetKey(prefs::kVmKey, base::Value(vm_name));
  new_container.SetKey(prefs::kContainerKey, base::Value(container_name));

  ListPrefUpdate updater(pref_service, crostini::prefs::kCrostiniContainers);
  updater->GetList().emplace_back(std::move(new_container));
}

void RemoveLxdContainerFromPrefs(Profile* profile,
                                 std::string vm_name,
                                 std::string container_name) {
  auto* pref_service = profile->GetPrefs();
  ListPrefUpdate updater(pref_service, crostini::prefs::kCrostiniContainers);

  for (auto it = updater->GetList().begin(); it != updater->GetList().end();
       it++) {
    auto* vm_name_test = it->FindKey(prefs::kVmKey);
    auto* container_name_test = it->FindKey(prefs::kContainerKey);
    if (vm_name_test->GetString() == vm_name &&
        container_name_test->GetString() == container_name) {
      updater->GetList().erase(it);
      break;
    }
  }
  CrostiniRegistryServiceFactory::GetForProfile(profile)->ClearApplicationList(
      vm_name, container_name);
  CrostiniMimeTypesServiceFactory::GetForProfile(profile)->ClearMimeTypes(
      vm_name, container_name);
}

base::string16 GetTimeRemainingMessage(base::TimeTicks start, int percent) {
  // Only estimate once we've spent at least 3 seconds OR gotten 10% of the way
  // through.
  constexpr base::TimeDelta kMinTimeForEstimate =
      base::TimeDelta::FromSeconds(3);
  constexpr base::TimeDelta kTimeDeltaZero = base::TimeDelta::FromSeconds(0);
  constexpr int kMinPercentForEstimate = 10;
  base::TimeDelta elapsed = base::TimeTicks::Now() - start;
  if ((elapsed >= kMinTimeForEstimate && percent > 0) ||
      (percent >= kMinPercentForEstimate && elapsed > kTimeDeltaZero)) {
    base::TimeDelta total_time_expected = (elapsed * 100) / percent;
    base::TimeDelta time_remaining = total_time_expected - elapsed;
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                  ui::TimeFormat::LENGTH_SHORT, time_remaining);
  } else {
    return l10n_util::GetStringUTF16(
        IDS_CROSTINI_NOTIFICATION_OPERATION_STARTING);
  }
}

}  // namespace crostini
