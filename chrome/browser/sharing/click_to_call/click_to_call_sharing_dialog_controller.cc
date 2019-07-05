// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_sharing_dialog_controller.h"

#include "base/strings/strcat.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/shell_integration.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

using SharingMessage = chrome_browser_sharing::SharingMessage;
using App = ClickToCallSharingDialogController::App;

constexpr base::TimeDelta
    ClickToCallSharingDialogController::kMessageExpiration;

ClickToCallSharingDialogController::ClickToCallSharingDialogController(
    content::WebContents* web_contents,
    SharingService* sharing_service,
    std::string phone_number)
    : web_contents_(web_contents),
      sharing_service_(sharing_service),
      phone_number_(std::move(phone_number)),
      phone_url_(base::StrCat({"tel:", phone_number_})) {}

ClickToCallSharingDialogController::~ClickToCallSharingDialogController() =
    default;

std::string ClickToCallSharingDialogController::GetTitle() {
  return l10n_util::GetStringUTF8(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
}

std::vector<SharingDeviceInfo>
ClickToCallSharingDialogController::GetSyncedDevices() {
  return sharing_service_->GetDeviceCandidates(
      static_cast<int>(SharingDeviceCapability::kTelephony));
}

std::vector<App> ClickToCallSharingDialogController::GetApps() {
  base::string16 app_name =
      shell_integration::GetApplicationNameForProtocol(phone_url_);

  std::vector<App> apps;
  if (!app_name.empty()) {
    // TODO: Get the icon and ID and test
    apps.emplace_back(nullptr, std::move(app_name), nullptr);
  }
  return apps;
}

void ClickToCallSharingDialogController::OnDeviceChosen(
    const SharingDeviceInfo& device,
    SharingService::SendMessageCallback callback) {
  SharingMessage sharing_message;
  sharing_message.mutable_click_to_call_message()->set_phone_number(
      phone_number_);
  sharing_service_->SendMessageToDevice(
      device.guid(), ClickToCallSharingDialogController::kMessageExpiration,
      std::move(sharing_message), std::move(callback));
}

void ClickToCallSharingDialogController::OnAppChosen(App app) {
  ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(phone_url_,
                                                         web_contents_);
}
