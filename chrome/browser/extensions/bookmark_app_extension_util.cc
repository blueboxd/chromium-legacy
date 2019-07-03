// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_extension_util.h"

#include <utility>

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"

#if defined(OS_MACOSX)
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut_mac.h"
#include "chrome/common/chrome_switches.h"
#endif

#if defined(OS_CHROMEOS)
// gn check complains on Linux Ozone.
#include "ash/public/cpp/shelf_model.h"  // nogncheck
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#endif

namespace extensions {

namespace {

#if !defined(OS_CHROMEOS)
bool CanOsAddDesktopShortcuts() {
#if defined(OS_LINUX) || defined(OS_WIN)
  return true;
#else
  return false;
#endif
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace

bool CanBookmarkAppCreateOsShortcuts() {
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

void BookmarkAppCreateOsShortcuts(
    Profile* profile,
    const Extension* extension,
    bool add_to_desktop,
    base::OnceCallback<void(bool created_shortcuts)> callback) {
  DCHECK(CanBookmarkAppCreateOsShortcuts());
#if !defined(OS_CHROMEOS)
  web_app::ShortcutLocations creation_locations;
  creation_locations.applications_menu_location =
      web_app::APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  creation_locations.in_quick_launch_bar = false;

  if (CanOsAddDesktopShortcuts())
    creation_locations.on_desktop = add_to_desktop;

  Profile* current_profile = profile->GetOriginalProfile();
  web_app::CreateShortcuts(web_app::SHORTCUT_CREATION_BY_USER,
                           creation_locations, current_profile, extension,
                           std::move(callback));
#endif  // !defined(OS_CHROMEOS)
}

bool CanBookmarkAppBePinnedToShelf() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

void BookmarkAppPinToShelf(const Extension* extension) {
  DCHECK(CanBookmarkAppBePinnedToShelf());
#if defined(OS_CHROMEOS)
  // ChromeLauncherController does not exist in unit tests.
  if (auto* controller = ChromeLauncherController::instance()) {
    controller->PinAppWithID(extension->id());
    controller->UpdateV1AppState(extension->id());
  }
#endif  // defined(OS_CHROMEOS)
}

bool CanBookmarkAppReparentTab(Profile* profile,
                               const Extension* extension,
                               bool shortcut_created) {
  // Reparent the web contents into its own window only if that is the
  // extension's launch type.
  if (!extension ||
      extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile),
                                extension) != extensions::LAUNCH_TYPE_WINDOW) {
    return false;
  }
#if defined(OS_MACOSX)
  // On macOS it is only possible to reparent the window when the shortcut (app
  // shim) was created.  See https://crbug.com/915571.
  return shortcut_created;
#else
  return true;
#endif
}

void BookmarkAppReparentTab(content::WebContents* contents,
                            const Extension* extension) {
  // Reparent the tab into an app window immediately when opening as a window.
  ReparentWebContentsIntoAppBrowser(contents, extension);
}

bool CanBookmarkAppRevealAppShim() {
#if defined(OS_MACOSX)
  return true;
#else   // defined(OS_MACOSX)
  return false;
#endif  // !defined(OS_MACOSX)
}

void BookmarkAppRevealAppShim(Profile* profile, const Extension* extension) {
  DCHECK(CanBookmarkAppRevealAppShim());
#if defined(OS_MACOSX)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableHostedAppShimCreation)) {
    Profile* current_profile = profile->GetOriginalProfile();
    web_app::RevealAppShimInFinderForApp(current_profile, extension);
  }
#endif  // defined(OS_MACOSX)
}

}  // namespace extensions
