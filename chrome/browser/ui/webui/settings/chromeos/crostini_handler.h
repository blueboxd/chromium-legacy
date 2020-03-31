// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/usb/cros_usb_detector.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"

class Profile;

namespace crostini {
enum class CrostiniResult;
struct CrostiniDiskInfo;
}  // namespace crostini

namespace chromeos {
namespace settings {

class CrostiniHandler : public ::settings::SettingsPageUIHandler,
                        public crostini::CrostiniDialogStatusObserver,
                        public crostini::CrostiniExportImport::Observer,
                        public crostini::CrostiniContainerPropertiesObserver,
                        public chromeos::CrosUsbDeviceObserver {
 public:
  explicit CrostiniHandler(Profile* profile);
  ~CrostiniHandler() override;

  // SettingsPageUIHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleRequestCrostiniInstallerView(const base::ListValue* args);
  void HandleRequestRemoveCrostini(const base::ListValue* args);
  // Callback for the "getSharedPathsDisplayText" message.  Converts actual
  // paths in chromeos to values suitable to display to users.
  // E.g. /home/chronos/u-<hash>/Downloads/foo => "Downloads > foo".
  void HandleGetCrostiniSharedPathsDisplayText(const base::ListValue* args);
  // Remove a specified path from being shared.
  void HandleRemoveCrostiniSharedPath(const base::ListValue* args);
  void OnCrostiniSharedPathRemoved(const std::string& callback_id,
                                   const std::string& path,
                                   bool result,
                                   const std::string& failure_reason);
  // Returns a list of available USB devices.
  void HandleGetCrostiniSharedUsbDevices(const base::ListValue* args);
  // Set the share state of a USB device.
  void HandleSetCrostiniUsbDeviceShared(const base::ListValue* args);
  // chromeos::SharedUsbDeviceObserver.
  void OnUsbDevicesChanged() override;
  // Export the crostini container.
  void HandleExportCrostiniContainer(const base::ListValue* args);
  // Import the crostini container.
  void HandleImportCrostiniContainer(const base::ListValue* args);
  // Handle a request for the CrostiniInstallerView status.
  void HandleCrostiniInstallerStatusRequest(const base::ListValue* args);
  // crostini::CrostiniDialogStatusObserver
  void OnCrostiniDialogStatusChanged(crostini::DialogType dialog_type,
                                     bool open) override;
  // crostini::CrostiniContainerPropertiesObserver
  void OnContainerOsReleaseChanged(const crostini::ContainerId& container_id,
                                   bool can_upgrade) override;
  // Handle a request for the CrostiniExportImport operation status.
  void HandleCrostiniExportImportOperationStatusRequest(
      const base::ListValue* args);
  // CrostiniExportImport::Observer:
  void OnCrostiniExportImportOperationStatusChanged(bool in_progress) override;
  // Handle a request for querying status of ARC adb sideloading.
  void HandleQueryArcAdbRequest(const base::ListValue* args);
  // Handle a request for enabling adb sideloading in ARC.
  void HandleEnableArcAdbRequest(const base::ListValue* args);
  // Handle a request for disabling adb sideloading in ARC.
  void HandleDisableArcAdbRequest(const base::ListValue* args);
  // Launch the Crostini terminal.
  void LaunchTerminal();
  // Handle a request for showing the container upgrade view.
  void HandleRequestContainerUpgradeView(const base::ListValue* args);
  // Callback of HandleQueryArcAdbRequest.
  void OnQueryAdbSideload(
      SessionManagerClient::AdbSideloadResponseCode response_code,
      bool enabled);
  // Returns whether the current user can change adb sideloading configuration
  // on current device.
  bool CheckEligibilityToChangeArcAdbSideloading() const;
  // Handle a request for the CrostiniUpgraderDialog status.
  void HandleCrostiniUpgraderDialogStatusRequest(const base::ListValue* args);
  // Handle a request for the availability of a container upgrade.
  void HandleCrostiniContainerUpgradeAvailableRequest(
      const base::ListValue* args);
  // Handles a request for forwarding a new port.
  void HandleAddCrostiniPortForward(const base::ListValue* args);
  // Callback of port forwarding requests.
  void OnPortForwardComplete(std::string callback_id, bool success);
  // Fetches disk info for a VM, can be slow (seconds).
  void HandleGetCrostiniDiskInfo(const base::ListValue* args);
  void ResolveGetCrostiniDiskInfoCallback(
      const std::string& callback_id,
      std::unique_ptr<crostini::CrostiniDiskInfo> disk_info);
  // Handles a request to resize a Crostini disk.
  void HandleResizeCrostiniDisk(const base::ListValue* args);
  void ResolveResizeCrostiniDiskCallback(const std::string& callback_id,
                                         bool succeeded);
  // Checks if a restart is required to update mic sharing settings.
  void HandleCheckCrostiniMicSharingStatus(const base::ListValue* args);

  Profile* profile_;
  // weak_ptr_factory_ should always be last member.
  base::WeakPtrFactory<CrostiniHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_
