// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"

#include <Security/Security.h>

#include <utility>

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/launcher.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_manager_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/mac/app_shim_param_traits.h"
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
#include "ui/base/ui_base_features.h"

using extensions::AppWindow;
using extensions::AppWindowRegistry;
using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

typedef AppWindowRegistry::AppWindowList AppWindowList;

void SetAppHidden(Profile* profile, const std::string& app_id, bool hidden) {
  AppWindowList windows =
      AppWindowRegistry::Get(profile)->GetAppWindowsForApp(app_id);
  for (AppWindowList::const_reverse_iterator it = windows.rbegin();
       it != windows.rend();
       ++it) {
    if (hidden)
      (*it)->GetBaseWindow()->HideWithApp();
    else
      (*it)->GetBaseWindow()->ShowWithApp();
  }
}

bool FocusWindows(const AppWindowList& windows) {
  if (windows.empty())
    return false;

  std::set<gfx::NativeWindow> native_windows;
  for (AppWindowList::const_iterator it = windows.begin(); it != windows.end();
       ++it) {
    native_windows.insert((*it)->GetNativeWindow());
  }
  // Allow workspace switching. For the browser process, we can reasonably rely
  // on OS X to switch spaces for us and honor relevant user settings. But shims
  // don't have windows, so we have to do it ourselves.
  ui::FocusWindowSet(native_windows);
  return true;
}

bool FocusHostedAppWindows(const std::set<Browser*>& browsers) {
  if (browsers.empty())
    return false;

  std::set<gfx::NativeWindow> native_windows;
  for (const Browser* browser : browsers)
    native_windows.insert(browser->window()->GetNativeWindow());

  ui::FocusWindowSet(native_windows);
  return true;
}

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

}  // namespace

namespace apps {

base::FilePath ExtensionAppShimHandler::Delegate::GetFullProfilePath(
    const base::FilePath& relative_profile_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->user_data_dir().Append(relative_profile_path);
}

bool ExtensionAppShimHandler::Delegate::ProfileExistsForPath(
    const base::FilePath& full_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Check for the profile name in the profile info cache to ensure that we
  // never access any directory that isn't a known profile.
  ProfileAttributesEntry* entry;
  return profile_manager->GetProfileAttributesStorage().
      GetProfileAttributesWithPath(full_path, &entry);
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

AppShimHost* ExtensionAppShimHandler::Delegate::CreateHost(
    Profile* profile,
    const extensions::Extension* extension) {
  return new AppShimHost(extension->id(), profile->GetPath(),
                         UsesRemoteViews(extension));
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
    OpenApplication(CreateAppLaunchParamsUserContainer(
        profile, extension, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        extensions::AppLaunchSource::kSourceCommandLine));
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
  AppShimHandler::MaybeTerminate();
}

ExtensionAppShimHandler::ExtensionAppShimHandler()
    : delegate_(new Delegate),
      weak_factory_(this) {
  // This is instantiated in BrowserProcessImpl::PreMainMessageLoopRun with
  // AppShimHostManager. Since PROFILE_CREATED is not fired until
  // ProfileManager::GetLastUsedProfile/GetLastOpenedProfiles, this should catch
  // notifications for all profiles.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  BrowserList::AddObserver(this);
}

ExtensionAppShimHandler::~ExtensionAppShimHandler() {
  BrowserList::RemoveObserver(this);
}

AppShimHost* ExtensionAppShimHandler::FindHost(Profile* profile,
                                               const std::string& app_id) {
  HostMap::iterator it = hosts_.find(make_pair(profile, app_id));
  return it == hosts_.end() ? NULL : it->second;
}

AppShimHost* ExtensionAppShimHandler::FindOrCreateHost(
    Profile* profile,
    const extensions::Extension* extension) {
  if (web_app::AppShimLaunchDisabled())
    return nullptr;
  AppShimHost*& host = hosts_[make_pair(profile, extension->id())];
  if (!host)
    host = delegate_->CreateHost(profile, extension);
  return host;
}

AppShimHost* ExtensionAppShimHandler::GetHostForBrowser(Browser* browser) {
  if (!features::HostWindowsInAppShimProcess())
    return nullptr;
  Profile* profile = Profile::FromBrowserContext(browser->profile());
  const Extension* extension =
      apps::ExtensionAppShimHandler::MaybeGetAppForBrowser(browser);
  if (!extension || !extension->is_hosted_app())
    return nullptr;
  return FindOrCreateHost(profile, extension);
}

void ExtensionAppShimHandler::SetHostedAppHidden(Profile* profile,
                                                 const std::string& app_id,
                                                 bool hidden) {
  const AppBrowserMap::iterator it =
      app_browser_windows_.find(std::make_pair(profile, app_id));
  if (it == app_browser_windows_.end())
    return;

  for (const Browser* browser : it->second) {
    if (web_app::GetAppIdFromApplicationName(browser->app_name()) != app_id)
      continue;

    if (hidden)
      browser->window()->Hide();
    else
      browser->window()->Show();
  }
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
  if (!browser || !browser->is_app())
    return NULL;

  return MaybeGetAppExtension(
      browser->profile(),
      web_app::GetAppIdFromApplicationName(browser->app_name()));
}

void ExtensionAppShimHandler::QuitAppForWindow(AppWindow* app_window) {
  AppShimHost* host =
      FindHost(Profile::FromBrowserContext(app_window->browser_context()),
               app_window->extension_id());
  if (host) {
    OnShimQuit(host);
  } else {
    // App shims might be disabled or the shim is still starting up.
    AppWindowRegistry::Get(
        Profile::FromBrowserContext(app_window->browser_context()))
        ->CloseAllAppWindowsForApp(app_window->extension_id());
  }
}

void ExtensionAppShimHandler::QuitHostedAppForWindow(
    Profile* profile,
    const std::string& app_id) {
  AppShimHost* host = FindHost(Profile::FromBrowserContext(profile), app_id);
  if (host)
    OnShimQuit(host);
  else
    CloseBrowsersForApp(profile, app_id);
}

void ExtensionAppShimHandler::HideAppForWindow(AppWindow* app_window) {
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  AppShimHost* host = FindHost(profile, app_window->extension_id());
  if (host)
    host->OnAppHide();
  else
    SetAppHidden(profile, app_window->extension_id(), true);
}

void ExtensionAppShimHandler::HideHostedApp(Profile* profile,
                                            const std::string& app_id) {
  AppShimHost* host = FindHost(profile, app_id);
  if (host)
    host->OnAppHide();
  else
    SetHostedAppHidden(profile, app_id, true);
}

void ExtensionAppShimHandler::FocusAppForWindow(AppWindow* app_window) {
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  const std::string& app_id = app_window->extension_id();
  AppShimHost* host = FindHost(profile, app_id);
  if (host) {
    OnShimFocus(host, APP_SHIM_FOCUS_NORMAL, std::vector<base::FilePath>());
  } else {
    FocusWindows(AppWindowRegistry::Get(profile)->GetAppWindowsForApp(app_id));
  }
}

void ExtensionAppShimHandler::UnhideWithoutActivationForWindow(
    AppWindow* app_window) {
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  AppShimHost* host = FindHost(profile, app_window->extension_id());
  if (host)
    host->OnAppUnhideWithoutActivation();
}

void ExtensionAppShimHandler::RequestUserAttentionForWindow(
    AppWindow* app_window,
    AppShimAttentionType attention_type) {
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  AppShimHost* host = FindHost(profile, app_window->extension_id());
  if (host)
    host->OnAppRequestUserAttention(attention_type);
}

void ExtensionAppShimHandler::OnChromeWillHide() {
  // Send OnAppHide to all the shims so that they go into the hidden state.
  // This is necessary so that when the shim is next focused, it will know to
  // unhide.
  for (auto& entry : hosts_)
    entry.second->OnAppHide();
}

void ExtensionAppShimHandler::OnShimLaunchRequested(
    AppShimHost* host,
    bool recreate_shims,
    apps::ShimLaunchedCallback launched_callback,
    apps::ShimTerminatedCallback terminated_callback) {
  Profile* profile = nullptr;
  const Extension* extension = MaybeGetExtensionOrCloseHost(host, &profile);
  if (!profile || !extension)
    return;
  delegate_->LaunchShim(profile, extension, recreate_shims,
                        std::move(launched_callback),
                        std::move(terminated_callback));
}

void ExtensionAppShimHandler::OnShimProcessConnected(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  const std::string& app_id = bootstrap->GetAppId();
  DCHECK(crx_file::id_util::IdIsValid(app_id));

  const base::FilePath& relative_profile_path = bootstrap->GetProfilePath();
  DCHECK(!relative_profile_path.empty());
  base::FilePath profile_path =
      delegate_->GetFullProfilePath(relative_profile_path);

  if (!delegate_->ProfileExistsForPath(profile_path)) {
    // User may have deleted the profile this shim was originally created for.
    // TODO(jackhou): Add some UI for this case and remove the LOG.
    LOG(ERROR) << "Requested directory is not a known profile '"
               << profile_path.value() << "'.";
    bootstrap->OnFailedToConnectToHost(APP_SHIM_LAUNCH_PROFILE_NOT_FOUND);
    return;
  }

  if (delegate_->IsProfileLockedForPath(profile_path)) {
    LOG(WARNING) << "Requested profile is locked.  Showing User Manager.";
    bootstrap->OnFailedToConnectToHost(APP_SHIM_LAUNCH_PROFILE_LOCKED);
    delegate_->LaunchUserManager();
    return;
  }

  Profile* profile = delegate_->ProfileForPath(profile_path);
  if (profile) {
    OnProfileLoaded(std::move(bootstrap), profile);
  } else {
    // If the profile is not loaded, this must have been a launch by the shim.
    // Load the profile asynchronously, the host will be registered in
    // OnProfileLoaded.
    DCHECK_EQ(APP_SHIM_LAUNCH_NORMAL, bootstrap->GetLaunchType());
    delegate_->LoadProfileAsync(
        profile_path,
        base::BindOnce(&ExtensionAppShimHandler::OnProfileLoaded,
                       weak_factory_.GetWeakPtr(), std::move(bootstrap)));
  }
}

// static
ExtensionAppShimHandler* ExtensionAppShimHandler::Get() {
  // This will only return nullptr in certain unit tests that do not initialize
  // the app shim host manager.
  auto* shim_host_manager =
      g_browser_process->platform_part()->app_shim_host_manager();
  if (shim_host_manager)
    return shim_host_manager->extension_app_shim_handler();
  return nullptr;
}

const Extension* ExtensionAppShimHandler::MaybeGetExtensionOrCloseHost(
    AppShimHost* host,
    Profile** profile_out) {
  DCHECK(delegate_->ProfileExistsForPath(host->GetProfilePath()));
  Profile* profile = delegate_->ProfileForPath(host->GetProfilePath());

  const Extension* extension =
      delegate_->MaybeGetAppExtension(profile, host->GetAppId());
  if (!extension) {
    // Extensions may have been uninstalled or disabled since the shim
    // started.
    host->OnAppClosed();
  }

  if (profile_out)
    *profile_out = profile;
  return extension;
}

void ExtensionAppShimHandler::CloseBrowsersForApp(Profile* profile,
                                                  const std::string& app_id) {
  AppBrowserMap::iterator it =
      app_browser_windows_.find(std::make_pair(profile, app_id));
  if (it == app_browser_windows_.end())
    return;

  for (const Browser* browser : it->second)
    browser->window()->Close();
}

void ExtensionAppShimHandler::OnProfileLoaded(
    std::unique_ptr<AppShimHostBootstrap> bootstrap,
    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const std::string& app_id = bootstrap->GetAppId();

  // TODO(jeremya): Handle the case that launching the app fails. Probably we
  // need to watch for 'app successfully launched' or at least 'background page
  // exists/was created' and time out with failure if we don't see that sign of
  // life within a certain window.
  const Extension* extension = delegate_->MaybeGetAppExtension(profile, app_id);
  if (extension) {
    OnExtensionEnabled(std::move(bootstrap));
  } else {
    delegate_->EnableExtension(
        profile, app_id,
        base::BindOnce(&ExtensionAppShimHandler::OnExtensionEnabled,
                       weak_factory_.GetWeakPtr(), std::move(bootstrap)));
  }
}

void ExtensionAppShimHandler::OnExtensionEnabled(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AppShimLaunchType launch_type = bootstrap->GetLaunchType();
  std::vector<base::FilePath> files = bootstrap->GetLaunchFiles();

  // If the profile doesn't exist, it may have been deleted during the enable
  // prompt.
  base::FilePath profile_path =
      delegate_->GetFullProfilePath(bootstrap->GetProfilePath());
  Profile* profile = delegate_->ProfileForPath(profile_path);
  if (!profile) {
    bootstrap->OnFailedToConnectToHost(APP_SHIM_LAUNCH_PROFILE_NOT_FOUND);
    return;
  }

  // If !extension, the extension doesn't exist, or was not re-enabled.
  const Extension* extension =
      delegate_->MaybeGetAppExtension(profile, bootstrap->GetAppId());
  if (!extension) {
    bootstrap->OnFailedToConnectToHost(APP_SHIM_LAUNCH_APP_NOT_FOUND);
    return;
  }

  AppShimHost* host = delegate_->AllowShimToConnect(profile, extension)
                          ? FindOrCreateHost(profile, extension)
                          : nullptr;
  if (host) {
    if (host->HasBootstrapConnected()) {
      // If another app shim process has already connected to this (profile,
      // app_id) pair, then focus the windows for the existing process, and
      // close the new process.
      OnShimFocus(host,
                  launch_type == APP_SHIM_LAUNCH_NORMAL ? APP_SHIM_FOCUS_REOPEN
                                                        : APP_SHIM_FOCUS_NORMAL,
                  files);
      bootstrap->OnFailedToConnectToHost(APP_SHIM_LAUNCH_DUPLICATE_HOST);
      return;
    }
    if (IsAcceptablyCodeSigned(bootstrap->GetAppShimPid())) {
      host->OnBootstrapConnected(std::move(bootstrap));
    } else {
      // If the connecting shim process doesn't have an acceptable code
      // signature, reject the connection and re-launch the shim. The internal
      // re-launch will likely fail, whereupon the shim will be recreated.
      LOG(ERROR) << "The attaching app shim's code signature is invalid.";
      bootstrap->OnFailedToConnectToHost(APP_SHIM_LAUNCH_FAILED_VALIDATION);
      host->LaunchShim();
    }
  } else {
    // If it's an app that has a shim to launch it but shouldn't use a host
    // (e.g, a hosted app that opens in a tab), terminate the shim, but still
    // launch the app (that is, open the relevant browser tabs).
    bootstrap->OnFailedToConnectToHost(APP_SHIM_LAUNCH_DUPLICATE_HOST);
  }

  // If this is not a register-only launch, then launch the app (that is, open
  // a browser window for it).
  if (launch_type == APP_SHIM_LAUNCH_NORMAL)
    delegate_->LaunchApp(profile, extension, files);
}

bool ExtensionAppShimHandler::IsAcceptablyCodeSigned(pid_t pid) const {
  return IsAcceptablyCodeSignedInternal(pid);
}

void ExtensionAppShimHandler::OnShimClose(AppShimHost* host) {
  // This might be called when shutting down. Don't try to look up the profile
  // since profile_manager might not be around.
  for (HostMap::iterator it = hosts_.begin(); it != hosts_.end(); ) {
    HostMap::iterator current = it++;
    if (current->second == host)
      hosts_.erase(current);
  }
}

void ExtensionAppShimHandler::OnShimFocus(
    AppShimHost* host,
    AppShimFocusType focus_type,
    const std::vector<base::FilePath>& files) {
  if (host->UsesRemoteViews())
    return;

  Profile* profile;
  const Extension* extension = MaybeGetExtensionOrCloseHost(host, &profile);
  if (!extension)
    return;

  bool windows_focused;
  const std::string& app_id = host->GetAppId();
  if (extension->is_hosted_app()) {
    AppBrowserMap::iterator it =
        app_browser_windows_.find(std::make_pair(profile, app_id));
    if (it == app_browser_windows_.end())
      return;

    windows_focused = FocusHostedAppWindows(it->second);
  } else {
    const AppWindowList windows =
        delegate_->GetWindows(profile, host->GetAppId());
    windows_focused = FocusWindows(windows);
  }

  if (focus_type == APP_SHIM_FOCUS_NORMAL ||
      (focus_type == APP_SHIM_FOCUS_REOPEN && windows_focused)) {
    return;
  }

  delegate_->LaunchApp(profile, extension, files);
}

void ExtensionAppShimHandler::OnShimSetHidden(AppShimHost* host, bool hidden) {
  if (host->UsesRemoteViews())
    return;

  Profile* profile;
  const Extension* extension = MaybeGetExtensionOrCloseHost(host, &profile);
  if (!extension)
    return;

  if (extension->is_hosted_app())
    SetHostedAppHidden(profile, host->GetAppId(), hidden);
  else
    SetAppHidden(profile, host->GetAppId(), hidden);
}

void ExtensionAppShimHandler::OnShimQuit(AppShimHost* host) {
  if (host->UsesRemoteViews())
    return;

  DCHECK(delegate_->ProfileExistsForPath(host->GetProfilePath()));
  Profile* profile = delegate_->ProfileForPath(host->GetProfilePath());

  const std::string& app_id = host->GetAppId();
  const Extension* extension = delegate_->MaybeGetAppExtension(profile, app_id);
  if (!extension)
    return;

  if (extension->is_hosted_app()) {
    CloseBrowsersForApp(profile, app_id);
  } else {
    const AppWindowList windows = delegate_->GetWindows(profile, app_id);
    for (AppWindowRegistry::const_iterator it = windows.begin();
         it != windows.end(); ++it) {
      (*it)->GetBaseWindow()->Close();
    }
  }
  // Once the last window closes, flow will end up in OnAppDeactivated via
  // AppLifetimeMonitor.
  // Otherwise, once the last window closes for a hosted app, OnBrowserRemoved
  // will call OnAppDeactivated.
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
      // Shut down every shim associated with this profile.
      for (HostMap::iterator it = hosts_.begin(); it != hosts_.end(); ) {
        // Increment the iterator first as OnAppClosed may call back to
        // OnShimClose and invalidate the iterator.
        HostMap::iterator current = it++;
        if (profile->IsSameProfile(current->first.first)) {
          AppShimHost* host = current->second;
          host->OnAppClosed();
        }
      }
      break;
    }
    case chrome::NOTIFICATION_BROWSER_OPENED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      // Don't keep track of browsers that are not associated with an app.
      const Extension* extension = MaybeGetAppForBrowser(browser);
      if (!extension)
        return;

      BrowserSet& browsers = app_browser_windows_[std::make_pair(
          browser->profile(), extension->id())];
      browsers.insert(browser);
      if (browsers.size() == 1)
        OnAppActivated(browser->profile(), extension->id());

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
  const Extension* extension = delegate_->MaybeGetAppExtension(context, app_id);
  if (!extension)
    return;

  Profile* profile = static_cast<Profile*>(context);
  AppShimHost* host = FindOrCreateHost(profile, extension);
  if (!host)
    return;
  host->LaunchShim();
}

void ExtensionAppShimHandler::OnAppDeactivated(content::BrowserContext* context,
                                               const std::string& app_id) {
  AppShimHost* host = FindHost(static_cast<Profile*>(context), app_id);
  if (host)
    host->OnAppClosed();

  if (hosts_.empty())
    delegate_->MaybeTerminate();
}

void ExtensionAppShimHandler::OnAppStop(content::BrowserContext* context,
                                        const std::string& app_id) {}

// The BrowserWindow may be NULL when this is called.
// Therefore we listen for the notification
// chrome::NOTIFICATION_BROWSER_OPENED and then call OnAppActivated.
// If this notification is removed, check that OnBrowserAdded is called after
// the BrowserWindow is ready.
void ExtensionAppShimHandler::OnBrowserAdded(Browser* browser) {}

void ExtensionAppShimHandler::OnBrowserRemoved(Browser* browser) {
  // Note that |browser| may no longer have an extension, if it was unloaded
  // before |browser| was closed. Search for |browser| in all extensions in
  // |app_browser_windows_|.
  for (auto it = app_browser_windows_.begin(); it != app_browser_windows_.end();
       ++it) {
    const std::string& extension_id = it->first.second;
    BrowserSet& browsers = it->second;
    auto found = browsers.find(browser);
    if (found == browsers.end())
      continue;

    browsers.erase(found);
    if (browsers.empty()) {
      OnAppDeactivated(browser->profile(), extension_id);
      app_browser_windows_.erase(it);
    }
    return;
  }
}

}  // namespace apps
