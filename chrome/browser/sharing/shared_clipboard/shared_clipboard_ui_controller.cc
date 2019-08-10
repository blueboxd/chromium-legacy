// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_ui_controller.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "components/sync_device_info/device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/url_util.h"

using App = SharingDialogController::App;

// static
SharedClipboardUiController*
SharedClipboardUiController::GetOrCreateFromWebContents(
    content::WebContents* web_contents) {
  SharedClipboardUiController::CreateForWebContents(web_contents);
  return SharedClipboardUiController::FromWebContents(web_contents);
}

// static
void SharedClipboardUiController::DeviceSelected(
    content::WebContents* web_contents,
    const base::string16 text,
    const SharingDeviceInfo& device) {
  auto* controller = GetOrCreateFromWebContents(web_contents);
  controller->text_ = text;
  controller->OnDeviceChosen(device);
}

SharedClipboardUiController::SharedClipboardUiController(
    content::WebContents* web_contents)
    : SharingDialogController(web_contents),
      sharing_service_(SharingServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())) {}

SharedClipboardUiController::~SharedClipboardUiController() = default;

base::string16 SharedClipboardUiController::GetTitle() {
  // There is no left click dialog - so no title
  return base::string16();
}

std::vector<SharingDeviceInfo> SharedClipboardUiController::GetSyncedDevices() {
  // TODO(yasmo): change it to kSharedClipboard when cl 1745473 is submitted.
  return sharing_service_->GetDeviceCandidates(
      static_cast<int>(SharingDeviceCapability::kNone));
}

// No need for apps for shared clipboard feature
std::vector<App> SharedClipboardUiController::GetApps() {
  return std::vector<App>();
}

// No left click dialog
SharingDialog* SharedClipboardUiController::DoShowDialog(
    BrowserWindow* window) {
  return nullptr;
}

void SharedClipboardUiController::OnDeviceChosen(
    const SharingDeviceInfo& device) {
  StartLoading();

  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_shared_clipboard_message()->set_text(
      base::UTF16ToUTF8(text_));

  sharing_service_->SendMessageToDevice(
      device.guid(), kSharingMessageTTL, std::move(sharing_message),
      base::Bind(&SharedClipboardUiController::OnMessageSentToDevice,
                 weak_ptr_factory_.GetWeakPtr(), 0));
}

void SharedClipboardUiController::OnMessageSentToDevice(int dialog_id,
                                                        bool success) {
  // Do nothing
}

void SharedClipboardUiController::OnAppChosen(const App& app) {
  // Do nothing - there is no apps
}

void SharedClipboardUiController::OnHelpTextClicked() {
  // No help text
}

PageActionIconType SharedClipboardUiController::GetIconType() {
  return PageActionIconType::kSendTabToSelf;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharedClipboardUiController)
