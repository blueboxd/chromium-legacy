// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_assistant_private/autofill_assistant_private_api.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/extensions/api/autofill_assistant_private/extension_access_token_fetcher.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/autofill/core/browser/field_types.h"
#include "extensions/browser/event_router_factory.h"

namespace extensions {
namespace {
content::WebContents* GetActiveWebContents(ExtensionFunction* function) {
  // TODO(crbug.com/1015753): GetCurrentBrowser is not deterministic and needs
  // to be replaced with an explicit tabID passed to the create(...) extension
  // function.
  return ChromeExtensionFunctionDetails(function)
      .GetCurrentBrowser()
      ->tab_strip_model()
      ->GetActiveWebContents();
}
}  // namespace

AutofillAssistantPrivateCreateFunction::
    AutofillAssistantPrivateCreateFunction() = default;

AutofillAssistantPrivateCreateFunction::
    ~AutofillAssistantPrivateCreateFunction() = default;

ExtensionFunction::ResponseAction
AutofillAssistantPrivateCreateFunction::Run() {
  auto* web_contents = GetActiveWebContents(this);
  CHECK(web_contents);
  AutofillAssistantPrivateAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->CreateAutofillAssistantController(web_contents);

  // TODO(crbug.com/1015753): Add unittests and check that no arguments are
  // returned!
  return RespondNow(NoArguments());
}

AutofillAssistantPrivateStartFunction::AutofillAssistantPrivateStartFunction() =
    default;

AutofillAssistantPrivateStartFunction::
    ~AutofillAssistantPrivateStartFunction() = default;

ExtensionFunction::ResponseAction AutofillAssistantPrivateStartFunction::Run() {
  std::unique_ptr<api::autofill_assistant_private::Start::Params> params =
      api::autofill_assistant_private::Start::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(crbug.com/1015753): Populate parameters from params.
  std::map<std::string, std::string> parameters;

  bool ok = AutofillAssistantPrivateAPI::GetFactoryInstance()
                ->Get(browser_context())
                ->StartAutofillAssistantController(std::move(parameters));

  if (!ok) {
    return RespondNow(Error("Starting requires a valid controller!"));
  }
  return RespondNow(NoArguments());
}

AutofillAssistantPrivatePerformActionFunction::
    AutofillAssistantPrivatePerformActionFunction() = default;

AutofillAssistantPrivatePerformActionFunction::
    ~AutofillAssistantPrivatePerformActionFunction() = default;

ExtensionFunction::ResponseAction
AutofillAssistantPrivatePerformActionFunction::Run() {
  std::unique_ptr<api::autofill_assistant_private::PerformAction::Params>
      parameters =
          api::autofill_assistant_private::PerformAction::Params::Create(
              *args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  bool ok = AutofillAssistantPrivateAPI::GetFactoryInstance()
                ->Get(browser_context())
                ->PerformAction(parameters->index);
  if (!ok) {
    return RespondNow(Error("performAction requires a valid controller!"));
  }
  return RespondNow(NoArguments());
}

AutofillAssistantPrivateProvideUserDataFunction::
    AutofillAssistantPrivateProvideUserDataFunction() = default;

AutofillAssistantPrivateProvideUserDataFunction::
    ~AutofillAssistantPrivateProvideUserDataFunction() = default;

ExtensionFunction::ResponseAction
AutofillAssistantPrivateProvideUserDataFunction::Run() {
  AutofillAssistantPrivateAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->ProvideUserData();

  return RespondNow(NoArguments());
}

AutofillAssistantPrivateEventRouter::AutofillAssistantPrivateEventRouter(
    autofill_assistant::Controller* controller,
    content::BrowserContext* browser_context)
    : controller_(controller), browser_context_(browser_context) {
  DCHECK_NE(nullptr, controller);
  controller_->AddObserver(this);
}

AutofillAssistantPrivateEventRouter::~AutofillAssistantPrivateEventRouter() {}

void AutofillAssistantPrivateEventRouter::OnUserActionsChanged(
    const std::vector<autofill_assistant::UserAction>& user_actions) {
  if (!HasEventListenerForEvent(
          api::autofill_assistant_private::OnActionsChanged::kEventName))
    return;

  std::vector<api::autofill_assistant_private::ActionObject> actions;
  actions.reserve(user_actions.size());
  for (size_t i = 0; i < user_actions.size(); ++i) {
    actions.emplace_back();
    actions.back().name = user_actions[i].chip().text;
    actions.back().index = i;
  }

  std::unique_ptr<base::ListValue> args(
      api::autofill_assistant_private::OnActionsChanged::Create(actions));

  std::unique_ptr<Event> extension_event = std::make_unique<Event>(
      events::AUTOFILL_ASSISTANT_PRIVATE_ON_ACTIONS_CHANGED,
      api::autofill_assistant_private::OnActionsChanged::kEventName,
      std::move(args));
  EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::move(extension_event));
}

void AutofillAssistantPrivateEventRouter::OnStatusMessageChanged(
    const std::string& message) {
  if (!HasEventListenerForEvent(
          api::autofill_assistant_private::OnStatusMessageChanged::kEventName))
    return;

  std::unique_ptr<base::ListValue> args(
      api::autofill_assistant_private::OnStatusMessageChanged::Create(message));

  std::unique_ptr<Event> extension_event(new Event(
      events::AUTOFILL_ASSISTANT_PRIVATE_ON_STATUS_MESSAGE_CHANGED,
      api::autofill_assistant_private::OnStatusMessageChanged::kEventName,
      std::move(args)));
  EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::move(extension_event));
}

void AutofillAssistantPrivateEventRouter::OnStateChanged(
    autofill_assistant::AutofillAssistantState new_state) {}

void AutofillAssistantPrivateEventRouter::OnBubbleMessageChanged(
    const std::string& message) {}

void AutofillAssistantPrivateEventRouter::CloseCustomTab() {}

void AutofillAssistantPrivateEventRouter::OnCollectUserDataOptionsChanged(
    const autofill_assistant::CollectUserDataOptions* options) {}

void AutofillAssistantPrivateEventRouter::OnUserDataChanged(
    const autofill_assistant::UserData* state,
    autofill_assistant::UserData::FieldChange field_change) {}

void AutofillAssistantPrivateEventRouter::OnDetailsChanged(
    const autofill_assistant::Details* details) {}

void AutofillAssistantPrivateEventRouter::OnInfoBoxChanged(
    const autofill_assistant::InfoBox* info_box) {}

void AutofillAssistantPrivateEventRouter::OnProgressChanged(int progress) {}

void AutofillAssistantPrivateEventRouter::OnProgressVisibilityChanged(
    bool visible) {}

void AutofillAssistantPrivateEventRouter::OnTouchableAreaChanged(
    const autofill_assistant::RectF& visual_viewport,
    const std::vector<autofill_assistant::RectF>& touchable_areas,
    const std::vector<autofill_assistant::RectF>& restricted_areas) {}

void AutofillAssistantPrivateEventRouter::OnViewportModeChanged(
    autofill_assistant::ViewportMode mode) {}

void AutofillAssistantPrivateEventRouter::OnPeekModeChanged(
    autofill_assistant::ConfigureBottomSheetProto::PeekMode peek_mode) {}

void AutofillAssistantPrivateEventRouter::OnOverlayColorsChanged(
    const autofill_assistant::UiDelegate::OverlayColors& colors) {}

void AutofillAssistantPrivateEventRouter::OnFormChanged(
    const autofill_assistant::FormProto* form,
    const autofill_assistant::FormProto::Result* result) {}

void AutofillAssistantPrivateEventRouter::OnClientSettingsChanged(
    const autofill_assistant::ClientSettings& settings) {}

void AutofillAssistantPrivateEventRouter::OnGenericUserInterfaceChanged(
    const autofill_assistant::GenericUserInterfaceProto* generic_ui) {}

void AutofillAssistantPrivateEventRouter::OnExpandBottomSheet() {}

void AutofillAssistantPrivateEventRouter::OnCollapseBottomSheet() {}

bool AutofillAssistantPrivateEventRouter::HasEventListenerForEvent(
    const char* event_name) {
  return EventRouter::Get(browser_context_)->HasEventListener(event_name);
}

// static
BrowserContextKeyedAPIFactory<AutofillAssistantPrivateAPI>*
AutofillAssistantPrivateAPI::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<AutofillAssistantPrivateAPI>>
      instance;
  return instance.get();
}

template <>
void BrowserContextKeyedAPIFactory<
    AutofillAssistantPrivateAPI>::DeclareFactoryDependencies() {
  DependsOn(EventRouterFactory::GetInstance());
}

AutofillAssistantPrivateAPI::AutofillAssistantPrivateAPI(
    content::BrowserContext* context)
    : browser_context_(context) {
  access_token_fetcher_ = std::make_unique<ExtensionAccessTokenFetcher>(
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_)));
}

AutofillAssistantPrivateAPI::~AutofillAssistantPrivateAPI() {
  access_token_fetcher_.reset();
}

void AutofillAssistantPrivateAPI::CreateAutofillAssistantController(
    content::WebContents* web_contents) {
  if (active_autofill_assistant_) {
    DestroyActiveAutofillAssistant();
  }

  CreateActiveAutofillAssistant(web_contents);
}

bool AutofillAssistantPrivateAPI::StartAutofillAssistantController(
    std::map<std::string, std::string> parameters) {
  if (!active_autofill_assistant_)
    return false;

  // TODO(crbug.com/1015753): Remove the hard coded parameter and use |params|
  // instead. The url usually comes from an Android intent data URL and is
  // logged as the trigger deep link. This does not fully translate to the
  // extension use case and needs more thought. For now we pass in a dummy URL.
  parameters["DETAILS_SHOW_INITIAL"] = "false";
  const std::string url = "http://localhost:8080/test.html";

  auto context = autofill_assistant::TriggerContext::Create(parameters, "");
  active_autofill_assistant_->controller->Start(GURL(url), std::move(context));
  return true;
}

bool AutofillAssistantPrivateAPI::PerformAction(int index) {
  if (!active_autofill_assistant_)
    return false;
  active_autofill_assistant_->controller->PerformUserAction(index);
  return true;
}

bool AutofillAssistantPrivateAPI::ProvideUserData() {
  if (!active_autofill_assistant_)
    return false;

  auto* controller = active_autofill_assistant_->controller.get();

  // TODO(crbug.com/1015753): Plumb the hard coded user data to the idl
  // interface definition and expose it to the extension.
  // Note that the set of empty profiles provided here is currently not
  // sufficient for a working e2e flow.
  auto shipping = std::make_unique<autofill::AutofillProfile>();
  // TBD: set some shipping fields.
  controller->SetShippingAddress(std::move(shipping));
  auto contact = std::make_unique<autofill::AutofillProfile>();
  controller->SetContactInfo(std::move(contact));

  auto billing = std::make_unique<autofill::AutofillProfile>();
  auto card = std::make_unique<autofill::CreditCard>();
  card->set_billing_address_id(billing->guid());

  controller->SetCreditCard(std::move(card), std::move(billing));
  controller->SetTermsAndConditions(
      autofill_assistant::TermsAndConditionsState::ACCEPTED);
  return true;
}

void AutofillAssistantPrivateAPI::AttachUI() {}

void AutofillAssistantPrivateAPI::DestroyUI() {}

std::string AutofillAssistantPrivateAPI::GetApiKey() const {
  // TODO(crbug.com/1015753): Use chromium's keys and also consider the
  // autofill-assistant-key here to override that particular key.
  return "invalid";
}

std::string AutofillAssistantPrivateAPI::GetAccountEmailAddress() const {
  return "joe@example.com";
}

autofill_assistant::AccessTokenFetcher*
AutofillAssistantPrivateAPI::GetAccessTokenFetcher() {
  return access_token_fetcher_.get();
}

autofill::PersonalDataManager*
AutofillAssistantPrivateAPI::GetPersonalDataManager() const {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
}

password_manager::PasswordManagerClient*
AutofillAssistantPrivateAPI::GetPasswordManagerClient() const {
  // TODO(crbug.com/1015753): Support credential leak flows.
  return nullptr;
}

autofill_assistant::WebsiteLoginFetcher*
AutofillAssistantPrivateAPI::GetWebsiteLoginFetcher() const {
  return nullptr;
}

std::string AutofillAssistantPrivateAPI::GetServerUrl() const {
  // TODO(crbug.com/1015753): Consider the autofill-assistant-url for endpoint
  // overrides and share the kDefaultAutofillAssistantServerUrl to expose it
  // here.
  return "https://automate-pa.googleapis.com";
}

std::string AutofillAssistantPrivateAPI::GetLocale() const {
  return "en-us";
}

std::string AutofillAssistantPrivateAPI::GetCountryCode() const {
  return "us";
}

autofill_assistant::DeviceContext
AutofillAssistantPrivateAPI::GetDeviceContext() const {
  return autofill_assistant::DeviceContext();
}

void AutofillAssistantPrivateAPI::Shutdown(
    autofill_assistant::Metrics::DropOutReason reason) {}

// Note that this method implements autofill_assistant::Client and simply
// forwards the web_contents associated with the controller. There is no reason
// to use this method in this context.
// TODO(crbug.com/1015753): Revisit the interfaces used for this extension API
// and introduce new, more concise ones if needed.
content::WebContents* AutofillAssistantPrivateAPI::GetWebContents() const {
  if (!active_autofill_assistant_)
    return nullptr;
  return active_autofill_assistant_->controller->GetWebContents();
}

void AutofillAssistantPrivateAPI::Shutdown() {
  if (active_autofill_assistant_)
    DestroyActiveAutofillAssistant();
}

AutofillAssistantPrivateAPI::ActiveAutofillAssistant::
    ActiveAutofillAssistant() {}
AutofillAssistantPrivateAPI::ActiveAutofillAssistant::
    ~ActiveAutofillAssistant() {}

void AutofillAssistantPrivateAPI::CreateActiveAutofillAssistant(
    content::WebContents* web_contents) {
  active_autofill_assistant_ = std::make_unique<ActiveAutofillAssistant>();
  // TODO(crbug.com/1015753): Make sure web_contents going away is properly
  // handled by the controller. And if so, clean up state here.
  active_autofill_assistant_->controller =
      std::make_unique<autofill_assistant::Controller>(
          web_contents, this, base::DefaultTickClock::GetInstance(),
          std::move(service_));
  active_autofill_assistant_->event_router =
      std::make_unique<AutofillAssistantPrivateEventRouter>(
          active_autofill_assistant_->controller.get(), browser_context_);
}

void AutofillAssistantPrivateAPI::DestroyActiveAutofillAssistant() {
  // The event router has a reference to controller, so that is being cleaned up
  // first.
  active_autofill_assistant_->event_router.reset();
  active_autofill_assistant_->controller.reset();
  active_autofill_assistant_.reset();
}

void AutofillAssistantPrivateAPI::SetService(
    std::unique_ptr<autofill_assistant::Service> service) {
  DCHECK_EQ(nullptr, service_);
  service_ = std::move(service);
}

}  // namespace extensions
