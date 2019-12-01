// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"

#include <Security/Security.h>

#include <algorithm>
#include <set>
#include <utility>

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/launcher.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/apps/app_shim/app_shim_termination_manager.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/apps/platform_apps/app_shim_registry_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut_mac.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "ui/base/cocoa/focus_window_set.h"

using extensions::AppWindow;
using extensions::AppWindowRegistry;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::NativeAppWindow;

namespace {

typedef AppWindowRegistry::AppWindowList AppWindowList;

// Create a SHA1 hex digest of a certificate, for use specifically in building
// a code signing requirement string in IsAcceptablyCodeSigned(), below.
std::string CertificateSHA1Digest(SecCertificateRef certificate) {
  base::ScopedCFTypeRef<CFDataRef> certificate_data(
      SecCertificateCopyData(certificate));
  char hash[base::kSHA1Length];
  base::SHA1HashBytes(CFDataGetBytePtr(certificate_data),
                      CFDataGetLength(certificate_data),
                      reinterpret_cast<unsigned char*>(hash));
  return base::HexEncode(hash, base::kSHA1Length);
}

// Returns whether |pid|'s code signature is trusted:
// - True if the caller is unsigned (there's nothing to verify).
// - True if |pid| satisfies the caller's designated requirement.
// - False otherwise (|pid| does not satisfy caller's designated requirement).
bool IsAcceptablyCodeSignedInternal(pid_t pid) {
  base::ScopedCFTypeRef<SecCodeRef> own_code;
  base::ScopedCFTypeRef<CFDictionaryRef> own_signing_info;

  // Fetch the calling process's designated requirement. The shim can only be
  // validated if the caller has one (i.e. if the caller is code signed).
  //
  // Note: Don't validate |own_code|: updates modify the browser's bundle and
  // invalidate its code signature while an update is pending. This can be
  // revisited after https://crbug.com/496298 is resolved.
  if (SecCodeCopySelf(kSecCSDefaultFlags, own_code.InitializeInto()) !=
          errSecSuccess ||
      SecCodeCopySigningInformation(own_code.get(), kSecCSSigningInformation,
                                    own_signing_info.InitializeInto()) !=
          errSecSuccess) {
    LOG(ERROR) << "Failed to get own code signing information.";
    return false;
  }

  auto* own_certificates = base::mac::GetValueFromDictionary<CFArrayRef>(
      own_signing_info, kSecCodeInfoCertificates);
  if (!own_certificates || CFArrayGetCount(own_certificates) < 1) {
    return true;
  }

  auto* own_certificate = base::mac::CFCast<SecCertificateRef>(
      CFArrayGetValueAtIndex(own_certificates, 0));
  auto own_certificate_hash = CertificateSHA1Digest(own_certificate);

  base::ScopedCFTypeRef<CFStringRef> shim_requirement_string(
      CFStringCreateWithFormat(
          kCFAllocatorDefault, nullptr,
          CFSTR(
              "identifier \"app_mode_loader\" and certificate leaf = H\"%s\""),
          own_certificate_hash.c_str()));

  base::ScopedCFTypeRef<SecRequirementRef> shim_requirement;
  if (SecRequirementCreateWithString(
          shim_requirement_string, kSecCSDefaultFlags,
          shim_requirement.InitializeInto()) != errSecSuccess) {
    LOG(ERROR)
        << "Failed to create a SecRequirementRef from the requirement string \""
        << shim_requirement_string << "\"";
    return false;
  }

  base::ScopedCFTypeRef<SecCodeRef> guest_code;

  base::ScopedCFTypeRef<CFNumberRef> pid_cf(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid));
  const void* guest_attribute_keys[] = {kSecGuestAttributePid};
  const void* guest_attribute_values[] = {pid_cf};
  base::ScopedCFTypeRef<CFDictionaryRef> guest_attributes(CFDictionaryCreate(
      nullptr, guest_attribute_keys, guest_attribute_values,
      base::size(guest_attribute_keys), &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
  if (SecCodeCopyGuestWithAttributes(nullptr, guest_attributes,
                                     kSecCSDefaultFlags,
                                     guest_code.InitializeInto())) {
    LOG(ERROR) << "Failed to create a SecCodeRef from the app shim's pid.";
    return false;
  }

  return SecCodeCheckValidity(guest_code, kSecCSDefaultFlags,
                              shim_requirement) == errSecSuccess;
}

// Attempts to launch a packaged app, prompting the user to enable it if
// necessary. The prompt is shown in its own window.
// This class manages its own lifetime.
class EnableViaPrompt : public ExtensionEnableFlowDelegate {
 public:
  EnableViaPrompt(Profile* profile,
                  const std::string& extension_id,
                  base::OnceCallback<void()> callback)
      : profile_(profile),
        extension_id_(extension_id),
        callback_(std::move(callback)) {}

  void Run() {
    flow_.reset(new ExtensionEnableFlow(profile_, extension_id_, this));
    flow_->Start();
  }

 private:
  ~EnableViaPrompt() override { std::move(callback_).Run(); }

  // ExtensionEnableFlowDelegate overrides.
  void ExtensionEnableFlowFinished() override {
    delete this;
  }
  void ExtensionEnableFlowAborted(bool user_initiated) override {
    delete this;
  }

  Profile* profile_;
  std::string extension_id_;
  base::OnceCallback<void()> callback_;
  std::unique_ptr<ExtensionEnableFlow> flow_;

  DISALLOW_COPY_AND_ASSIGN(EnableViaPrompt);
};

bool UsesRemoteViews(const extensions::Extension* extension) {
  return extension->is_hosted_app() && extension->from_bookmark();
}

bool ProfileMenuItemComparator(const chrome::mojom::ProfileMenuItemPtr& a,
                               const chrome::mojom::ProfileMenuItemPtr& b) {
  return a->menu_index < b->menu_index;
}

}  // namespace

namespace apps {

// The state for an individual (app, Profile) pair. This includes the
// AppShimHost.
struct ExtensionAppShimHandler::ProfileState {
  ProfileState(ExtensionAppShimHandler::AppState* in_app_state,
               std::unique_ptr<AppShimHost> in_single_profile_host);
  ~ProfileState() = default;

  AppShimHost* GetHost() const;

  // Weak, owns |this|.
  ExtensionAppShimHandler::AppState* const app_state;

  // The AppShimHost for apps that are not multi-profile.
  const std::unique_ptr<AppShimHost> single_profile_host;

  // All browser instances for this (app, Profile) pair.
  std::set<Browser*> browsers;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileState);
};

// The state for an individual app. This includes the state for all
// profiles that are using the app.
struct ExtensionAppShimHandler::AppState {
  AppState(const std::string& app_id,
           std::unique_ptr<AppShimHost> multi_profile_host)
      : app_id(app_id), multi_profile_host(std::move(multi_profile_host)) {}
  ~AppState() = default;

  bool IsMultiProfile() const;

  // Mark the last-active profiles in AppShimRegistry, so that they will re-open
  // when the app is started next.
  void SaveLastActiveProfiles() const;

  const std::string app_id;

  // Multi-profile apps share the same shim process across multiple profiles.
  const std::unique_ptr<AppShimHost> multi_profile_host;

  // The profile state for the profiles currently running this app.
  std::map<Profile*, std::unique_ptr<ProfileState>> profiles;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppState);
};

ExtensionAppShimHandler::ProfileState::ProfileState(
    ExtensionAppShimHandler::AppState* in_app_state,
    std::unique_ptr<AppShimHost> in_single_profile_host)
    : app_state(in_app_state),
      single_profile_host(std::move(in_single_profile_host)) {
  // Assert that the ProfileState and AppState agree about whether or not this
  // is a multi-profile shim.
  DCHECK_NE(!!single_profile_host, !!app_state->multi_profile_host);
}

AppShimHost* ExtensionAppShimHandler::ProfileState::GetHost() const {
  if (app_state->multi_profile_host)
    return app_state->multi_profile_host.get();
  return single_profile_host.get();
}

bool ExtensionAppShimHandler::AppState::IsMultiProfile() const {
  return multi_profile_host.get();
}

void ExtensionAppShimHandler::AppState::SaveLastActiveProfiles() const {
  if (!IsMultiProfile())
    return;
  std::set<base::FilePath> last_active_profile_paths;
  for (auto iter_profile = profiles.begin(); iter_profile != profiles.end();
       ++iter_profile) {
    last_active_profile_paths.insert(iter_profile->first->GetPath());
  }
  AppShimRegistry::Get()->OnAppQuit(app_id, last_active_profile_paths);
}

Profile* ExtensionAppShimHandler::Delegate::ProfileForPath(
    const base::FilePath& full_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(full_path);

  // Use IsValidProfile to check if the profile has been created.
  return profile && profile_manager->IsValidProfile(profile) ? profile : NULL;
}

void ExtensionAppShimHandler::Delegate::LoadProfileAsync(
    const base::FilePath& full_path,
    base::OnceCallback<void(Profile*)> callback) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->LoadProfileByPath(full_path, false, std::move(callback));
}

bool ExtensionAppShimHandler::Delegate::IsProfileLockedForPath(
    const base::FilePath& full_path) {
  return profiles::IsProfileLocked(full_path);
}

AppWindowList ExtensionAppShimHandler::Delegate::GetWindows(
    Profile* profile,
    const std::string& extension_id) {
  return AppWindowRegistry::Get(profile)->GetAppWindowsForApp(extension_id);
}

const Extension* ExtensionAppShimHandler::Delegate::MaybeGetAppExtension(
    content::BrowserContext* context,
    const std::string& extension_id) {
  return ExtensionAppShimHandler::MaybeGetAppExtension(context, extension_id);
}

bool ExtensionAppShimHandler::Delegate::AllowShimToConnect(
    Profile* profile,
    const extensions::Extension* extension) {
  if (!profile || !extension)
    return false;
  if (extension->is_hosted_app() &&
      extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile),
                                extension) == extensions::LAUNCH_TYPE_REGULAR) {
    return false;
  }
  // Note that this will return true for non-hosted apps (e.g, Chrome Remote
  // Desktop).
  return true;
}

std::unique_ptr<AppShimHost> ExtensionAppShimHandler::Delegate::CreateHost(
    AppShimHost::Client* client,
    const base::FilePath& profile_path,
    const std::string& app_id,
    bool use_remote_cocoa) {
  return std::make_unique<AppShimHost>(client, app_id, profile_path,
                                       use_remote_cocoa);
}

void ExtensionAppShimHandler::Delegate::EnableExtension(
    Profile* profile,
    const std::string& extension_id,
    base::OnceCallback<void()> callback) {
  (new EnableViaPrompt(profile, extension_id, std::move(callback)))->Run();
}

void ExtensionAppShimHandler::Delegate::LaunchApp(
    Profile* profile,
    const Extension* extension,
    const std::vector<base::FilePath>& files) {
  extensions::RecordAppLaunchType(
      extension_misc::APP_LAUNCH_CMD_LINE_APP, extension->GetType());
  if (extension->is_hosted_app()) {
    apps::LaunchService::Get(profile)->OpenApplication(
        CreateAppLaunchParamsUserContainer(
            profile, extension, WindowOpenDisposition::NEW_FOREGROUND_TAB,
            apps::mojom::AppLaunchSource::kSourceCommandLine));
    return;
  }
  if (files.empty()) {
    apps::LaunchPlatformApp(profile, extension,
                            extensions::AppLaunchSource::kSourceCommandLine);
  } else {
    for (std::vector<base::FilePath>::const_iterator it = files.begin();
         it != files.end(); ++it) {
      apps::LaunchPlatformAppWithPath(profile, extension, *it);
    }
  }
}

void ExtensionAppShimHandler::Delegate::OpenAppURLInBrowserWindow(
    const base::FilePath& profile_path,
    const GURL& url) {
  Profile* profile =
      profile_path.empty() ? nullptr : ProfileForPath(profile_path);
  if (!profile)
    profile = g_browser_process->profile_manager()->GetLastUsedProfile();
  if (!profile)
    return;
  Browser* browser =
      new Browser(Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
  browser->window()->Show();
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.tabstrip_add_types = TabStripModel::ADD_ACTIVE;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void ExtensionAppShimHandler::Delegate::LaunchShim(
    Profile* profile,
    const Extension* extension,
    bool recreate_shims,
    apps::ShimLaunchedCallback launched_callback,
    apps::ShimTerminatedCallback terminated_callback) {
  // Only force recreation of shims when RemoteViews is in use (that is, for
  // PWAs). Otherwise, shims may be created unexpectedly.
  // https://crbug.com/941160
  if (recreate_shims && UsesRemoteViews(extension)) {
    // Load the resources needed to build the app shim (icons, etc), and then
    // recreate the shim and launch it.
    web_app::GetShortcutInfoForApp(
        extension, profile,
        base::BindOnce(
            &web_app::LaunchShim,
            web_app::LaunchShimUpdateBehavior::RECREATE_UNCONDITIONALLY,
            std::move(launched_callback), std::move(terminated_callback)));
  } else {
    web_app::LaunchShim(
        web_app::LaunchShimUpdateBehavior::DO_NOT_RECREATE,
        std::move(launched_callback), std::move(terminated_callback),
        web_app::ShortcutInfoForExtensionAndProfile(extension, profile));
  }
}

void ExtensionAppShimHandler::Delegate::LaunchUserManager() {
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
}

void ExtensionAppShimHandler::Delegate::MaybeTerminate() {
  apps::AppShimTerminationManager::Get()->MaybeTerminate();
}

ExtensionAppShimHandler::ExtensionAppShimHandler()
    : delegate_(new Delegate),
      weak_factory_(this) {
  // This is instantiated in BrowserProcessImpl::PreMainMessageLoopRun with
  // AppShimListener. Since PROFILE_CREATED is not fired until
  // ProfileManager::GetLastUsedProfile/GetLastOpenedProfiles, this should catch
  // notifications for all profiles.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  BrowserList::AddObserver(this);
}

ExtensionAppShimHandler::~ExtensionAppShimHandler() {
  BrowserList::RemoveObserver(this);
}

AppShimHost* ExtensionAppShimHandler::FindHost(Profile* profile,
                                               const std::string& app_id) {
  auto found_app = apps_.find(app_id);
  if (found_app == apps_.end())
    return nullptr;
  AppState* app_state = found_app->second.get();
  auto found_profile = app_state->profiles.find(profile);
  if (found_profile == app_state->profiles.end())
    return nullptr;
  ProfileState* profile_state = found_profile->second.get();
  return profile_state->GetHost();
}

AppShimHost* ExtensionAppShimHandler::GetHostForBrowser(Browser* browser) {
  Profile* profile = Profile::FromBrowserContext(browser->profile());
  const Extension* extension =
      apps::ExtensionAppShimHandler::MaybeGetAppForBrowser(browser);
  if (!extension || !extension->is_hosted_app())
    return nullptr;
  ProfileState* profile_state = GetOrCreateProfileState(profile, extension);
  if (!profile_state)
    return nullptr;
  return profile_state->GetHost();
}

// static
const Extension* ExtensionAppShimHandler::MaybeGetAppExtension(
    content::BrowserContext* context,
    const std::string& extension_id) {
  if (!context)
    return NULL;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
  return extension &&
                 (extension->is_platform_app() || extension->is_hosted_app())
             ? extension
             : NULL;
}

// static
const Extension* ExtensionAppShimHandler::MaybeGetAppForBrowser(
    Browser* browser) {
  if (!browser || !browser->deprecated_is_app())
    return NULL;

  return MaybeGetAppExtension(
      browser->profile(),
      web_app::GetAppIdFromApplicationName(browser->app_name()));
}

void ExtensionAppShimHandler::RequestUserAttentionForWindow(
    AppWindow* app_window,
    chrome::mojom::AppShimAttentionType attention_type) {
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  AppShimHost* host = FindHost(profile, app_window->extension_id());
  if (host && !host->UsesRemoteViews())
    host->GetAppShim()->SetUserAttention(attention_type);
}

void ExtensionAppShimHandler::OnShimLaunchRequested(
    AppShimHost* host,
    bool recreate_shims,
    apps::ShimLaunchedCallback launched_callback,
    apps::ShimTerminatedCallback terminated_callback) {
  // A shim can only be launched through an active profile, so find a profile
  // through which to do the launch. For multi-profile apps, select one
  // arbitrarily. For non-multi-profile apps, select the specified profile.
  Profile* profile = nullptr;
  {
    auto found_app = apps_.find(host->GetAppId());
    DCHECK(found_app != apps_.end());
    AppState* app_state = found_app->second.get();
    if (app_state->IsMultiProfile()) {
      DCHECK(!app_state->profiles.empty());
      profile = app_state->profiles.begin()->first;
    } else {
      profile = delegate_->ProfileForPath(host->GetProfilePath());
    }
  }

  const Extension* extension =
      delegate_->MaybeGetAppExtension(profile, host->GetAppId());
  if (!profile || !extension) {
    // If the profile or extension has been unloaded, indicate that the launch
    // failed. This will close the AppShimHost eventually, if appropriate.
    std::move(launched_callback).Run(base::Process());
    return;
  }
  delegate_->LaunchShim(profile, extension, recreate_shims,
                        std::move(launched_callback),
                        std::move(terminated_callback));
}

void ExtensionAppShimHandler::OnShimProcessConnected(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  DCHECK(crx_file::id_util::IdIsValid(bootstrap->GetAppId()));
  switch (bootstrap->GetLaunchType()) {
    case chrome::mojom::AppShimLaunchType::kNormal:
      OnShimProcessConnectedForLaunch(std::move(bootstrap));
      break;
    case chrome::mojom::AppShimLaunchType::kRegisterOnly:
      OnShimProcessConnectedForRegisterOnly(std::move(bootstrap));
      break;
  }
}

void ExtensionAppShimHandler::OnShimProcessConnectedForRegisterOnly(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  const std::string& app_id = bootstrap->GetAppId();
  DCHECK_EQ(bootstrap->GetLaunchType(),
            chrome::mojom::AppShimLaunchType::kRegisterOnly);

  // Create a ProfileState the specified profile (if there is one). We should
  // not do this (if there exists no ProfileState, then the shim should just
  // exit), but many tests assume this behavior, and need to be updated.
  Profile* profile = delegate_->ProfileForPath(bootstrap->GetProfilePath());
  const Extension* extension = delegate_->MaybeGetAppExtension(profile, app_id);
  if (profile && extension && delegate_->AllowShimToConnect(profile, extension))
    GetOrCreateProfileState(profile, extension);

  // Because this was a register-only launch, it must have been launched by
  // Chrome, and so there should probably still exist the ProfileState through
  // which the launch was originally done.
  ProfileState* profile_state = nullptr;
  auto found_app = apps_.find(app_id);
  if (found_app != apps_.end()) {
    AppState* app_state = found_app->second.get();
    if (app_state->IsMultiProfile()) {
      DCHECK(!app_state->profiles.empty());
      profile_state = app_state->profiles.begin()->second.get();
    } else {
      auto found_profile = app_state->profiles.find(profile);
      if (found_profile != app_state->profiles.end()) {
        profile_state = found_profile->second.get();
      }
    }
  }

  OnShimProcessConnectedAndAllLaunchesDone(
      profile_state,
      profile_state ? chrome::mojom::AppShimLaunchResult::kSuccess
                    : chrome::mojom::AppShimLaunchResult::kNoHost,
      std::move(bootstrap));
}

void ExtensionAppShimHandler::OnShimProcessConnectedForLaunch(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  const std::string& app_id = bootstrap->GetAppId();
  DCHECK_EQ(bootstrap->GetLaunchType(),
            chrome::mojom::AppShimLaunchType::kNormal);

  // Retrieve the list of last-active profiles. If there are no last-active
  // profiles (which is rare -- e.g, when the last-active profiles were
  // removed), then use all profiles for which the app is installed.
  std::set<base::FilePath> last_active_profile_paths =
      AppShimRegistry::Get()->GetLastActiveProfilesForApp(app_id);
  if (last_active_profile_paths.empty()) {
    last_active_profile_paths =
        AppShimRegistry::Get()->GetInstalledProfilesForApp(app_id);
  }

  // Construct |profile_paths_to_launch| to be the list of all profiles to
  // attempt to launch, starting with the profile specified in |bootstrap|,
  // at the front of the list.
  std::vector<base::FilePath> profile_paths_to_launch = {
      bootstrap->GetProfilePath()};
  for (const auto& profile_path : last_active_profile_paths)
    profile_paths_to_launch.push_back(profile_path);

  // Attempt load all of the profiles in |profile_paths_to_launch|, and once
  // they're loaded (or have failed to load), call
  // OnShimProcessConnectedAndProfilesToLaunchLoaded.
  base::OnceClosure callback = base::BindOnce(
      &ExtensionAppShimHandler::OnShimProcessConnectedAndProfilesToLaunchLoaded,
      weak_factory_.GetWeakPtr(), std::move(bootstrap),
      profile_paths_to_launch);
  {
    // This will update |callback| to be a chain of callbacks that load the
    // profiles in |profile_paths_to_load|, one by one, using
    // LoadProfileAndApp, and then finally call the initial |callback|. This
    // may end up being async (if some profiles aren't loaded), or may be
    // synchronous (if all profiles happen to already be loaded).
    for (const auto& profile_path : profile_paths_to_launch) {
      if (profile_path.empty())
        continue;
      LoadProfileAppCallback callback_wrapped = base::BindOnce(
          [](base::OnceClosure callback_to_wrap, Profile*,
             const extensions::Extension*) {
            std::move(callback_to_wrap).Run();
          },
          std::move(callback));
      callback = base::BindOnce(&ExtensionAppShimHandler::LoadProfileAndApp,
                                weak_factory_.GetWeakPtr(), profile_path,
                                app_id, std::move(callback_wrapped));
    }
  }
  std::move(callback).Run();
}

void ExtensionAppShimHandler::OnShimProcessConnectedAndProfilesToLaunchLoaded(
    std::unique_ptr<AppShimHostBootstrap> bootstrap,
    const std::vector<base::FilePath>& profile_paths_to_launch) {
  // The the profile specified in |bootstrap| (even if it's empty) should be the
  // first profile listed in |profile_paths_to_launch|.
  DCHECK_EQ(profile_paths_to_launch[0], bootstrap->GetProfilePath());

  const auto& app_id = bootstrap->GetAppId();
  auto launch_files = bootstrap->GetLaunchFiles();

  // Launch all of the profiles in |profile_paths_to_launch|. Record the most
  // profile successfully launched in |launched_profile_state|, and the most
  // recent reason for a failure (if any) in |launch_result|.
  ProfileState* launched_profile_state = nullptr;
  auto launch_result = chrome::mojom::AppShimLaunchResult::kProfileNotFound;
  for (size_t iter = 0; iter < profile_paths_to_launch.size(); ++iter) {
    const base::FilePath& profile_path = profile_paths_to_launch[iter];
    if (profile_path.empty())
      continue;
    if (delegate_->IsProfileLockedForPath(profile_path)) {
      launch_result = chrome::mojom::AppShimLaunchResult::kProfileLocked;
      continue;
    }
    Profile* profile = delegate_->ProfileForPath(profile_path);
    if (!profile) {
      launch_result = chrome::mojom::AppShimLaunchResult::kProfileNotFound;
      continue;
    }
    const Extension* extension =
        delegate_->MaybeGetAppExtension(profile, app_id);
    if (!extension) {
      launch_result = chrome::mojom::AppShimLaunchResult::kAppNotFound;
      continue;
    }

    // Create a ProfileState for this app, if appropriate (e.g, not for
    // open-in-a-tab bookmark apps).
    ProfileState* profile_state = nullptr;
    if (delegate_->AllowShimToConnect(profile, extension))
      profile_state = GetOrCreateProfileState(profile, extension);

    // If there exist any open window for this profile, then bring them to the
    // front.
    bool had_open_windows = false;
    if (profile_state && !profile_state->browsers.empty()) {
      for (auto* browser : profile_state->browsers) {
        if (auto* window = browser->window()) {
          window->Show();
          had_open_windows = true;
        }
      }
    }

    // Launch the app (open a window for it) if there were no open windows for
    // it already, or if we were asked to open files.
    if (!had_open_windows || !launch_files.empty()) {
      delegate_->LaunchApp(profile, extension, launch_files);
      launch_files.clear();
    }

    // If we successfully created a profile state, save it for |bootstrap| to
    // connect to once all launches are done.
    if (profile_state)
      launched_profile_state = profile_state;
    else
      launch_result = chrome::mojom::AppShimLaunchResult::kNoHost;

    // If this was the first profile in |profile_paths_to_launch|, then this
    // was the profile specified in the bootstrap, so stop here.
    if (iter == 0)
      break;
  }

  if (launched_profile_state) {
    // If we launched any profile, report success.
    launch_result = chrome::mojom::AppShimLaunchResult::kSuccess;
  } else {
    // Otherwise, if the app specified a URL, open that URL in a new window.
    const GURL& url = bootstrap->GetAppURL();
    if (url.is_valid())
      delegate_->OpenAppURLInBrowserWindow(bootstrap->GetProfilePath(), url);
  }

  OnShimProcessConnectedAndAllLaunchesDone(launched_profile_state,
                                           launch_result, std::move(bootstrap));
}

void ExtensionAppShimHandler::OnShimProcessConnectedAndAllLaunchesDone(
    ProfileState* profile_state,
    chrome::mojom::AppShimLaunchResult result,
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  // If we failed because the profile was locked, launch the profile manager.
  if (result == chrome::mojom::AppShimLaunchResult::kProfileLocked)
    delegate_->LaunchUserManager();

  // If we failed to find a AppShimHost (in a ProfileState) for |bootstrap|
  // to attempt to connect to, then quit the shim. This may not represent an
  // actual failure (e.g, for open-in-a-tab bookmarks).
  if (result != chrome::mojom::AppShimLaunchResult::kSuccess) {
    DCHECK(!profile_state);
    bootstrap->OnFailedToConnectToHost(result);
    return;
  }
  DCHECK(profile_state);
  AppShimHost* host = profile_state->GetHost();
  DCHECK(host);

  // If we already have a host attached (e.g, due to multiple launches racing),
  // close down the app shim that didn't win the race.
  if (host->HasBootstrapConnected()) {
    // If another app shim process has already connected to this (profile,
    // app_id) pair, then focus the windows for the existing process. Note
    // that this only does anything for non-RemoveCocoa apps.
    OnShimFocus(
        profile_state->GetHost(),
        bootstrap->GetLaunchType() == chrome::mojom::AppShimLaunchType::kNormal
            ? chrome::mojom::AppShimFocusType::kReopen
            : chrome::mojom::AppShimFocusType::kNormal,
        bootstrap->GetLaunchFiles());

    bootstrap->OnFailedToConnectToHost(
        chrome::mojom::AppShimLaunchResult::kDuplicateHost);
    return;
  }

  // If the connecting shim process doesn't have an acceptable code
  // signature, reject the connection and re-launch the shim. The internal
  // re-launch will likely fail, whereupon the shim will be recreated.
  if (!IsAcceptablyCodeSigned(bootstrap->GetAppShimPid())) {
    LOG(ERROR) << "The attaching app shim's code signature is invalid.";
    bootstrap->OnFailedToConnectToHost(
        chrome::mojom::AppShimLaunchResult::kFailedValidation);
    host->LaunchShim();
    return;
  }

  host->OnBootstrapConnected(std::move(bootstrap));
}

// static
ExtensionAppShimHandler* ExtensionAppShimHandler::Get() {
  // This will only return nullptr in certain unit tests that do not initialize
  // the app shim host manager.
  auto* shim_host_manager =
      g_browser_process->platform_part()->app_shim_listener();
  if (shim_host_manager)
    return shim_host_manager->extension_app_shim_handler();
  return nullptr;
}

void ExtensionAppShimHandler::CloseShimsForProfile(Profile* profile) {
  for (auto iter_app = apps_.begin(); iter_app != apps_.end();) {
    AppState* app_state = iter_app->second.get();
    app_state->profiles.erase(profile);
    if (app_state->profiles.empty())
      iter_app = apps_.erase(iter_app);
    else
      ++iter_app;
  }
}

void ExtensionAppShimHandler::CloseShimForApp(Profile* profile,
                                              const std::string& app_id) {
  auto found_app = apps_.find(app_id);
  if (found_app == apps_.end())
    return;
  AppState* app_state = found_app->second.get();
  auto found_profile = app_state->profiles.find(profile);
  if (found_profile == app_state->profiles.end())
    return;
  if (app_state->profiles.size() == 1) {
    app_state->SaveLastActiveProfiles();
    apps_.erase(found_app);
  } else {
    app_state->profiles.erase(found_profile);
  }
}

void ExtensionAppShimHandler::LoadProfileAndApp(
    const base::FilePath& profile_path,
    const std::string& app_id,
    LoadProfileAppCallback callback) {
  Profile* profile = delegate_->ProfileForPath(profile_path);
  if (profile) {
    OnProfileLoaded(profile_path, app_id, std::move(callback), profile);
  } else {
    delegate_->LoadProfileAsync(
        profile_path, base::BindOnce(&ExtensionAppShimHandler::OnProfileLoaded,
                                     weak_factory_.GetWeakPtr(), profile_path,
                                     app_id, std::move(callback)));
  }
}

void ExtensionAppShimHandler::OnProfileLoaded(
    const base::FilePath& profile_path,
    const std::string& app_id,
    LoadProfileAppCallback callback,
    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile) {
    // User may have deleted the profile this shim was originally created for.
    // TODO(jackhou): Add some UI for this case and remove the LOG.
    LOG(ERROR) << "Requested directory is not a known profile '"
               << profile_path.value() << "'.";
    std::move(callback).Run(profile, nullptr);
    return;
  }

  // TODO(jeremya): Handle the case that launching the app fails. Probably we
  // need to watch for 'app successfully launched' or at least 'background page
  // exists/was created' and time out with failure if we don't see that sign of
  // life within a certain window.
  const Extension* extension = delegate_->MaybeGetAppExtension(profile, app_id);
  if (extension) {
    std::move(callback).Run(profile, extension);
  } else {
    delegate_->EnableExtension(
        profile, app_id,
        base::BindOnce(&ExtensionAppShimHandler::OnAppEnabled,
                       weak_factory_.GetWeakPtr(), profile_path, app_id,
                       std::move(callback)));
  }
}

void ExtensionAppShimHandler::OnAppEnabled(const base::FilePath& profile_path,
                                           const std::string& app_id,
                                           LoadProfileAppCallback callback) {
  // If the profile doesn't exist, it may have been deleted during the enable
  // prompt.
  Profile* profile = delegate_->ProfileForPath(profile_path);
  const Extension* extension =
      profile ? delegate_->MaybeGetAppExtension(profile, app_id) : nullptr;
  std::move(callback).Run(profile, extension);
}

bool ExtensionAppShimHandler::IsAcceptablyCodeSigned(pid_t pid) const {
  return IsAcceptablyCodeSignedInternal(pid);
}

void ExtensionAppShimHandler::OnShimProcessDisconnected(AppShimHost* host) {
  const std::string app_id = host->GetAppId();

  auto found_app = apps_.find(app_id);
  DCHECK(found_app != apps_.end());
  AppState* app_state = found_app->second.get();
  DCHECK(app_state);

  // For multi-profile apps, just delete the AppState, which will take down
  // |host| and all profiles' state.
  if (app_state->IsMultiProfile()) {
    app_state->SaveLastActiveProfiles();
    DCHECK_EQ(host, app_state->multi_profile_host.get());
    apps_.erase(found_app);
    return;
  }

  // For non-RemoteCocoa apps, close all of the windows only if the the shim
  // process has successfully connected (if it never connected, then let the
  // app run as normal).
  bool close_windows =
      !host->UsesRemoteViews() && host->HasBootstrapConnected();

  // Erase the ProfileState, which will delete |host|.
  Profile* profile = delegate_->ProfileForPath(host->GetProfilePath());
  auto found_profile = app_state->profiles.find(profile);
  DCHECK(found_profile != app_state->profiles.end());
  ProfileState* profile_state = found_profile->second.get();
  DCHECK_EQ(host, profile_state->single_profile_host.get());
  app_state->profiles.erase(found_profile);
  host = nullptr;

  // Erase |app_state| if this was the last profile.
  if (app_state->profiles.empty())
    apps_.erase(found_app);

  // Close app windows if we decided to do so above.
  if (close_windows) {
    AppWindowList windows = delegate_->GetWindows(profile, app_id);
    for (auto it = windows.begin(); it != windows.end(); ++it) {
      if (*it)
        (*it)->GetBaseWindow()->Close();
    }
  }
}

void ExtensionAppShimHandler::OnShimFocus(
    AppShimHost* host,
    chrome::mojom::AppShimFocusType focus_type,
    const std::vector<base::FilePath>& files) {
  // This path is only for legacy apps (which are perforce single-profile).
  if (host->UsesRemoteViews())
    return;

  Profile* profile = delegate_->ProfileForPath(host->GetProfilePath());
  const Extension* extension =
      delegate_->MaybeGetAppExtension(profile, host->GetAppId());
  if (!extension) {
    CloseShimForApp(profile, host->GetAppId());
    return;
  }

  AppWindowList windows = delegate_->GetWindows(profile, host->GetAppId());
  for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
    if (*it)
      (*it)->GetBaseWindow()->Show();
  }

  if (focus_type == chrome::mojom::AppShimFocusType::kNormal ||
      (focus_type == chrome::mojom::AppShimFocusType::kReopen &&
       !windows.empty())) {
    return;
  }
  delegate_->LaunchApp(profile, extension, files);
}

void ExtensionAppShimHandler::OnShimSelectedProfile(
    AppShimHost* host,
    const base::FilePath& profile_path) {
  LoadProfileAndApp(
      profile_path, host->GetAppId(),
      base::BindOnce(
          &ExtensionAppShimHandler::OnShimSelectedProfileAndAppLoaded,
          weak_factory_.GetWeakPtr()));
}

void ExtensionAppShimHandler::OnShimSelectedProfileAndAppLoaded(
    Profile* profile,
    const extensions::Extension* extension) {
  if (!extension)
    return;

  auto found_app = apps_.find(extension->id());
  if (found_app == apps_.end())
    return;
  AppState* app_state = found_app->second.get();
  auto found_profile = app_state->profiles.find(profile);
  if (found_profile != app_state->profiles.end()) {
    // If this profile is currently open for the app, focus its windows.
    ProfileState* profile_state = found_profile->second.get();
    for (auto* browser : profile_state->browsers) {
      if (auto* window = browser->window())
        window->Show();
    }
  } else {
    // Otherwise, launch the app for this profile (which will open a new
    // window).
    delegate_->LaunchApp(profile, extension, std::vector<base::FilePath>());
  }
}

void ExtensionAppShimHandler::set_delegate(Delegate* delegate) {
  delegate_.reset(delegate);
}

void ExtensionAppShimHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile->IsOffTheRecord())
        return;

      AppLifetimeMonitorFactory::GetForBrowserContext(profile)->AddObserver(
          this);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile->IsOffTheRecord())
        return;

      AppLifetimeMonitorFactory::GetForBrowserContext(profile)->RemoveObserver(
          this);
      CloseShimsForProfile(profile);
      break;
    }
    default: {
      NOTREACHED();  // Unexpected notification.
      break;
    }
  }
}

void ExtensionAppShimHandler::OnAppStart(content::BrowserContext* context,
                                         const std::string& app_id) {}

void ExtensionAppShimHandler::OnAppActivated(content::BrowserContext* context,
                                             const std::string& app_id) {
  Profile* profile = Profile::FromBrowserContext(context);
  const Extension* extension = delegate_->MaybeGetAppExtension(context, app_id);
  if (!extension)
    return;
  if (auto* profile_state = GetOrCreateProfileState(profile, extension))
    profile_state->GetHost()->LaunchShim();
}

void ExtensionAppShimHandler::OnAppDeactivated(content::BrowserContext* context,
                                               const std::string& app_id) {
  CloseShimForApp(static_cast<Profile*>(context), app_id);
  if (apps_.empty())
    delegate_->MaybeTerminate();
}

void ExtensionAppShimHandler::OnAppStop(content::BrowserContext* context,
                                        const std::string& app_id) {}

void ExtensionAppShimHandler::OnBrowserAdded(Browser* browser) {
  // Don't keep track of browsers that are not associated with an app.
  const Extension* extension = MaybeGetAppForBrowser(browser);
  if (!extension)
    return;
  if (auto* profile_state =
          GetOrCreateProfileState(browser->profile(), extension)) {
    profile_state->browsers.insert(browser);
    if (profile_state->browsers.size() == 1)
      OnAppActivated(browser->profile(), extension->id());
  }
}

void ExtensionAppShimHandler::OnBrowserRemoved(Browser* browser) {
  // Note that |browser| may no longer have an extension, if it was unloaded
  // before |browser| was closed. Search for |browser| in all |apps_|.
  for (auto iter_app = apps_.begin(); iter_app != apps_.end(); ++iter_app) {
    AppState* app_state = iter_app->second.get();
    const std::string& app_id = iter_app->first;
    for (auto iter_profile = app_state->profiles.begin();
         iter_profile != app_state->profiles.end(); ++iter_profile) {
      ProfileState* profile_state = iter_profile->second.get();
      auto found = profile_state->browsers.find(browser);
      if (found != profile_state->browsers.end()) {
        // If we have no browser windows open after erasing this window, then
        // close the ProfileState (and potentially the shim as well).
        profile_state->browsers.erase(found);
        if (profile_state->browsers.empty())
          OnAppDeactivated(browser->profile(), app_id);
        return;
      }
    }
  }
}

void ExtensionAppShimHandler::OnBrowserSetLastActive(Browser* browser) {
  // Rebuild the profile menu items (to ensure that the checkmark in the menu
  // is next to the new-active item).
  if (avatar_menu_)
    avatar_menu_->ActiveBrowserChanged(browser);
  UpdateAllProfileMenus();
}

void ExtensionAppShimHandler::UpdateAllProfileMenus() {
  RebuildProfileMenuItemsFromAvatarMenu();
  for (auto& iter_app : apps_) {
    AppState* app_state = iter_app.second.get();
    if (app_state->IsMultiProfile())
      UpdateAppProfileMenu(app_state);
  }
}

void ExtensionAppShimHandler::RebuildProfileMenuItemsFromAvatarMenu() {
  if (!avatar_menu_) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    avatar_menu_ = std::make_unique<AvatarMenu>(
        &profile_manager->GetProfileAttributesStorage(), this, nullptr);
  }
  avatar_menu_->RebuildMenu();
  profile_menu_items_.clear();
  for (size_t i = 0; i < avatar_menu_->GetNumberOfItems(); ++i) {
    auto mojo_item = chrome::mojom::ProfileMenuItem::New();
    const AvatarMenu::Item& item = avatar_menu_->GetItemAt(i);
    mojo_item->name = item.name;
    mojo_item->menu_index = item.menu_index;
    mojo_item->active = item.active;
    mojo_item->profile_path = item.profile_path;
    mojo_item->icon =
        profiles::GetAvatarIconForNSMenu(item.profile_path).ToImageSkia()[0];
    profile_menu_items_.push_back(std::move(mojo_item));
  }
}

void ExtensionAppShimHandler::OnAvatarMenuChanged(AvatarMenu* menu) {
  // Rebuild the profile menu to reflect changes (e.g, added or removed
  // profiles).
  DCHECK_EQ(avatar_menu_.get(), menu);
  UpdateAllProfileMenus();
}

void ExtensionAppShimHandler::UpdateAppProfileMenu(AppState* app_state) {
  DCHECK(app_state->IsMultiProfile());
  // Include in |items| the profiles from |profile_menu_items_| for which this
  // app is installed, sorted by |menu_index|.
  std::vector<chrome::mojom::ProfileMenuItemPtr> items;
  auto installed_profiles =
      AppShimRegistry::Get()->GetInstalledProfilesForApp(app_state->app_id);
  for (const auto& item : profile_menu_items_) {
    if (installed_profiles.count(item->profile_path))
      items.push_back(item->Clone());
  }
  std::sort(items.begin(), items.end(), ProfileMenuItemComparator);

  // Do not show a profile menu unless it has at least 2 entries (that is, the
  // app is available for at least 2 profiles).
  if (items.size() < 2)
    items.clear();

  // Send the profile menu to the app shim process.
  app_state->multi_profile_host->GetAppShim()->UpdateProfileMenu(
      std::move(items));
}

ExtensionAppShimHandler::ProfileState*
ExtensionAppShimHandler::GetOrCreateProfileState(
    Profile* profile,
    const extensions::Extension* extension) {
  if (web_app::AppShimLaunchDisabled())
    return nullptr;

  const bool is_multi_profile =
      base::FeatureList::IsEnabled(features::kAppShimMultiProfile) &&
      extension->from_bookmark();
  const base::FilePath profile_path =
      is_multi_profile ? base::FilePath() : profile->GetPath();
  const std::string app_id = extension->id();
  const bool use_remote_cocoa = UsesRemoteViews(extension);

  auto found_app = apps_.find(extension->id());
  if (found_app == apps_.end()) {
    std::unique_ptr<AppShimHost> multi_profile_host;
    if (is_multi_profile) {
      multi_profile_host =
          delegate_->CreateHost(this, profile_path, app_id, use_remote_cocoa);
    }
    auto new_app_state =
        std::make_unique<AppState>(app_id, std::move(multi_profile_host));
    found_app =
        apps_.insert(std::make_pair(app_id, std::move(new_app_state))).first;
  }
  AppState* app_state = found_app->second.get();

  // Initialize the profile menu.
  if (is_multi_profile)
    UpdateAppProfileMenu(app_state);

  auto found_profile = app_state->profiles.find(profile);
  if (found_profile == app_state->profiles.end()) {
    std::unique_ptr<AppShimHost> single_profile_host;
    if (!is_multi_profile) {
      single_profile_host =
          delegate_->CreateHost(this, profile_path, app_id, use_remote_cocoa);
    }
    auto new_profile_state = std::make_unique<ProfileState>(
        app_state, std::move(single_profile_host));
    found_profile =
        app_state->profiles
            .insert(std::make_pair(profile, std::move(new_profile_state)))
            .first;
  }
  return found_profile->second.get();
}

}  // namespace apps
