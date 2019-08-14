// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/password_manager_internals_ui_ios.h"

#include "base/hash/hash.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/grit/components_resources.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_handler_helper.h"
#include "components/version_ui/version_ui_constants.h"
#import "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/passwords/password_manager_log_router_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#include "net/base/escape.h"

using autofill::LogRouter;

namespace {

web::WebUIIOSDataSource* CreatePasswordManagerInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIPasswordManagerInternalsHost);

  source->AddResourcePath("autofill_and_password_manager_internals.js",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_JS);
  source->AddResourcePath("autofill_and_password_manager_internals.css",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_HTML);
  // Data strings:
  source->AddString(version_ui::kVersion, version_info::GetVersionNumber());
  source->AddString(version_ui::kOfficial, version_info::IsOfficialBuild()
                                               ? "official"
                                               : "Developer build");
  source->AddString(version_ui::kVersionModifier,
                    GetChannelString(GetChannel()));
  source->AddString(version_ui::kCL, version_info::GetLastChange());
  source->AddString(version_ui::kUserAgent, web::GetWebClient()->GetUserAgent(
                                                web::UserAgentType::MOBILE));
  source->AddString("app_locale",
                    GetApplicationContext()->GetApplicationLocale());
  return source;
}

}  //  namespace

PasswordManagerInternalsUIIOS::PasswordManagerInternalsUIIOS(
    web::WebUIIOS* web_ui)
    : WebUIIOSController(web_ui) {
  web_ui->GetWebState()->AddObserver(this);
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui);
  DCHECK(browser_state);
  web::WebUIIOSDataSource::Add(browser_state,
                               CreatePasswordManagerInternalsHTMLSource());
}

PasswordManagerInternalsUIIOS::~PasswordManagerInternalsUIIOS() {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui());
  DCHECK(browser_state);
  if (registered_with_log_router_) {
    registered_with_log_router_ = false;
    autofill::LogRouter* log_router =
        ios::PasswordManagerLogRouterFactory::GetForBrowserState(browser_state);
    if (log_router)
      log_router->UnregisterReceiver(this);
  }
  web_ui()->GetWebState()->RemoveObserver(this);
}

void PasswordManagerInternalsUIIOS::LogEntry(const base::Value& entry) {
  if (!registered_with_log_router_ || entry.is_none())
    return;
  web_ui()->CallJavascriptFunction("addRawLog", {&entry});
}

void PasswordManagerInternalsUIIOS::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status != web::PageLoadCompletionStatus::SUCCESS)
    return;

  web_ui()->CallJavascriptFunction("setUpPasswordManagerInternals", {});
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui());
  autofill::LogRouter* log_router =
      ios::PasswordManagerLogRouterFactory::GetForBrowserState(browser_state);
  // No service means the WebUI is displayed in Incognito.
  base::Value is_incognito(!log_router);

  std::vector<const base::Value*> args{&is_incognito};
  web_ui()->CallJavascriptFunction("notifyAboutIncognito", args);

  if (log_router) {
    registered_with_log_router_ = true;

    const auto& past_logs = log_router->RegisterReceiver(this);
    for (const auto& entry : past_logs)
      LogEntry(entry);
  }
}

void PasswordManagerInternalsUIIOS::WebStateDestroyed(
    web::WebState* web_state) {
  web_state->RemoveObserver(this);
}
