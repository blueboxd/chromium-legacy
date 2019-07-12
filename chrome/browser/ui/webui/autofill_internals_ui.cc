// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/autofill/core/browser/autofill_internals_logging.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

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
class AutofillInternalsUIHandler
    : public content::WebUIMessageHandler,
      public autofill::AutofillInternalsLogging::Observer {
 public:
  AutofillInternalsUIHandler() = default;
  ~AutofillInternalsUIHandler() override = default;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Implements content::WebUIMessageHandler.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Implements autofill::AutofillInternalsLogging::Observer.
  void Log(const base::Value& message) override;
  void LogRaw(const base::Value& message) override;

  // JavaScript call handler.
  void OnLoaded(const base::ListValue* args);

  void StartSubscription();
  void EndSubscription();

  ScopedObserver<autofill::AutofillInternalsLogging,
                 autofill::AutofillInternalsLogging::Observer>
      observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsUIHandler);
};

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

void AutofillInternalsUIHandler::Log(const base::Value& message) {
  CallJavascriptFunction("addLog", message);
}

void AutofillInternalsUIHandler::LogRaw(const base::Value& message) {
  CallJavascriptFunction("addRawLog", message);
}

void AutofillInternalsUIHandler::StartSubscription() {
  observer_.Add(autofill::AutofillInternalsLogging::GetInstance());
}

void AutofillInternalsUIHandler::EndSubscription() {
  observer_.RemoveAll();
}

}  // namespace

AutofillInternalsUI::AutofillInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateAutofillInternalsHTMLSource());
  web_ui->AddMessageHandler(std::make_unique<AutofillInternalsUIHandler>());
}

AutofillInternalsUI::~AutofillInternalsUI() = default;
