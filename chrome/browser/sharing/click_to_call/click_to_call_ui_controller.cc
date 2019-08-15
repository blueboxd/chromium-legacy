// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"

#include <utility>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "components/sync_device_info/device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/url_util.h"

using SharingMessage = chrome_browser_sharing::SharingMessage;
using App = ClickToCallUiController::App;

// static
ClickToCallUiController* ClickToCallUiController::GetOrCreateFromWebContents(
    content::WebContents* web_contents) {
  ClickToCallUiController::CreateForWebContents(web_contents);
  return ClickToCallUiController::FromWebContents(web_contents);
}

// static
void ClickToCallUiController::ShowDialog(content::WebContents* web_contents,
                                         const GURL& url,
                                         bool hide_default_handler) {
  auto* controller = GetOrCreateFromWebContents(web_contents);
  controller->phone_url_ = url;
  controller->hide_default_handler_ = hide_default_handler;
  controller->UpdateAndShowDialog();
}

ClickToCallUiController::ClickToCallUiController(
    content::WebContents* web_contents)
    : SharingUiController(web_contents) {}

ClickToCallUiController::~ClickToCallUiController() = default;

void ClickToCallUiController::OnDeviceSelected(
    const GURL& url,
    const SharingDeviceInfo& device) {
  phone_url_ = url;
  OnDeviceChosen(device);
}

base::string16 ClickToCallUiController::GetTitle() {
  return l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_LABEL);
}

PageActionIconType ClickToCallUiController::GetIconType() {
  return PageActionIconType::kClickToCall;
}

int ClickToCallUiController::GetRequiredDeviceCapabilities() {
  return static_cast<int>(SharingDeviceCapability::kTelephony);
}

void ClickToCallUiController::DoUpdateApps(UpdateAppsCallback callback) {
  std::vector<App> apps;
  if (hide_default_handler_) {
    std::move(callback).Run(std::move(apps));
    return;
  }

  base::string16 app_name =
      shell_integration::GetApplicationNameForProtocol(phone_url_);

  if (!app_name.empty()) {
    apps.emplace_back(&vector_icons::kOpenInNewIcon, gfx::Image(),
                      std::move(app_name), std::string());
  }
  std::move(callback).Run(std::move(apps));
}

void ClickToCallUiController::OnDeviceChosen(const SharingDeviceInfo& device) {
  std::string phone_number_string(phone_url_.GetContent());
  url::RawCanonOutputT<base::char16> unescaped_phone_number;
  url::DecodeURLEscapeSequences(
      phone_number_string.data(), phone_number_string.size(),
      url::DecodeURLMode::kUTF8OrIsomorphic, &unescaped_phone_number);

  SharingMessage sharing_message;
  sharing_message.mutable_click_to_call_message()->set_phone_number(
      base::UTF16ToUTF8(base::string16(unescaped_phone_number.data(),
                                       unescaped_phone_number.length())));

  SendMessageToDevice(device, std::move(sharing_message));
}

void ClickToCallUiController::OnAppChosen(const App& app) {
  ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(phone_url_,
                                                         web_contents());
}

void ClickToCallUiController::OnHelpTextClicked() {
  ShowSingletonTab(chrome::FindBrowserWithWebContents(web_contents()),
                   GURL(chrome::kSyncLearnMoreURL));
}

SharingDialog* ClickToCallUiController::DoShowDialog(BrowserWindow* window) {
  return window->ShowClickToCallDialog(web_contents(), this);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClickToCallUiController)
