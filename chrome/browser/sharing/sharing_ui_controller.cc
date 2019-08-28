// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_ui_controller.h"

#include <utility>

#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

BrowserWindow* GetWindowFromWebContents(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser ? browser->window() : nullptr;
}

content::WebContents* GetCurrentWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser ? browser->tab_strip_model()->GetActiveWebContents() : nullptr;
}

}  // namespace

SharingUiController::App::App(const gfx::VectorIcon* vector_icon,
                              const gfx::Image& image,
                              base::string16 name,
                              std::string identifier)
    : vector_icon(vector_icon),
      image(image),
      name(std::move(name)),
      identifier(std::move(identifier)) {}

SharingUiController::App::App(App&& other) = default;

SharingUiController::App::~App() = default;

SharingUiController::SharingUiController(content::WebContents* web_contents)
    : web_contents_(web_contents),
      sharing_service_(SharingServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())) {}

SharingUiController::~SharingUiController() = default;

void SharingUiController::CloseDialog() {
  if (dialog_)
    dialog_->Hide();

  // Treat the dialog as closed as the process of closing the native widget
  // might be async.
  dialog_ = nullptr;
}

void SharingUiController::ShowNewDialog() {
  CloseDialog();
  BrowserWindow* window = GetWindowFromWebContents(web_contents_);
  if (!window)
    return;

  dialog_ = DoShowDialog(window);
  UpdateIcon();
}

void SharingUiController::UpdateIcon() {
  BrowserWindow* window = GetWindowFromWebContents(web_contents_);
  if (!window)
    return;

  auto* icon_container = window->GetOmniboxPageActionIconContainer();
  if (icon_container)
    icon_container->UpdatePageActionIcon(GetIconType());
}

void SharingUiController::OnDialogClosed(SharingDialog* dialog) {
  // Ignore already replaced dialogs.
  if (dialog != dialog_)
    return;

  dialog_ = nullptr;
  UpdateIcon();
}

void SharingUiController::MaybeShowErrorDialog() {
  if (HasSendFailed() && web_contents_ == GetCurrentWebContents(web_contents_))
    ShowNewDialog();
}

void SharingUiController::SendMessageToDevice(
    const syncer::DeviceInfo& device,
    chrome_browser_sharing::SharingMessage sharing_message) {
  last_dialog_id_++;
  is_loading_ = true;
  send_result_ = SharingSendMessageResult::kSuccessful;
  UpdateIcon();

  sharing_service_->SendMessageToDevice(
      device.guid(), kSharingMessageTTL, std::move(sharing_message),
      base::Bind(&SharingUiController::OnMessageSentToDevice,
                 weak_ptr_factory_.GetWeakPtr(), last_dialog_id_));
}

void SharingUiController::OnMessageSentToDevice(
    int dialog_id,
    SharingSendMessageResult result) {
  if (dialog_id != last_dialog_id_)
    return;

  is_loading_ = false;
  send_result_ = result;
  UpdateIcon();
}

void SharingUiController::UpdateAndShowDialog() {
  last_dialog_id_++;
  is_loading_ = false;
  send_result_ = SharingSendMessageResult::kSuccessful;

  CloseDialog();
  UpdateIcon();
  DoUpdateApps(base::BindOnce(&SharingUiController::OnAppsReceived,
                              weak_ptr_factory_.GetWeakPtr(), last_dialog_id_));
}

void SharingUiController::UpdateDevices() {
  devices_ =
      sharing_service_->GetDeviceCandidates(GetRequiredDeviceCapabilities());
}

// TODO(himanshujaju): Return different title based on |send_result_|.
base::string16 SharingUiController::GetErrorDialogTitle() const {
  DCHECK(HasSendFailed());
  return l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_FAILED_TO_SEND);
}

// TODO(himanshujaju): Return different text based on |send_result_|.
base::string16 SharingUiController::GetErrorDialogText() const {
  DCHECK(HasSendFailed());
  return l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_FAILED_MESSAGE);
}

void SharingUiController::OnAppsReceived(int dialog_id, std::vector<App> apps) {
  if (dialog_id != last_dialog_id_)
    return;

  apps_ = std::move(apps);
  UpdateDevices();

  ShowNewDialog();
}

bool SharingUiController::HasSendFailed() const {
  return send_result_ != SharingSendMessageResult::kSuccessful;
}
