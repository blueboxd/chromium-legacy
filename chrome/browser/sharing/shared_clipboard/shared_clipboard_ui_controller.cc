// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_ui_controller.h"

#include <utility>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

using App = SharingUiController::App;

// static
SharedClipboardUiController*
SharedClipboardUiController::GetOrCreateFromWebContents(
    content::WebContents* web_contents) {
  SharedClipboardUiController::CreateForWebContents(web_contents);
  return SharedClipboardUiController::FromWebContents(web_contents);
}

SharedClipboardUiController::SharedClipboardUiController(
    content::WebContents* web_contents)
    : SharingUiController(web_contents) {}

SharedClipboardUiController::~SharedClipboardUiController() = default;

void SharedClipboardUiController::OnDeviceSelected(
    const base::string16& text,
    const SharingDeviceInfo& device) {
  text_ = text;
  OnDeviceChosen(device);
}

base::string16 SharedClipboardUiController::GetTitle() {
  // There is no left click dialog - so no title
  return base::string16();
}

PageActionIconType SharedClipboardUiController::GetIconType() {
  return PageActionIconType::kSendTabToSelf;
}

int SharedClipboardUiController::GetRequiredDeviceCapabilities() {
  return static_cast<int>(SharingDeviceCapability::kSharedClipboard);
}

// No need for apps for shared clipboard feature
void SharedClipboardUiController::DoUpdateApps(UpdateAppsCallback callback) {
  std::move(callback).Run(std::vector<App>());
}

// No left click dialog
SharingDialog* SharedClipboardUiController::DoShowDialog(
    BrowserWindow* window) {
  return nullptr;
}

void SharedClipboardUiController::OnDeviceChosen(
    const SharingDeviceInfo& device) {
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_shared_clipboard_message()->set_text(
      base::UTF16ToUTF8(text_));

  SendMessageToDevice(device, std::move(sharing_message));
}

void SharedClipboardUiController::OnAppChosen(const App& app) {
  // Do nothing - there is no apps
}

void SharedClipboardUiController::OnHelpTextClicked() {
  // No help text
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharedClipboardUiController)
