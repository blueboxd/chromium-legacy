// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_api_permissions.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/settings_override_permission.h"

namespace extensions {
namespace chrome_api_permissions {

namespace {

template <typename T>
std::unique_ptr<APIPermission> CreateAPIPermission(
    const APIPermissionInfo* permission) {
  return std::make_unique<T>(permission);
}

// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageProvider::GetPermissionMessages as well.
constexpr APIPermissionInfo::InitInfo permissions_to_register[] = {
    // Register permissions for all extension types.
    {APIPermission::kBackground, "background",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDeclarativeContent, "declarativeContent"},
    {APIPermission::kDesktopCapture, "desktopCapture"},
    {APIPermission::kDesktopCapturePrivate, "desktopCapturePrivate",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDownloads, "downloads"},
    {APIPermission::kDownloadsOpen, "downloads.open"},
    {APIPermission::kDownloadsShelf, "downloads.shelf",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kIdentity, "identity"},
    {APIPermission::kIdentityEmail, "identity.email"},
    {APIPermission::kExperimental, "experimental",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kGeolocation, "geolocation",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kNotifications, "notifications",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kGcm, "gcm",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    // Register extension permissions.
    {APIPermission::kAccessibilityFeaturesModify,
     "accessibilityFeatures.modify",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAccessibilityFeaturesRead, "accessibilityFeatures.read"},
    {APIPermission::kAccessibilityPrivate, "accessibilityPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kActiveTab, "activeTab"},
    {APIPermission::kBookmark, "bookmarks"},
    {APIPermission::kBrailleDisplayPrivate, "brailleDisplayPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kBrowsingData, "browsingData",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCertificateProvider, "certificateProvider",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kContentSettings, "contentSettings"},
    {APIPermission::kContextMenus, "contextMenus",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCookie, "cookies",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCryptotokenPrivate, "cryptotokenPrivate",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDataReductionProxy, "dataReductionProxy",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kEnterpriseDeviceAttributes, "enterprise.deviceAttributes",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kEnterpriseHardwarePlatform, "enterprise.hardwarePlatform",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kEnterprisePlatformKeys, "enterprise.platformKeys",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileBrowserHandler, "fileBrowserHandler",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFontSettings, "fontSettings",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kHistory, "history",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kIdltest, "idltest"},
    {APIPermission::kInput, "input"},
    {APIPermission::kManagement, "management"},
    {APIPermission::kMDns, "mdns",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPlatformKeys, "platformKeys",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPrivacy, "privacy"},
    {APIPermission::kProcesses, "processes",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kSessions, "sessions"},
    {APIPermission::kSignedInDevices, "signedInDevices"},
    {APIPermission::kTab, "tabs",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kTopSites, "topSites",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kTransientBackground, "transientBackground",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kTts, "tts", APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kTtsEngine, "ttsEngine",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWallpaper, "wallpaper",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWebNavigation, "webNavigation",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},

    // Register private permissions.
    {APIPermission::kActivityLogPrivate, "activityLogPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAutoTestPrivate, "autotestPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kBookmarkManagerPrivate, "bookmarkManagerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCast, "cast", APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kChromeosInfoPrivate, "chromeosInfoPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCommandsAccessibility, "commands.accessibility",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCommandLinePrivate, "commandLinePrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDeveloperPrivate, "developerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDownloadsInternal, "downloadsInternal",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileBrowserHandlerInternal, "fileBrowserHandlerInternal",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileManagerPrivate, "fileManagerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kIdentityPrivate, "identityPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWebcamPrivate, "webcamPrivate",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kMediaPlayerPrivate, "mediaPlayerPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kMediaRouterPrivate, "mediaRouterPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kNetworkingCastPrivate, "networking.castPrivate",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemPrivate, "systemPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCloudPrintPrivate, "cloudPrintPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kInputMethodPrivate, "inputMethodPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kEchoPrivate, "echoPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kImageWriterPrivate, "imageWriterPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kRtcPrivate, "rtcPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kTerminalPrivate, "terminalPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kVirtualKeyboardPrivate, "virtualKeyboardPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWallpaperPrivate, "wallpaperPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWebstorePrivate, "webstorePrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kEnterprisePlatformKeysPrivate,
     "enterprise.platformKeysPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kEnterpriseReportingPrivate, "enterprise.reportingPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWebrtcAudioPrivate, "webrtcAudioPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWebrtcDesktopCapturePrivate, "webrtcDesktopCapturePrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWebrtcLoggingPrivate, "webrtcLoggingPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWebrtcLoggingPrivateAudioDebug,
     "webrtcLoggingPrivate.audioDebug",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSettingsPrivate, "settingsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAutofillAssistantPrivate, "autofillAssistantPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAutofillPrivate, "autofillPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPasswordsPrivate, "passwordsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kUsersPrivate, "usersPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kLanguageSettingsPrivate, "languageSettingsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kResourcesPrivate, "resourcesPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSafeBrowsingPrivate, "safeBrowsingPrivate",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    // Full url access permissions.
    {APIPermission::kDebugger, "debugger",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kDevtools, "devtools",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal},
    {APIPermission::kPageCapture, "pageCapture",
     APIPermissionInfo::kFlagImpliesFullURLAccess},
    {APIPermission::kTabCapture, "tabCapture",
     APIPermissionInfo::kFlagImpliesFullURLAccess},
    {APIPermission::kTabCaptureForTab, "tabCaptureForTab",
     APIPermissionInfo::kFlagInternal},
    {APIPermission::kProxy, "proxy",
     APIPermissionInfo::kFlagImpliesFullURLAccess |
         APIPermissionInfo::kFlagCannotBeOptional},

    // Platform-app permissions.

    {APIPermission::kFileSystemProvider, "fileSystemProvider",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCastStreaming, "cast.streaming",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kLauncherSearchProvider, "launcherSearchProvider",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    // Settings override permissions.
    {APIPermission::kHomepage, "homepage",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal,
     &CreateAPIPermission<SettingsOverrideAPIPermission>},
    {APIPermission::kSearchProvider, "searchProvider",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal,
     &CreateAPIPermission<SettingsOverrideAPIPermission>},
    {APIPermission::kStartupPages, "startupPages",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal,
     &CreateAPIPermission<SettingsOverrideAPIPermission>},
};

}  // namespace

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return base::make_span(permissions_to_register);
}

base::span<const Alias> GetPermissionAliases() {
  // In alias constructor, first value is the alias name; second value is the
  // real name. See also alias.h.
  static constexpr Alias aliases[] = {Alias("windows", "tabs")};

  return base::make_span(aliases);
}

}  // namespace chrome_api_permissions
}  // namespace extensions
