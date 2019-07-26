// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_sharing_dialog_controller.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_constants.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_dialog.h"
#include "chrome/browser/sharing/sharing_device_info.h"
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

using SharingMessage = chrome_browser_sharing::SharingMessage;
using App = ClickToCallSharingDialogController::App;

// static
ClickToCallSharingDialogController*
ClickToCallSharingDialogController::GetOrCreateFromWebContents(
    content::WebContents* web_contents) {
  ClickToCallSharingDialogController::CreateForWebContents(web_contents);
  return ClickToCallSharingDialogController::FromWebContents(web_contents);
}

// static
void ClickToCallSharingDialogController::ShowDialog(
    content::WebContents* web_contents,
    const GURL& url,
    bool hide_default_handler) {
  auto* controller = GetOrCreateFromWebContents(web_contents);
  // Invalidate old dialog results.
  controller->last_dialog_id_++;
  controller->phone_url_ = url;
  controller->is_loading_ = false;
  controller->send_failed_ = false;
  controller->hide_default_handler_ = hide_default_handler;
  controller->ShowNewDialog();
}

// static
void ClickToCallSharingDialogController::DeviceSelected(
    content::WebContents* web_contents,
    const GURL& url,
    const SharingDeviceInfo& device) {
  auto* controller = GetOrCreateFromWebContents(web_contents);
  // Invalidate old dialog results.
  controller->last_dialog_id_++;
  controller->phone_url_ = url;
  controller->OnDeviceChosen(device);
}

ClickToCallSharingDialogController::ClickToCallSharingDialogController(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      sharing_service_(SharingServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())) {}

ClickToCallSharingDialogController::~ClickToCallSharingDialogController() =
    default;

base::string16 ClickToCallSharingDialogController::GetTitle() {
  return l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
}

void ClickToCallSharingDialogController::ShowNewDialog() {
  if (dialog_)
    dialog_->Hide();

  // Treat the dialog as closed as the process of closing the native widget
  // might be async.
  dialog_ = nullptr;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  auto* window = browser ? browser->window() : nullptr;
  if (!window)
    return;

  dialog_ = window->ShowClickToCallDialog(web_contents_, this);
  UpdateIcon();
}

void ClickToCallSharingDialogController::ShowErrorDialog() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser)
    return;

  if (web_contents_ == browser->tab_strip_model()->GetActiveWebContents())
    ShowNewDialog();
}

void ClickToCallSharingDialogController::UpdateIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  auto* window = browser ? browser->window() : nullptr;
  if (!window)
    return;

  auto* icon_container = window->GetOmniboxPageActionIconContainer();
  if (icon_container)
    icon_container->UpdatePageActionIcon(PageActionIconType::kClickToCall);
}

std::vector<SharingDeviceInfo>
ClickToCallSharingDialogController::GetSyncedDevices() {
  return sharing_service_->GetDeviceCandidates(
      static_cast<int>(SharingDeviceCapability::kTelephony));
}

std::vector<App> ClickToCallSharingDialogController::GetApps() {
  std::vector<App> apps;
  if (hide_default_handler_)
    return apps;

  base::string16 app_name =
      shell_integration::GetApplicationNameForProtocol(phone_url_);

  if (!app_name.empty()) {
    apps.emplace_back(vector_icons::kOpenInNewIcon, std::move(app_name),
                      std::string());
  }
  return apps;
}

void ClickToCallSharingDialogController::OnDeviceChosen(
    const SharingDeviceInfo& device) {
  is_loading_ = true;
  send_failed_ = false;
  UpdateIcon();

  std::string phone_number_string(phone_url_.GetContent());
  url::RawCanonOutputT<base::char16> unescaped_phone_number;
  url::DecodeURLEscapeSequences(
      phone_number_string.data(), phone_number_string.size(),
      url::DecodeURLMode::kUTF8OrIsomorphic, &unescaped_phone_number);

  SharingMessage sharing_message;
  sharing_message.mutable_click_to_call_message()->set_phone_number(
      base::UTF16ToUTF8(base::string16(unescaped_phone_number.data(),
                                       unescaped_phone_number.length())));

  sharing_service_->SendMessageToDevice(
      device.guid(), kSharingClickToCallMessageTTL, std::move(sharing_message),
      base::Bind(&ClickToCallSharingDialogController::OnMessageSentToDevice,
                 weak_ptr_factory_.GetWeakPtr(), last_dialog_id_));
}

void ClickToCallSharingDialogController::OnMessageSentToDevice(int dialog_id,
                                                               bool success) {
  if (dialog_id != last_dialog_id_)
    return;

  is_loading_ = false;
  send_failed_ = !success;
  UpdateIcon();

  if (!success)
    ShowErrorDialog();
}

void ClickToCallSharingDialogController::OnAppChosen(const App& app) {
  ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(phone_url_,
                                                         web_contents_);
}

void ClickToCallSharingDialogController::OnDialogClosed(
    ClickToCallDialog* dialog) {
  // Ignore already replaced dialogs.
  if (dialog != dialog_)
    return;

  dialog_ = nullptr;
  UpdateIcon();
}

void ClickToCallSharingDialogController::OnHelpTextClicked() {
  ShowSingletonTab(chrome::FindBrowserWithWebContents(web_contents_),
                   GURL(chrome::kSyncLearnMoreURL));
}

ClickToCallDialog* ClickToCallSharingDialogController::GetDialog() const {
  return dialog_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClickToCallSharingDialogController)
