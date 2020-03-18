// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/extensions_api_permissions.h"

#include <stddef.h>

#include <memory>

#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/permissions/usb_device_permission.h"

namespace extensions {
namespace api_permissions {

namespace {

template <typename T>
std::unique_ptr<APIPermission> CreateAPIPermission(
    const APIPermissionInfo* permission) {
  return std::make_unique<T>(permission);
}

// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageRule::GetAllRules as well.
constexpr APIPermissionInfo::InitInfo permissions_to_register[] = {
    {APIPermission::kAlarms, "alarms",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAlphaEnabled, "app.window.alpha",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAlwaysOnTopWindows, "app.window.alwaysOnTop",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAppView, "appview",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAudio, "audio",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAudioCapture, "audioCapture",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kBluetoothPrivate, "bluetoothPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCecPrivate, "cecPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kClipboard, "clipboard",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kClipboardRead, "clipboardRead",
     APIPermissionInfo::kFlagSupportsContentCapabilities},
    {APIPermission::kClipboardWrite, "clipboardWrite",
     APIPermissionInfo::kFlagSupportsContentCapabilities |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCrashReportPrivate, "crashReportPrivate",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDeclarativeWebRequest, "declarativeWebRequest"},
    {APIPermission::kDiagnostics, "diagnostics",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kDisplaySource, "displaySource",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDns, "dns",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDocumentScan, "documentScan"},
    {APIPermission::kExternallyConnectableAllUrls,
     "externally_connectable.all_urls",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFeedbackPrivate, "feedbackPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFullscreen, "app.window.fullscreen",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    // The permission string for "fileSystem" is only shown when
    // "write" or "directory" is present. Read-only access is only
    // granted after the user has been shown a file or directory
    // chooser dialog and selected a file or directory. Selecting
    // the file or directory is considered consent to read it.
    {APIPermission::kFileSystem, "fileSystem",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileSystemDirectory, "fileSystem.directory",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileSystemRequestFileSystem,
     "fileSystem.requestFileSystem",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileSystemRetainEntries, "fileSystem.retainEntries",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileSystemWrite, "fileSystem.write",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    {APIPermission::kHid, "hid",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kImeWindowEnabled, "app.window.ime",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kOverrideEscFullscreen, "app.window.fullscreen.overrideEsc",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kIdle, "idle",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kLockScreen, "lockScreen",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kLockWindowFullscreenPrivate, "lockWindowFullscreenPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kLogin, "login",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kLoginScreenStorage, "loginScreenStorage",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kLoginScreenUi, "loginScreenUi",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kLoginState, "loginState",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kMediaPerceptionPrivate, "mediaPerceptionPrivate",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kMetricsPrivate, "metricsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kNativeMessaging, "nativeMessaging",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kNetworkingConfig, "networking.config",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kNetworkingOnc, "networking.onc",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kNetworkingPrivate, "networkingPrivate",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kNewTabPageOverride, "newTabPageOverride",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagRequiresManagementUIWarning |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPower, "power",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPrinterProvider, "printerProvider",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPrinting, "printing",
     APIPermissionInfo::kFlagRequiresManagementUIWarning |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPrintingMetrics, "printingMetrics",
     APIPermissionInfo::kFlagRequiresManagementUIWarning |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSerial, "serial",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSocket, "socket",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning,
     &CreateAPIPermission<SocketPermission>},
    {APIPermission::kStorage, "storage",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemCpu, "system.cpu",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemMemory, "system.memory",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemNetwork, "system.network",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemDisplay, "system.display",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemPowerSource, "system.powerSource",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemStorage, "system.storage",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kU2fDevices, "u2fDevices",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kUnlimitedStorage, "unlimitedStorage",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagSupportsContentCapabilities |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kUsb, "usb", APIPermissionInfo::kFlagNone},
    {APIPermission::kUsbDevice, "usbDevices",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning,
     &CreateAPIPermission<UsbDevicePermission>},
    {APIPermission::kVideoCapture, "videoCapture",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kVirtualKeyboard, "virtualKeyboard",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kVpnProvider, "vpnProvider",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    // NOTE(kalman): This is provided by a manifest property but needs to
    // appear in the install permission dialogue, so we need a fake
    // permission for it. See http://crbug.com/247857.
    {APIPermission::kWebConnectable, "webConnectable",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagInternal},
    {APIPermission::kWebRequest, "webRequest"},
    {APIPermission::kWebRequestBlocking, "webRequestBlocking",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDeclarativeNetRequest,
     declarative_net_request::kAPIPermission,
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWebView, "webview",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWindowShape, "app.window.shape",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileSystemRequestDownloads, "fileSystem.requestDownloads",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kDeclarativeNetRequestFeedback,
     declarative_net_request::kFeedbackAPIPermission,
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
};

}  // namespace

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return base::make_span(permissions_to_register);
}

base::span<const Alias> GetPermissionAliases() {
  // In alias constructor, first value is the alias name; second value is the
  // real name. See also alias.h.
  static constexpr Alias aliases[] = {
      Alias("alwaysOnTopWindows", "app.window.alwaysOnTop"),
      Alias("fullscreen", "app.window.fullscreen"),
      Alias("overrideEscFullscreen", "app.window.fullscreen.overrideEsc"),
      Alias("unlimited_storage", "unlimitedStorage")};

  return base::make_span(aliases);
}

}  // namespace api_permissions
}  // namespace extensions
