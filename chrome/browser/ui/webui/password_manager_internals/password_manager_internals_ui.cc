// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager_internals/password_manager_internals_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/grit/components_resources.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/escape.h"

using autofill::LogRouter;
using password_manager::PasswordManagerLogRouterFactory;

namespace {

content::WebUIDataSource* CreatePasswordManagerInternalsHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIPasswordManagerInternalsHost);
  source->AddResourcePath("autofill_and_password_manager_internals.js",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_JS);
  source->AddResourcePath("autofill_and_password_manager_internals.css",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_HTML);
  return source;
}

// chrome://password-manager-internals specific UI handler that takes care of
// subscribing to the autofill logging instance.
class PasswordManagerInternalsUIHandler : public content::WebUIMessageHandler,
                                          public autofill::LogReceiver {
 public:
  PasswordManagerInternalsUIHandler() = default;
  ~PasswordManagerInternalsUIHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Implements content::WebUIMessageHandler.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // LogReceiver implementation.
  void LogEntry(const base::Value& entry) override;

  void StartSubscription();
  void EndSubscription();

  // JavaScript call handler.
  void OnLoaded(const base::ListValue* args);

  // Whether |this| is registered as a log receiver with the LogRouter.
  bool registered_with_log_router_ = false;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerInternalsUIHandler);
};

PasswordManagerInternalsUIHandler::~PasswordManagerInternalsUIHandler() {
  EndSubscription();
}

void PasswordManagerInternalsUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "loaded",
      base::BindRepeating(&PasswordManagerInternalsUIHandler::OnLoaded,
                          base::Unretained(this)));
}

void PasswordManagerInternalsUIHandler::OnJavascriptAllowed() {
  StartSubscription();
}

void PasswordManagerInternalsUIHandler::OnJavascriptDisallowed() {
  EndSubscription();
}

void PasswordManagerInternalsUIHandler::OnLoaded(const base::ListValue* args) {
  AllowJavascript();
  CallJavascriptFunction("setUpPasswordManagerInternals");
  CallJavascriptFunction(
      "notifyAboutIncognito",
      base::Value(Profile::FromWebUI(web_ui())->IsIncognitoProfile()));
}

void PasswordManagerInternalsUIHandler::StartSubscription() {
  LogRouter* log_router = PasswordManagerLogRouterFactory::GetForBrowserContext(
      Profile::FromWebUI(web_ui()));
  if (!log_router)
    return;

  registered_with_log_router_ = true;

  const auto& past_logs = log_router->RegisterReceiver(this);
  for (const auto& entry : past_logs)
    LogEntry(entry);
}

void PasswordManagerInternalsUIHandler::EndSubscription() {
  if (!registered_with_log_router_)
    return;
  registered_with_log_router_ = false;
  LogRouter* log_router = PasswordManagerLogRouterFactory::GetForBrowserContext(
      Profile::FromWebUI(web_ui()));
  if (log_router)
    log_router->UnregisterReceiver(this);
}

void PasswordManagerInternalsUIHandler::LogEntry(const base::Value& entry) {
  if (!registered_with_log_router_ || entry.is_none())
    return;
  CallJavascriptFunction("addRawLog", entry);
}

}  // namespace

PasswordManagerInternalsUI::PasswordManagerInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                CreatePasswordManagerInternalsHTMLSource());
  web_ui->AddMessageHandler(
      std::make_unique<PasswordManagerInternalsUIHandler>());
}

PasswordManagerInternalsUI::~PasswordManagerInternalsUI() = default;
