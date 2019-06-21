// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill_assistant {

namespace {

// The initial progress to set when autostarting and showing the "Loading..."
// message.
static constexpr int kAutostartInitialProgress = 5;

// Cookie experiment name.
// TODO(crbug.com/806868): Introduce a dedicated experiment extra parameter to
// pass allow passing more than one experiment.
static const char* const kCookieExperimentName = "EXP_COOKIE";
// Website visited before parameter.
// Note: This parameter goes with the previous experiment name. I.e. it is only
// set when the cookie experiment is active.
static const char* const kWebsiteVisitedBeforeParameterName =
    "WEBSITE_VISITED_BEFORE";
// Parameter that allows setting the color of the overlay.
static const char* const kOverlayColorParameterName = "OVERLAY_COLORS";

static const char* const kTrueValue = "true";
}  // namespace

Controller::Controller(content::WebContents* web_contents,
                       Client* client,
                       const base::TickClock* tick_clock)
    : content::WebContentsObserver(web_contents),
      client_(client),
      tick_clock_(tick_clock),
      navigating_to_new_document_(web_contents->IsWaitingForResponse()),
      weak_ptr_factory_(this) {}

Controller::~Controller() = default;

const ClientSettings& Controller::GetSettings() {
  return settings_;
}

const GURL& Controller::GetCurrentURL() {
  const GURL& last_committed = web_contents()->GetLastCommittedURL();
  if (!last_committed.is_empty())
    return last_committed;

  return deeplink_url_;
}

const GURL& Controller::GetDeeplinkURL() {
  return deeplink_url_;
}

Service* Controller::GetService() {
  if (!service_) {
    service_ = Service::Create(web_contents()->GetBrowserContext(), client_);
  }
  return service_.get();
}

UiController* Controller::GetUiController() {
  if (will_shutdown_) {
    // Never call the UI controller after having announced a shutdown, no matter
    // what happens; this is part of the contract of UIDelegate.
    //
    // TODO(crbug/925947): Once UIController has become observer, clear list of
    // observers instead.
    if (!noop_ui_controller_) {
      noop_ui_controller_ = std::make_unique<UiController>();
    }
    return noop_ui_controller_.get();
  }
  return client_->GetUiController();
}

WebController* Controller::GetWebController() {
  if (!web_controller_) {
    web_controller_ =
        WebController::CreateForWebContents(web_contents(), &settings_);
  }
  return web_controller_.get();
}

ClientMemory* Controller::GetClientMemory() {
  if (!memory_) {
    memory_ = std::make_unique<ClientMemory>();
  }
  return memory_.get();
}

const TriggerContext* Controller::GetTriggerContext() {
  DCHECK(trigger_context_);
  return trigger_context_.get();
}

autofill::PersonalDataManager* Controller::GetPersonalDataManager() {
  return client_->GetPersonalDataManager();
}

content::WebContents* Controller::GetWebContents() {
  return web_contents();
}

void Controller::SetTouchableElementArea(const ElementAreaProto& area) {
  touchable_element_area()->SetFromProto(area);
}

void Controller::SetStatusMessage(const std::string& message) {
  status_message_ = message;
  GetUiController()->OnStatusMessageChanged(message);
}

std::string Controller::GetStatusMessage() const {
  return status_message_;
}

void Controller::SetDetails(std::unique_ptr<Details> details) {
  details_ = std::move(details);
  GetUiController()->OnDetailsChanged(details_.get());
}

const Details* Controller::GetDetails() const {
  return details_.get();
}

int Controller::GetProgress() const {
  return progress_;
}

void Controller::SetInfoBox(const InfoBox& info_box) {
  if (!info_box_) {
    info_box_ = std::make_unique<InfoBox>();
  }
  *info_box_ = info_box;
  GetUiController()->OnInfoBoxChanged(info_box_.get());
}

void Controller::ClearInfoBox() {
  info_box_.reset();
  GetUiController()->OnInfoBoxChanged(nullptr);
}

const InfoBox* Controller::GetInfoBox() const {
  return info_box_.get();
}

void Controller::SetProgress(int progress) {
  // Progress can only increase.
  if (progress_ >= progress)
    return;

  progress_ = progress;
  GetUiController()->OnProgressChanged(progress);
}

void Controller::SetProgressVisible(bool visible) {
  if (progress_visible_ == visible)
    return;

  progress_visible_ = visible;
  GetUiController()->OnProgressVisibilityChanged(visible);
}

bool Controller::GetProgressVisible() const {
  return progress_visible_;
}

const std::vector<Chip>& Controller::GetSuggestions() const {
  static const base::NoDestructor<std::vector<Chip>> no_suggestions_;
  return suggestions_ ? *suggestions_ : *no_suggestions_;
}

const std::vector<Chip>& Controller::GetActions() const {
  static const base::NoDestructor<std::vector<Chip>> no_actions_;
  return actions_ ? *actions_ : *no_actions_;
}

void Controller::SetChips(std::unique_ptr<std::vector<Chip>> chips) {
  // We split the chips into suggestions and actions, that are displayed in
  // different carousels.
  actions_.reset();
  suggestions_.reset();

  if (chips && !chips->empty()) {
    for (auto iter = chips->begin(); iter != chips->end(); iter++) {
      if (iter->type == SUGGESTION) {
        if (!suggestions_) {
          suggestions_ = std::make_unique<std::vector<Chip>>();
        }

        suggestions_->emplace_back(std::move(*iter));
      } else {
        if (!actions_) {
          actions_ = std::make_unique<std::vector<Chip>>();
        }

        actions_->emplace_back(std::move(*iter));
      }
    }
  }

  GetUiController()->OnSuggestionsChanged(GetSuggestions());
  GetUiController()->OnActionsChanged(GetActions());
}

bool Controller::IsNavigatingToNewDocument() {
  return navigating_to_new_document_;
}

bool Controller::HasNavigationError() {
  return navigation_error_;
}

void Controller::AddListener(ScriptExecutorDelegate::Listener* listener) {
  auto found = std::find(listeners_.begin(), listeners_.end(), listener);
  if (found == listeners_.end())
    listeners_.emplace_back(listener);
}

void Controller::RemoveListener(ScriptExecutorDelegate::Listener* listener) {
  auto found = std::find(listeners_.begin(), listeners_.end(), listener);
  if (found != listeners_.end())
    listeners_.erase(found);
}

void Controller::SelectSuggestion(int index) {
  SelectChip(suggestions_.get(), index);
}

void Controller::SelectAction(int index) {
  SelectChip(actions_.get(), index);
}

void Controller::SelectChip(std::vector<Chip>* chips, int chip_index) {
  if (!chips || chip_index < 0 ||
      static_cast<size_t>(chip_index) >= chips->size()) {
    NOTREACHED() << "Invalid chip index: " << chip_index;
    return;
  }

  if (!(*chips)[chip_index].callback) {
    return;
  }

  auto callback = std::move((*chips)[chip_index].callback);
  SetChips(nullptr);
  std::move(callback).Run();
}

void Controller::SetResizeViewport(bool resize_viewport) {
  if (resize_viewport == resize_viewport_)
    return;

  resize_viewport_ = resize_viewport;
  GetUiController()->OnResizeViewportChanged(resize_viewport);
}

void Controller::SetPeekMode(ConfigureBottomSheetProto::PeekMode peek_mode) {
  if (peek_mode == peek_mode_)
    return;

  peek_mode_ = peek_mode;
  GetUiController()->OnPeekModeChanged(peek_mode);
}

const FormProto* Controller::GetForm() const {
  return form_.get();
}

bool Controller::SetForm(
    std::unique_ptr<FormProto> form,
    base::RepeatingCallback<void(const FormProto::Result*)> callback) {
  form_.reset();
  form_result_.reset();
  form_callback_ = base::DoNothing();

  if (!form) {
    GetUiController()->OnFormChanged(nullptr);
    return true;
  }

  // Initialize form result. This will return false if the form is invalid or
  // contains unsupported inputs.
  auto form_result = std::make_unique<FormProto::Result>();
  for (FormInputProto& input : *form->mutable_inputs()) {
    FormInputProto::Result* result = form_result->add_input_results();
    switch (input.input_type_case()) {
      case FormInputProto::InputTypeCase::kCounter:
        // Add the initial value of each counter into the form result.
        for (const CounterInputProto::Counter& counter :
             input.counter().counters()) {
          result->mutable_counter()->add_values(counter.initial_value());
        }
        break;
      case FormInputProto::InputTypeCase::kSelection: {
        // Add the initial selected state of each choice into the form result.
        bool has_selected = false;
        for (const SelectionInputProto::Choice& choice :
             input.selection().choices()) {
          if (choice.selected()) {
            if (has_selected && !input.selection().allow_multiple()) {
              // Multiple choices are initially selected even though it is not
              // allowed by the input.
              return false;
            }
            has_selected = true;
          }
          result->mutable_selection()->add_selected(choice.selected());
        }
        break;
      }
      case FormInputProto::InputTypeCase::INPUT_TYPE_NOT_SET:
        DVLOG(1) << "Encountered input with INPUT_TYPE_NOT_SET";
        return false;
        // Intentionally no default case to make compilation fail if a new value
        // was added to the enum but not to this list.
    }
  }

  // Form is valid.
  form_ = std::move(form);
  form_result_ = std::move(form_result);
  form_callback_ = callback;

  // Call the callback with initial result.
  form_callback_.Run(form_result_.get());

  GetUiController()->OnFormChanged(form_.get());
  return true;
}

void Controller::SetCounterValue(int input_index,
                                 int counter_index,
                                 int value) {
  if (!form_result_ || input_index < 0 ||
      input_index >= form_result_->input_results_size()) {
    NOTREACHED() << "Invalid input index: " << input_index;
    return;
  }

  FormInputProto::Result* input_result =
      form_result_->mutable_input_results(input_index);
  if (!input_result->has_counter() || counter_index < 0 ||
      counter_index >= input_result->counter().values_size()) {
    NOTREACHED() << "Invalid counter index: " << counter_index;
    return;
  }

  input_result->mutable_counter()->set_values(counter_index, value);
  form_callback_.Run(form_result_.get());
}

void Controller::SetChoiceSelected(int input_index,
                                   int choice_index,
                                   bool selected) {
  if (!form_result_ || input_index < 0 ||
      input_index >= form_result_->input_results_size()) {
    NOTREACHED() << "Invalid input index: " << input_index;
    return;
  }

  FormInputProto::Result* input_result =
      form_result_->mutable_input_results(input_index);
  if (!input_result->has_selection() || choice_index < 0 ||
      choice_index >= input_result->selection().selected_size()) {
    NOTREACHED() << "Invalid choice index: " << choice_index;
    return;
  }

  input_result->mutable_selection()->set_selected(choice_index, selected);
  form_callback_.Run(form_result_.get());
}

bool Controller::GetResizeViewport() {
  return resize_viewport_;
}

ConfigureBottomSheetProto::PeekMode Controller::GetPeekMode() {
  return peek_mode_;
}

void Controller::SetOverlayColors(std::unique_ptr<OverlayColors> colors) {
  overlay_colors_ = std::move(colors);
  if (overlay_colors_) {
    GetUiController()->OnOverlayColorsChanged(*overlay_colors_);
  } else {
    OverlayColors default_colors;
    GetUiController()->OnOverlayColorsChanged(default_colors);
  }
}

void Controller::GetOverlayColors(OverlayColors* colors) const {
  if (!overlay_colors_)
    return;
  *colors = *overlay_colors_;
}

void Controller::ReportNavigationStateChanged() {
  // Listeners are called in the same order they were added.
  for (auto* listener : listeners_) {
    listener->OnNavigationStateChanged();
  }
}

void Controller::EnterStoppedState() {
  ClearInfoBox();
  SetDetails(nullptr);
  SetChips(nullptr);
  SetPaymentRequestOptions(nullptr);
  EnterState(AutofillAssistantState::STOPPED);
}

void Controller::EnterState(AutofillAssistantState state) {
  if (state_ == state || state_ == AutofillAssistantState::STOPPED)
    return;
  // TODO(b/128300038): Forbid transition out of stopped again instead of
  // ignoring it once shutdown sequence is less complex.

  DVLOG(2) << __func__ << ": " << state_ << " -> " << state;

  state_ = state;
  GetUiController()->OnStateChanged(state);
}

void Controller::SetWebControllerAndServiceForTest(
    std::unique_ptr<WebController> web_controller,
    std::unique_ptr<Service> service) {
  web_controller_ = std::move(web_controller);
  service_ = std::move(service);
}

void Controller::GetOrCheckScripts() {
  if (!started_ || script_tracker()->running()) {
    return;
  }

  const GURL& url = GetCurrentURL();
  if (script_domain_ != url.host()) {
    StopPeriodicScriptChecks();
    script_domain_ = url.host();
    DVLOG(2) << "GetScripts for " << script_domain_;
    GetService()->GetScriptsForUrl(
        url, trigger_context_.get(),
        base::BindOnce(&Controller::OnGetScripts, base::Unretained(this), url));
  } else {
    script_tracker()->CheckScripts();
    StartPeriodicScriptChecks();
  }
}

void Controller::StartPeriodicScriptChecks() {
  periodic_script_check_count_ = settings_.periodic_script_check_count;
  // If periodic checks are running, setting periodic_script_check_count_ keeps
  // them running longer.
  if (periodic_script_check_scheduled_)
    return;
  periodic_script_check_scheduled_ = true;
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&Controller::OnPeriodicScriptCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      settings_.periodic_script_check_interval);
}

void Controller::StopPeriodicScriptChecks() {
  periodic_script_check_count_ = 0;
}

void Controller::OnPeriodicScriptCheck() {
  if (periodic_script_check_count_ > 0) {
    periodic_script_check_count_--;
  }

  if (periodic_script_check_count_ <= 0 && !allow_autostart_) {
    DCHECK_EQ(0, periodic_script_check_count_);
    periodic_script_check_scheduled_ = false;
    return;
  }

  if (allow_autostart_ && !autostart_timeout_script_path_.empty() &&
      tick_clock_->NowTicks() >= absolute_autostart_timeout_) {
    DVLOG(1) << __func__ << " giving up waiting on autostart.";
    std::string script_path = autostart_timeout_script_path_;
    autostart_timeout_script_path_.clear();
    periodic_script_check_scheduled_ = false;
    ExecuteScript(script_path, state_);
    return;
  }

  script_tracker()->CheckScripts();
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&Controller::OnPeriodicScriptCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      settings_.periodic_script_check_interval);
}

void Controller::OnGetScripts(const GURL& url,
                              bool result,
                              const std::string& response) {
  if (state_ == AutofillAssistantState::STOPPED)
    return;

  // If the domain of the current URL changed since the request was sent, the
  // response is not relevant anymore and can be safely discarded.
  if (url.host() != script_domain_)
    return;

  if (!result) {
    DVLOG(1) << "Failed to get assistant scripts for " << script_domain_;
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::GET_SCRIPTS_FAILED);
    return;
  }

  SupportsScriptResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    DVLOG(2) << __func__ << " from " << script_domain_ << " returned "
             << "unparseable response";
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::GET_SCRIPTS_UNPARSABLE);
    return;
  }
  if (response_proto.has_client_settings())
    settings_.UpdateFromProto(response_proto.client_settings());

  std::vector<std::unique_ptr<Script>> scripts;
  for (const auto& script_proto : response_proto.scripts()) {
    ProtocolUtils::AddScript(script_proto, &scripts);
  }
  if (scripts.empty()) {
    OnNoRunnableScripts();
    return;
  }

  if (allow_autostart_) {
    autostart_timeout_script_path_ =
        response_proto.script_timeout_error().script_path();
    autostart_timeout_ = base::TimeDelta::FromMilliseconds(
        response_proto.script_timeout_error().timeout_ms());
    absolute_autostart_timeout_ = tick_clock_->NowTicks() + autostart_timeout_;
  }

  DVLOG(2) << __func__ << " from " << script_domain_ << " returned "
           << scripts.size() << " scripts";
  if (VLOG_IS_ON(3)) {
    for (const auto& script : scripts) {
      // Strip domain from beginning if possible (redundant with log above).
      auto pos = script->handle.path.find(script_domain_);
      if (pos == 0) {
        DVLOG(3) << "\t" << script->handle.path.substr(script_domain_.length());
      } else {
        DVLOG(3) << "\t" << script->handle.path;
      }
    }
  }
  script_tracker()->SetScripts(std::move(scripts));
  script_tracker()->CheckScripts();
  StartPeriodicScriptChecks();
}

void Controller::ExecuteScript(const std::string& script_path,
                               AutofillAssistantState end_state) {
  DCHECK(!script_tracker()->running());
  EnterState(AutofillAssistantState::RUNNING);

  touchable_element_area()->Clear();

  StopPeriodicScriptChecks();
  // Runnable scripts will be checked and reported if necessary after executing
  // the script.
  script_tracker_->ClearRunnableScripts();
  SetChips(nullptr);
  // TODO(crbug.com/806868): Consider making ClearRunnableScripts part of
  // ExecuteScripts to simplify the controller.
  script_tracker()->ExecuteScript(
      script_path,
      base::BindOnce(&Controller::OnScriptExecuted,
                     // script_tracker_ is owned by Controller.
                     base::Unretained(this), script_path, end_state));
}

void Controller::OnScriptExecuted(const std::string& script_path,
                                  AutofillAssistantState end_state,
                                  const ScriptExecutor::Result& result) {
  if (!result.success) {
    DVLOG(1) << "Failed to execute script " << script_path;
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::SCRIPT_FAILED);
    return;
  }

  if (result.touchable_element_area) {
    touchable_element_area()->SetFromProto(*result.touchable_element_area);
  }

  switch (result.at_end) {
    case ScriptExecutor::SHUTDOWN:
      client_->Shutdown(Metrics::SCRIPT_SHUTDOWN);
      return;

    case ScriptExecutor::SHUTDOWN_GRACEFULLY:
      GetWebController()->ClearCookie();
      EnterStoppedState();
      client_->Shutdown(Metrics::SCRIPT_SHUTDOWN);
      return;

    case ScriptExecutor::CLOSE_CUSTOM_TAB:
      client_->Shutdown(Metrics::CUSTOM_TAB_CLOSED);
      return;

    case ScriptExecutor::RESTART:
      script_tracker_.reset();
      memory_.reset();
      script_domain_ = "";
      break;

    case ScriptExecutor::CONTINUE:
      break;

    default:
      DVLOG(1) << "Unexpected value for at_end: " << result.at_end;
      break;
  }
  EnterState(end_state);
  GetOrCheckScripts();
}

bool Controller::MaybeAutostartScript(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // We want to go through all runnable autostart interrupts first, one at a
  // time. To do that, always run highest priority autostartable interrupt from
  // runnable_script, which is ordered by priority.
  for (const auto& script : runnable_scripts) {
    if (script.autostart && script.interrupt) {
      std::string script_path = script.path;
      ExecuteScript(script_path, state_);
      // making a copy of script.path is necessary, as ExecuteScript clears
      // runnable_scripts, so script.path will not survive until the end of
      // ExecuteScript.
      return true;
    }
  }

  // Under specific conditions, we can directly run a non-interrupt script
  // without first displaying it. This is meant to work only at the very
  // beginning, when no non-interrupt scripts have run, and only if there's
  // exactly one autostartable script.
  if (allow_autostart_) {
    int autostart_count = 0;
    std::string autostart_path;
    for (const auto& script : runnable_scripts) {
      if (script.autostart && !script.interrupt) {
        autostart_count++;
        autostart_path = script.path;
      }
    }
    if (autostart_count == 1) {
      DisableAutostart();
      ExecuteScript(autostart_path, AutofillAssistantState::PROMPT);
      return true;
    }
  }
  return false;
}

void Controller::DisableAutostart() {
  allow_autostart_ = false;
  autostart_timeout_script_path_.clear();
}

void Controller::OnGetCookie(bool has_cookie) {
  if (has_cookie) {
    // This code is only active with the experiment parameter.
    // TODO(crbug.com/806868): Remove the cookie experiment.
    trigger_context_->script_parameters.insert(
        std::make_pair(kWebsiteVisitedBeforeParameterName, kTrueValue));
    OnSetCookie(has_cookie);
    return;
  }
  GetWebController()->SetCookie(
      deeplink_url_.host(),
      base::BindOnce(&Controller::OnSetCookie,
                     // WebController is owned by Controller.
                     base::Unretained(this)));
}

void Controller::OnSetCookie(bool result) {
  DCHECK(result) << "Setting cookie failed";
  FinishStart();
}

void Controller::FinishStart() {
  started_ = true;
  if (allow_autostart_) {
    SetStatusMessage(
        l10n_util::GetStringFUTF8(IDS_AUTOFILL_ASSISTANT_LOADING,
                                  base::UTF8ToUTF16(deeplink_url_.host())));
    SetProgress(kAutostartInitialProgress);
  }
  GetOrCheckScripts();
}

void Controller::InitFromParameters() {
  auto details = std::make_unique<Details>();
  if (details->UpdateFromParameters(trigger_context_->script_parameters))
    SetDetails(std::move(details));

  const auto iter =
      trigger_context_->script_parameters.find(kOverlayColorParameterName);
  if (iter != trigger_context_->script_parameters.end()) {
    std::unique_ptr<OverlayColors> colors = std::make_unique<OverlayColors>();
    std::vector<std::string> color_strings = base::SplitString(
        iter->second, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (color_strings.size() > 0) {
      colors->background = color_strings[0];
    }
    if (color_strings.size() > 1) {
      colors->highlight_border = color_strings[1];
    }
    // Ignore other colors, to allow future versions of the client to support
    // setting more colors.

    SetOverlayColors(std::move(colors));
  }
}

bool Controller::NeedsUI() const {
  return state_ != AutofillAssistantState::INACTIVE &&
         state_ != AutofillAssistantState::STOPPED;
}

void Controller::Start(const GURL& deeplink_url,
                       std::unique_ptr<TriggerContext> trigger_context) {
  if (state_ != AutofillAssistantState::INACTIVE) {
    NOTREACHED();
    return;
  }
  trigger_context_ = std::move(trigger_context);
  InitFromParameters();
  deeplink_url_ = deeplink_url;
  EnterState(AutofillAssistantState::STARTING);
  client_->ShowUI();
  if (IsCookieExperimentEnabled()) {
    GetWebController()->HasCookie(
        base::BindOnce(&Controller::OnGetCookie,
                       // WebController is owned by Controller.
                       base::Unretained(this)));
  } else {
    FinishStart();
  }
}

AutofillAssistantState Controller::GetState() {
  return state_;
}

void Controller::WillShutdown(Metrics::DropOutReason reason) {
  StopPeriodicScriptChecks();
  if (!will_shutdown_) {
    UiController* ui_controller = GetUiController();
    will_shutdown_ = true;
    ui_controller->WillShutdown(reason);
  }
}

void Controller::OnScriptSelected(const std::string& script_path) {
  DCHECK(!script_path.empty());
  ExecuteScript(script_path, AutofillAssistantState::PROMPT);
}

void Controller::UpdateTouchableArea() {
  touchable_element_area()->Update();
}

void Controller::OnUserInteractionInsideTouchableArea() {
  script_tracker()->CheckScripts();
  StartPeriodicScriptChecks();
}

std::string Controller::GetDebugContext() {
  base::Value dict(base::Value::Type::DICTIONARY);

  dict.SetKey("status", base::Value(status_message_));
  if (trigger_context_) {
    std::vector<base::Value> parameters_js;
    for (const auto& entry : trigger_context_->script_parameters) {
      base::Value parameter_js = base::Value(base::Value::Type::DICTIONARY);
      parameter_js.SetKey(entry.first, base::Value(entry.second));
      parameters_js.push_back(std::move(parameter_js));
    }
    dict.SetKey("parameters", base::Value(parameters_js));
  }
  dict.SetKey("scripts", script_tracker()->GetDebugContext());

  if (details_)
    dict.SetKey("details", details_->GetDebugContext());

  std::string output_js;
  base::JSONWriter::Write(dict, &output_js);
  return output_js;
}

const PaymentRequestOptions* Controller::GetPaymentRequestOptions() const {
  return payment_request_options_.get();
}

const PaymentInformation* Controller::GetPaymentRequestInformation() const {
  return payment_request_info_.get();
}

void Controller::OnPaymentRequestContinueButtonClicked() {
  if (!payment_request_options_ || !payment_request_info_)
    return;

  auto callback = std::move(payment_request_options_->callback);
  auto payment_request_info = std::move(payment_request_info_);

  // TODO(crbug.com/806868): succeed is currently always true, but we might want
  // to set it to false and propagate the result to GetPaymentInformationAction
  // when the user clicks "Cancel" during that action.
  payment_request_info->succeed = true;

  SetPaymentRequestOptions(nullptr);
  std::move(callback).Run(std::move(payment_request_info));
}

void Controller::SetShippingAddress(
    std::unique_ptr<autofill::AutofillProfile> address) {
  if (!payment_request_info_)
    return;

  payment_request_info_->shipping_address = std::move(address);
  GetUiController()->OnPaymentRequestInformationChanged(
      payment_request_info_.get());
  UpdatePaymentRequestActions();
}

void Controller::SetBillingAddress(
    std::unique_ptr<autofill::AutofillProfile> address) {
  if (!payment_request_info_)
    return;

  payment_request_info_->billing_address = std::move(address);
  GetUiController()->OnPaymentRequestInformationChanged(
      payment_request_info_.get());
  UpdatePaymentRequestActions();
}

void Controller::SetContactInfo(std::string name,
                                std::string phone,
                                std::string email) {
  if (!payment_request_info_)
    return;

  payment_request_info_->payer_name = name;
  payment_request_info_->payer_phone = phone;
  payment_request_info_->payer_email = email;
  GetUiController()->OnPaymentRequestInformationChanged(
      payment_request_info_.get());
  UpdatePaymentRequestActions();
}

void Controller::SetCreditCard(std::unique_ptr<autofill::CreditCard> card) {
  if (!payment_request_info_)
    return;

  payment_request_info_->card = std::move(card);
  GetUiController()->OnPaymentRequestInformationChanged(
      payment_request_info_.get());
  UpdatePaymentRequestActions();
}

void Controller::SetTermsAndConditions(
    TermsAndConditionsState terms_and_conditions) {
  if (!payment_request_info_)
    return;

  payment_request_info_->terms_and_conditions = terms_and_conditions;
  UpdatePaymentRequestActions();
  GetUiController()->OnPaymentRequestInformationChanged(
      payment_request_info_.get());
}

void Controller::UpdatePaymentRequestActions() {
  // TODO(crbug.com/806868): This method uses #SetChips(), which means that
  // updating the PR actions will also clear the suggestions. We should update
  // the actions only if there are use cases of PR + suggestions.
  if (!payment_request_options_ || !payment_request_info_) {
    return;
  }

  bool contact_info_ok = (!payment_request_options_->request_payer_name ||
                          !payment_request_info_->payer_name.empty()) &&
                         (!payment_request_options_->request_payer_email ||
                          !payment_request_info_->payer_email.empty()) &&
                         (!payment_request_options_->request_payer_phone ||
                          !payment_request_info_->payer_phone.empty());

  bool shipping_address_ok = !payment_request_options_->request_shipping ||
                             payment_request_info_->shipping_address;

  bool payment_method_ok = !payment_request_options_->request_payment_method ||
                           payment_request_info_->card;

  bool terms_ok = payment_request_info_->terms_and_conditions != NOT_SELECTED ||
                  !payment_request_options_->request_terms_and_conditions;

  bool continue_button_enabled =
      contact_info_ok && shipping_address_ok && payment_method_ok && terms_ok;

  auto chips = std::make_unique<std::vector<Chip>>();
  chips->emplace_back();
  if (!payment_request_options_->confirm_button_text.empty()) {
    chips->back().text = payment_request_options_->confirm_button_text;
  } else {
    chips->back().text =
        l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM);
  }
  chips->back().type = HIGHLIGHTED_ACTION;
  chips->back().disabled = !continue_button_enabled;
  if (continue_button_enabled) {
    chips->back().callback =
        base::BindOnce(&Controller::OnPaymentRequestContinueButtonClicked,
                       weak_ptr_factory_.GetWeakPtr());
  }

  SetChips(std::move(chips));
}

void Controller::GetTouchableArea(std::vector<RectF>* area) const {
  if (touchable_element_area_)
    touchable_element_area_->GetTouchableRectangles(area);
}

void Controller::GetRestrictedArea(std::vector<RectF>* area) const {
  if (touchable_element_area_)
    touchable_element_area_->GetRestrictedRectangles(area);
}

void Controller::GetVisualViewport(RectF* visual_viewport) const {
  if (touchable_element_area_)
    touchable_element_area_->GetVisualViewport(visual_viewport);
}

void Controller::OnFatalError(const std::string& error_message,
                              Metrics::DropOutReason reason) {
  LOG(ERROR) << "Autofill Assistant has encountered an error and is shutting "
                "down, reason="
             << reason;
  if (state_ == AutofillAssistantState::STOPPED)
    return;

  StopPeriodicScriptChecks();
  SetStatusMessage(error_message);
  EnterStoppedState();
  client_->Shutdown(reason);
}

void Controller::OnNoRunnableScripts() {
  if (script_tracker()->running())
    return;

  if (state_ == AutofillAssistantState::STARTING) {
    // We're still waiting for the set of initial scripts, but either didn't get
    // any scripts or didn't get scripts that could possibly become runnable
    // with a DOM change.
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_DEFAULT_ERROR),
                 Metrics::NO_INITIAL_SCRIPTS);
    return;
  }

  // We're navigated to a page that has no scripts or the scripts have reached a
  // state from which they cannot recover through a DOM change.
  OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP),
               Metrics::NO_SCRIPTS);
}

void Controller::OnRunnableScriptsChanged(
    const std::vector<ScriptHandle>& runnable_scripts) {
  // Script selection is disabled when a script is already running. We will
  // check again and maybe update when the current script has finished.
  if (script_tracker()->running() || state_ == AutofillAssistantState::STOPPED)
    return;

  if (MaybeAutostartScript(runnable_scripts)) {
    return;
  }

  // Show the initial prompt if available.
  for (const auto& script : runnable_scripts) {
    // runnable_scripts is ordered by priority.
    if (!script.initial_prompt.empty()) {
      SetStatusMessage(script.initial_prompt);
      break;
    }
  }

  // Update the set of scripts in the UI.
  auto chips = std::make_unique<std::vector<Chip>>();
  for (const auto& script : runnable_scripts) {
    if (!script.autostart && (!script.chip.text().empty() ||
                              script.chip.icon() != ChipIcon::NO_ICON)) {
      chips->emplace_back(script.chip);
      chips->back().callback =
          base::BindOnce(&Controller::OnScriptSelected,
                         weak_ptr_factory_.GetWeakPtr(), script.path);
    }
  }
  SetDefaultChipType(chips.get());

  if (chips->empty() && state_ == AutofillAssistantState::STARTING) {
    // Continue waiting
    return;
  }

  if (allow_autostart_ ||
      state_ == AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT) {
    // Autostart was expected, but only non-autostartable scripts were found.
    DisableAutostart();
    EnterState(AutofillAssistantState::AUTOSTART_FALLBACK_PROMPT);
  } else {
    EnterState(AutofillAssistantState::PROMPT);
  }
  SetChips(std::move(chips));
}

void Controller::DidAttachInterstitialPage() {
  client_->Shutdown(Metrics::INTERSTITIAL_PAGE);
}

void Controller::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                               const GURL& validated_url) {
  // validated_url might not be the page URL. Ignore it and always check the
  // last committed url.
  GetOrCheckScripts();
}

void Controller::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  if (!navigating_to_new_document_) {
    navigating_to_new_document_ = true;
    ReportNavigationStateChanged();
  }

  // The following types of navigations are allowed for the main frame, when
  // in PROMPT state:
  //  - first-time URL load
  //  - script-directed navigation, while a script is running unless
  //    there's a touchable area.
  //  - server redirections, which might happen outside of a script, but
  //    because of a load triggered by a previously-running script.
  //  - same-document modifications, which might happen automatically
  //  - javascript-initiated navigation or refresh
  //  - navigation by clicking on a link
  //  In the last two cases, autofill assistant might still give up later on if
  //  it discovers that the new page has no scripts.
  //
  // Everything else, such as going back to a previous page, or refreshing the
  // page is considered an end condition.
  if (state_ == AutofillAssistantState::PROMPT &&
      web_contents()->GetLastCommittedURL().is_valid() &&
      !navigation_handle->WasServerRedirect() &&
      !navigation_handle->IsRendererInitiated()) {
    OnFatalError(l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP),
                 Metrics::NAVIGATION);
  }
}

void Controller::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted() || !IsNavigatingToNewDocument()) {
    return;
  }

  bool is_successful =
      !navigation_handle->IsErrorPage() &&
      navigation_handle->GetNetErrorCode() == net::OK &&
      navigation_handle->GetResponseHeaders() &&
      (navigation_handle->GetResponseHeaders()->response_code() / 100) == 2;
  navigation_error_ = !is_successful;
  navigating_to_new_document_ = false;
  ReportNavigationStateChanged();

  if (is_successful)
    GetOrCheckScripts();
}

void Controller::DocumentAvailableInMainFrame() {
  GetOrCheckScripts();
}

void Controller::RenderProcessGone(base::TerminationStatus status) {
  client_->Shutdown(Metrics::RENDER_PROCESS_GONE);
}

void Controller::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (NeedsUI() &&
      base::FeatureList::IsEnabled(features::kAutofillAssistantChromeEntry)) {
    // Show UI again when re-focused in case the web contents moved activity.
    // This is only enabled when tab-switching is enabled.
    client_->ShowUI();
  }
}

bool Controller::IsCookieExperimentEnabled() const {
  auto iter = trigger_context_->script_parameters.find(kCookieExperimentName);
  return iter != trigger_context_->script_parameters.end() &&
         iter->second == "1";
}

void Controller::OnTouchableAreaChanged(
    const RectF& visual_viewport,
    const std::vector<RectF>& touchable_areas,
    const std::vector<RectF>& restricted_areas) {
  GetUiController()->OnTouchableAreaChanged(visual_viewport, touchable_areas,
                                            restricted_areas);
}

void Controller::SetPaymentRequestOptions(
    std::unique_ptr<PaymentRequestOptions> options) {
  DCHECK(!options || options->callback);

  if (payment_request_options_ == nullptr && options == nullptr)
    return;

  if (options) {
    payment_request_info_ = std::make_unique<PaymentInformation>();

    // TODO(crbug.com/806868): set initial state according to proto.
    payment_request_info_->terms_and_conditions =
        options->initial_terms_and_conditions;
  }

  payment_request_options_ = std::move(options);
  UpdatePaymentRequestActions();
  GetUiController()->OnPaymentRequestOptionsChanged(
      payment_request_options_.get());
  GetUiController()->OnPaymentRequestInformationChanged(
      payment_request_info_.get());
}

ElementArea* Controller::touchable_element_area() {
  if (!touchable_element_area_) {
    touchable_element_area_ = std::make_unique<ElementArea>(this);
    touchable_element_area_->SetOnUpdate(base::BindRepeating(
        &Controller::OnTouchableAreaChanged, weak_ptr_factory_.GetWeakPtr()));
  }
  return touchable_element_area_.get();
}

ScriptTracker* Controller::script_tracker() {
  if (!script_tracker_) {
    script_tracker_ = std::make_unique<ScriptTracker>(/* delegate= */ this,
                                                      /* listener= */ this);
  }
  return script_tracker_.get();
}

}  // namespace autofill_assistant
