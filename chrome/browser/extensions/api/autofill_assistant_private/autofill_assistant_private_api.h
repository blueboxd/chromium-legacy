// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_ASSISTANT_PRIVATE_AUTOFILL_ASSISTANT_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_ASSISTANT_PRIVATE_AUTOFILL_ASSISTANT_PRIVATE_API_H_

#include <string>

#include "chrome/common/extensions/api/autofill_assistant_private.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/controller_observer.h"
#include "components/autofill_assistant/browser/service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class AutofillAssistantPrivateCreateFunction : public ExtensionFunction {
 public:
  AutofillAssistantPrivateCreateFunction();
  DECLARE_EXTENSION_FUNCTION("autofillAssistantPrivate.create",
                             AUTOFILLASSISTANTPRIVATE_CREATE)

  AutofillAssistantPrivateCreateFunction(
      const AutofillAssistantPrivateCreateFunction&) = delete;
  AutofillAssistantPrivateCreateFunction& operator=(
      const AutofillAssistantPrivateCreateFunction&) = delete;

 protected:
  ~AutofillAssistantPrivateCreateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class AutofillAssistantPrivateStartFunction : public ExtensionFunction {
 public:
  AutofillAssistantPrivateStartFunction();
  DECLARE_EXTENSION_FUNCTION("autofillAssistantPrivate.start",
                             AUTOFILLASSISTANTPRIVATE_START)

  AutofillAssistantPrivateStartFunction(
      const AutofillAssistantPrivateStartFunction&) = delete;
  AutofillAssistantPrivateStartFunction& operator=(
      const AutofillAssistantPrivateStartFunction&) = delete;

 protected:
  ~AutofillAssistantPrivateStartFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class AutofillAssistantPrivateGetStateFunction : public ExtensionFunction {
 public:
  AutofillAssistantPrivateGetStateFunction();
  DECLARE_EXTENSION_FUNCTION("autofillAssistantPrivate.getState",
                             AUTOFILLASSISTANTPRIVATE_GETSTATE)

  AutofillAssistantPrivateGetStateFunction(
      const AutofillAssistantPrivateGetStateFunction&) = delete;
  AutofillAssistantPrivateGetStateFunction& operator=(
      const AutofillAssistantPrivateGetStateFunction&) = delete;

 protected:
  ~AutofillAssistantPrivateGetStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class AutofillAssistantPrivatePerformActionFunction : public ExtensionFunction {
 public:
  AutofillAssistantPrivatePerformActionFunction();
  DECLARE_EXTENSION_FUNCTION("autofillAssistantPrivate.performAction",
                             AUTOFILLASSISTANTPRIVATE_PERFORMACTION)

  AutofillAssistantPrivatePerformActionFunction(
      const AutofillAssistantPrivatePerformActionFunction&) = delete;
  AutofillAssistantPrivatePerformActionFunction& operator=(
      const AutofillAssistantPrivatePerformActionFunction&) = delete;

 protected:
  ~AutofillAssistantPrivatePerformActionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class AutofillAssistantPrivateProvideUserDataFunction
    : public ExtensionFunction {
 public:
  AutofillAssistantPrivateProvideUserDataFunction();
  DECLARE_EXTENSION_FUNCTION("autofillAssistantPrivate.provideUserData",
                             AUTOFILLASSISTANTPRIVATE_PROVIDEUSERDATA)

  AutofillAssistantPrivateProvideUserDataFunction(
      const AutofillAssistantPrivateProvideUserDataFunction&) = delete;
  AutofillAssistantPrivateProvideUserDataFunction& operator=(
      const AutofillAssistantPrivateProvideUserDataFunction&) = delete;

 protected:
  ~AutofillAssistantPrivateProvideUserDataFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class AutofillAssistantPrivateEventRouter
    : public autofill_assistant::ControllerObserver {
 public:
  AutofillAssistantPrivateEventRouter(
      autofill_assistant::Controller* controller,
      content::BrowserContext* browser_context);
  ~AutofillAssistantPrivateEventRouter() override;

  AutofillAssistantPrivateEventRouter(
      const AutofillAssistantPrivateEventRouter&) = delete;
  AutofillAssistantPrivateEventRouter& operator=(
      const AutofillAssistantPrivateEventRouter&) = delete;

  // autofill_assistant::ControllerObserver:
  void OnUserActionsChanged(
      const std::vector<autofill_assistant::UserAction>& user_actions) override;
  void OnStatusMessageChanged(const std::string& message) override;
  // TODO(crbug.com/1015753): Expose more notifications as needed here.
  void OnStateChanged(
      autofill_assistant::AutofillAssistantState new_state) override;
  void OnBubbleMessageChanged(const std::string& message) override;
  void CloseCustomTab() override;
  void OnCollectUserDataOptionsChanged(
      const autofill_assistant::CollectUserDataOptions* options) override;
  void OnUserDataChanged(
      const autofill_assistant::UserData* state,
      autofill_assistant::UserData::FieldChange field_change) override;
  void OnDetailsChanged(const autofill_assistant::Details* details) override;
  void OnInfoBoxChanged(const autofill_assistant::InfoBox* info_box) override;
  void OnProgressChanged(int progress) override;
  void OnProgressVisibilityChanged(bool visible) override;
  void OnTouchableAreaChanged(
      const autofill_assistant::RectF& visual_viewport,
      const std::vector<autofill_assistant::RectF>& touchable_areas,
      const std::vector<autofill_assistant::RectF>& restricted_areas) override;
  void OnViewportModeChanged(autofill_assistant::ViewportMode mode) override;
  void OnPeekModeChanged(autofill_assistant::ConfigureBottomSheetProto::PeekMode
                             peek_mode) override;
  void OnOverlayColorsChanged(
      const autofill_assistant::UiDelegate::OverlayColors& colors) override;
  void OnFormChanged(
      const autofill_assistant::FormProto* form,
      const autofill_assistant::FormProto::Result* result) override;
  void OnClientSettingsChanged(
      const autofill_assistant::ClientSettings& settings) override;
  void OnGenericUserInterfaceChanged(
      const autofill_assistant::GenericUserInterfaceProto* generic_ui) override;

  void OnExpandBottomSheet() override;
  void OnCollapseBottomSheet() override;

 private:
  bool HasEventListenerForEvent(const char* event_name);

  // |controller_|'s lifetime and this object are managed by the
  // AutofillAssistantPrivateAPI, and |controller_| is guaranteed to be alive
  // while this object is around.
  autofill_assistant::Controller* controller_;
  content::BrowserContext* browser_context_;
};

class AutofillAssistantPrivateAPI : public BrowserContextKeyedAPI,
                                    public autofill_assistant::Client {
 public:
  static BrowserContextKeyedAPIFactory<AutofillAssistantPrivateAPI>*
  GetFactoryInstance();

  explicit AutofillAssistantPrivateAPI(content::BrowserContext* context);
  ~AutofillAssistantPrivateAPI() override;

  AutofillAssistantPrivateAPI(const AutofillAssistantPrivateAPI&) = delete;
  AutofillAssistantPrivateAPI& operator=(const AutofillAssistantPrivateAPI&) =
      delete;

  void CreateAutofillAssistantController(content::WebContents* web_contents);
  bool StartAutofillAssistantController(
      std::map<std::string, std::string> params);
  // Relay a selected action to the autofill assistant controller.
  bool PerformAction(int index);
  // Provides user data to the controller.
  // TODO(crbug.com/1015753): Flesh this out to let the extension pass in user
  // data.
  bool ProvideUserData();

  // autofill_assistant::Client:
  void AttachUI() override;
  void DestroyUI() override;
  std::string GetApiKey() const override;
  std::string GetAccountEmailAddress() const override;
  autofill_assistant::AccessTokenFetcher* GetAccessTokenFetcher() override;
  autofill::PersonalDataManager* GetPersonalDataManager() const override;
  password_manager::PasswordManagerClient* GetPasswordManagerClient()
      const override;
  autofill_assistant::WebsiteLoginFetcher* GetWebsiteLoginFetcher()
      const override;
  std::string GetServerUrl() const override;
  std::string GetLocale() const override;
  std::string GetCountryCode() const override;
  autofill_assistant::DeviceContext GetDeviceContext() const override;
  void Shutdown(autofill_assistant::Metrics::DropOutReason reason) override;
  content::WebContents* GetWebContents() const override;

  // BrowserContextKeyedAPI:
  void Shutdown() override;

  // Used to inject a mock backend service in tests.
  void SetService(std::unique_ptr<autofill_assistant::Service> service);

 private:
  friend class BrowserContextKeyedAPIFactory<AutofillAssistantPrivateAPI>;

  // BrowserContextKeyedAPI:
  static const char* service_name() { return "AutofillAssistantPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;

  void CreateActiveAutofillAssistant(content::WebContents* web_contents);
  void DestroyActiveAutofillAssistant();

  content::BrowserContext* browser_context_;
  std::unique_ptr<autofill_assistant::Service> service_;

  // This struct ties the lifecycle of the event router and the controller
  // together. That way event are only dispatched when both are alive.
  struct ActiveAutofillAssistant {
    ActiveAutofillAssistant();
    ~ActiveAutofillAssistant();

    std::unique_ptr<autofill_assistant::Controller> controller;
    std::unique_ptr<AutofillAssistantPrivateEventRouter> event_router;
  };
  std::unique_ptr<ActiveAutofillAssistant> active_autofill_assistant_;
  std::unique_ptr<autofill_assistant::AccessTokenFetcher> access_token_fetcher_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_ASSISTANT_PRIVATE_AUTOFILL_ASSISTANT_PRIVATE_API_H_
