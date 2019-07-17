// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace settings {

CrostiniHandler::CrostiniHandler(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

CrostiniHandler::~CrostiniHandler() {
  DisallowJavascript();
}

void CrostiniHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestCrostiniInstallerView",
      base::BindRepeating(&CrostiniHandler::HandleRequestCrostiniInstallerView,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestRemoveCrostini",
      base::BindRepeating(&CrostiniHandler::HandleRequestRemoveCrostini,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniSharedPathsDisplayText",
      base::BindRepeating(
          &CrostiniHandler::HandleGetCrostiniSharedPathsDisplayText,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeCrostiniSharedPath",
      base::BindRepeating(&CrostiniHandler::HandleRemoveCrostiniSharedPath,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniSharedUsbDevices",
      base::BindRepeating(&CrostiniHandler::HandleGetCrostiniSharedUsbDevices,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setCrostiniUsbDeviceShared",
      base::BindRepeating(&CrostiniHandler::HandleSetCrostiniUsbDeviceShared,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "exportCrostiniContainer",
      base::BindRepeating(&CrostiniHandler::HandleExportCrostiniContainer,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "importCrostiniContainer",
      base::BindRepeating(&CrostiniHandler::HandleImportCrostiniContainer,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniInstallerStatus",
      base::BindRepeating(
          &CrostiniHandler::HandleCrostiniInstallerStatusRequest,
          weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnJavascriptAllowed() {
  crostini::CrostiniManager::GetForProfile(profile_)
      ->AddInstallerViewStatusObserver(this);
  if (chromeos::CrosUsbDetector::Get()) {
    chromeos::CrosUsbDetector::Get()->AddUsbDeviceObserver(this);
  }
}

void CrostiniHandler::OnJavascriptDisallowed() {
  if (crostini::CrostiniManager::GetForProfile(profile_)
          ->HasInstallerViewStatusObserver(this)) {
    crostini::CrostiniManager::GetForProfile(profile_)
        ->RemoveInstallerViewStatusObserver(this);
  }
  if (chromeos::CrosUsbDetector::Get()) {
    chromeos::CrosUsbDetector::Get()->RemoveUsbDeviceObserver(this);
  }
}

void CrostiniHandler::HandleRequestCrostiniInstallerView(
    const base::ListValue* args) {
  AllowJavascript();
  ShowCrostiniInstallerView(Profile::FromWebUI(web_ui()),
                            crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleRequestRemoveCrostini(const base::ListValue* args) {
  AllowJavascript();
  ShowCrostiniUninstallerView(Profile::FromWebUI(web_ui()),
                              crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleGetCrostiniSharedPathsDisplayText(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  const base::ListValue* paths;
  CHECK(args->GetString(0, &callback_id));
  CHECK(args->GetList(1, &paths));

  base::ListValue texts;
  for (auto it = paths->begin(); it != paths->end(); ++it) {
    texts.AppendString(file_manager::util::GetPathDisplayTextForSettings(
        profile_, it->GetString()));
  }
  ResolveJavascriptCallback(base::Value(callback_id), texts);
}

void CrostiniHandler::HandleRemoveCrostiniSharedPath(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string vm_name;
  CHECK(args->GetString(0, &vm_name));
  std::string path;
  CHECK(args->GetString(1, &path));

  guest_os::GuestOsSharePath::GetForProfile(profile_)->UnsharePath(
      vm_name, base::FilePath(path),
      /*unpersist=*/true,
      base::BindOnce(
          [](const std::string& path, bool result, std::string failure_reason) {
            if (!result) {
              LOG(ERROR) << "Error unsharing " << path << ": "
                         << failure_reason;
            }
          },
          path));
}

namespace {
base::ListValue UsbDevicesToListValue(
    const std::vector<CrosUsbDeviceInfo> shared_usbs) {
  base::ListValue usb_devices_list;
  for (auto device : shared_usbs) {
    base::Value device_info(base::Value::Type::DICTIONARY);
    device_info.SetKey("guid", base::Value(device.guid));
    device_info.SetKey("label", base::Value(device.label));
    const bool shared_in_crostini =
        device.vm_sharing_info[crostini::kCrostiniDefaultVmName].shared;
    device_info.SetKey("shared", base::Value(shared_in_crostini));
    usb_devices_list.GetList().push_back(std::move(device_info));
  }
  return usb_devices_list;
}
}  // namespace

void CrostiniHandler::HandleGetCrostiniSharedUsbDevices(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetSize());

  std::string callback_id = args->GetList()[0].GetString();

  chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
  if (!detector) {
    ResolveJavascriptCallback(base::Value(callback_id), base::ListValue());
    return;
  }

  ResolveJavascriptCallback(
      base::Value(callback_id),
      UsbDevicesToListValue(detector->GetDevicesSharableWithCrostini()));
}

void CrostiniHandler::HandleSetCrostiniUsbDeviceShared(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  const auto& args_list = args->GetList();
  std::string guid = args_list[0].GetString();
  bool shared = args_list[1].GetBool();

  chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
  if (!detector)
    return;

  if (shared) {
    detector->AttachUsbDeviceToVm(crostini::kCrostiniDefaultVmName, guid,
                                  base::DoNothing());
    return;
  }
  detector->DetachUsbDeviceFromVm(crostini::kCrostiniDefaultVmName, guid,
                                  base::DoNothing());
}

void CrostiniHandler::OnUsbDevicesChanged() {
  chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
  DCHECK(detector);  // This callback is called by the detector.
  FireWebUIListener(
      "crostini-shared-usb-devices-changed",
      UsbDevicesToListValue(detector->GetDevicesSharableWithCrostini()));
}

void CrostiniHandler::HandleExportCrostiniContainer(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  crostini::CrostiniExportImport::GetForProfile(profile_)->ExportContainer(
      web_ui()->GetWebContents());
}

void CrostiniHandler::HandleImportCrostiniContainer(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  crostini::CrostiniExportImport::GetForProfile(profile_)->ImportContainer(
      web_ui()->GetWebContents());
}

void CrostiniHandler::HandleCrostiniInstallerStatusRequest(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(0U, args->GetSize());
  bool status = crostini::CrostiniManager::GetForProfile(profile_)
                    ->GetInstallerViewStatus();
  OnCrostiniInstallerViewStatusChanged(status);
}

void CrostiniHandler::OnCrostiniInstallerViewStatusChanged(bool status) {
  // It's technically possible for this to be called before Javascript is
  // enabled, in which case we must not call FireWebUIListener
  if (IsJavascriptAllowed()) {
    // Other side listens with cr.addWebUIListener
    FireWebUIListener("crostini-installer-status-changed", base::Value(status));
  }
}

}  // namespace settings
}  // namespace chromeos
