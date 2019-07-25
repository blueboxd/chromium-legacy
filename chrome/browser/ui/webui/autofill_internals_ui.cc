// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/autofill/content/browser/autofill_log_router_factory.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

using autofill::AutofillLogRouterFactory;
using autofill::LogRouter;

namespace {

content::WebUIDataSource* CreateAutofillInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAutofillInternalsHost);
  source->AddResourcePath("autofill_and_password_manager_internals.js",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_JS);
  source->AddResourcePath("autofill_and_password_manager_internals.css",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_HTML);
  return source;
}

// Message handler for AutofillInternalsLoggingImpl. The purpose of this class
// is to enable safe calls to JavaScript, while the renderer is fully loaded.
class AutofillInternalsUIHandler : public content::WebUIMessageHandler,
                                   public autofill::LogReceiver {
 public:
  AutofillInternalsUIHandler() = default;
  ~AutofillInternalsUIHandler() override;

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

  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsUIHandler);
};

AutofillInternalsUIHandler::~AutofillInternalsUIHandler() {
  EndSubscription();
}

void AutofillInternalsUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "loaded", base::BindRepeating(&AutofillInternalsUIHandler::OnLoaded,
                                    base::Unretained(this)));
}

void AutofillInternalsUIHandler::OnJavascriptAllowed() {
  StartSubscription();
}

void AutofillInternalsUIHandler::OnJavascriptDisallowed() {
  EndSubscription();
}

void AutofillInternalsUIHandler::OnLoaded(const base::ListValue* args) {
  AllowJavascript();
  CallJavascriptFunction("setUpAutofillInternals");
  CallJavascriptFunction(
      "notifyAboutIncognito",
      base::Value(Profile::FromWebUI(web_ui())->IsIncognitoProfile()));
}

void AutofillInternalsUIHandler::LogEntry(const base::Value& entry) {
  if (!registered_with_log_router_ || entry.is_none())
    return;
  CallJavascriptFunction("addRawLog", entry);
}

void AutofillInternalsUIHandler::StartSubscription() {
  LogRouter* log_router = AutofillLogRouterFactory::GetForBrowserContext(
      Profile::FromWebUI(web_ui()));

  if (!log_router)
    return;

  registered_with_log_router_ = true;

  const auto& past_logs = log_router->RegisterReceiver(this);
  for (const auto& entry : past_logs)
    LogEntry(entry);
}

void AutofillInternalsUIHandler::EndSubscription() {
  if (!registered_with_log_router_)
    return;
  registered_with_log_router_ = false;
  LogRouter* log_router = AutofillLogRouterFactory::GetForBrowserContext(
      Profile::FromWebUI(web_ui()));
  if (log_router)
    log_router->UnregisterReceiver(this);
}

}  // namespace

AutofillInternalsUI::AutofillInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateAutofillInternalsHTMLSource());
  web_ui->AddMessageHandler(std::make_unique<AutofillInternalsUIHandler>());
}

AutofillInternalsUI::~AutofillInternalsUI() = default;
