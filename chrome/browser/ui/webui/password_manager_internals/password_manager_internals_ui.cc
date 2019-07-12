// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager_internals/password_manager_internals_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/grit/components_resources.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/escape.h"

using password_manager::PasswordManagerInternalsService;
using password_manager::PasswordManagerInternalsServiceFactory;

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
                                          public password_manager::LogReceiver {
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
  void LogSavePasswordProgress(const std::string& text) override;

  void StartSubscription();
  void EndSubscription();

  // JavaScript call handler.
  void OnLoaded(const base::ListValue* args);

  // Whether |this| is registered as a log receiver with the
  // PasswordManagerInternalsService.
  bool registered_with_logging_service_ = false;

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
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  if (!service)
    return;

  registered_with_logging_service_ = true;

  std::string past_logs(service->RegisterReceiver(this));
  LogSavePasswordProgress(past_logs);
}

void PasswordManagerInternalsUIHandler::EndSubscription() {
  if (!registered_with_logging_service_)
    return;
  registered_with_logging_service_ = false;
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  if (service)
    service->UnregisterReceiver(this);
}

void PasswordManagerInternalsUIHandler::LogSavePasswordProgress(
    const std::string& text) {
  if (!registered_with_logging_service_ || text.empty())
    return;
  std::string no_quotes(text);
  std::replace(no_quotes.begin(), no_quotes.end(), '"', ' ');
  base::Value text_string_value(net::EscapeForHTML(no_quotes));
  CallJavascriptFunction("addLog", text_string_value);
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
