// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_ui_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/ui/assistant_container_view.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/proactive_suggestions_view.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/assistant/util/histogram_util.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/public/cpp/assistant/util/histogram_util.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "base/bind.h"
#include "base/optional.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// When hidden, Assistant will automatically close after |kAutoCloseThreshold|.
constexpr base::TimeDelta kAutoCloseThreshold = base::TimeDelta::FromMinutes(5);

// Toast -----------------------------------------------------------------------

constexpr int kToastDurationMs = 2500;

constexpr char kStylusPromptToastId[] = "stylus_prompt_for_embedded_ui";
constexpr char kUnboundServiceToastId[] =
    "assistant_controller_unbound_service";

void ShowToast(const std::string& id, int message_id) {
  ToastData toast(id, l10n_util::GetStringUTF16(message_id), kToastDurationMs,
                  base::nullopt);
  Shell::Get()->toast_manager()->Show(toast);
}

}  // namespace

// AssistantUiController -------------------------------------------------------

AssistantUiController::AssistantUiController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  AddModelObserver(this);
  assistant_controller_->AddObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantUiController::~AssistantUiController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  assistant_controller_->RemoveObserver(this);
  RemoveModelObserver(this);
}

void AssistantUiController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;
}

void AssistantUiController::AddModelObserver(
    AssistantUiModelObserver* observer) {
  model_.AddObserver(observer);
}

void AssistantUiController::RemoveModelObserver(
    AssistantUiModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AssistantUiController::OnWidgetActivationChanged(views::Widget* widget,
                                                      bool active) {
  if (active) {
    container_view_->RequestFocus();
  } else if (model_.ui_mode() != AssistantUiMode::kMiniUi) {
    // When the Assistant widget is deactivated we should hide Assistant UI. We
    // don't do this when in mini UI mode since Assistant in that state should
    // not be obstructing much content. Note that we already handle press events
    // happening outside of the UI container but this will also handle the case
    // where we are deactivated without a press event occurring. This happens,
    // for example, when launching Chrome OS feedback using keyboard shortcuts.
    HideUi(AssistantExitPoint::kUnspecified);
  }
}

void AssistantUiController::OnWidgetVisibilityChanged(views::Widget* widget,
                                                      bool visible) {
  UpdateUiMode();
}

void AssistantUiController::OnWidgetDestroying(views::Widget* widget) {
  // We need to update the model when the widget is destroyed as this may have
  // happened outside our control. This can occur as the result of pressing the
  // ESC key, for example.
  model_.SetClosed(AssistantExitPoint::kUnspecified);

  ResetContainerView();
}

void AssistantUiController::OnInputModalityChanged(
    InputModality input_modality) {
  UpdateUiMode();
}

void AssistantUiController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kActive)
    return;

  // If there is an active interaction, we need to show Assistant UI if it is
  // not already showing. We don't have enough information here to know what
  // the interaction source is.
  ShowUi(AssistantEntryPoint::kUnspecified);

  // We also need to ensure that we're in the appropriate UI mode if we aren't
  // already so that the interaction is visible to the user. Note that we
  // indicate that this UI mode change is occurring due to an interaction so
  // that we won't inadvertently stop the interaction due to the UI mode change.
  UpdateUiMode(app_list_features::IsEmbeddedAssistantUIEnabled()
                   ? AssistantUiMode::kLauncherEmbeddedUi
                   : AssistantUiMode::kMainUi,
               /*due_to_interaction=*/true);
}

void AssistantUiController::OnMicStateChanged(MicState mic_state) {
  // When the mic is opened we update the UI mode to ensure that the user is
  // being presented with the main stage. When closing the mic it is appropriate
  // to stay in whatever UI mode we are currently in.
  if (mic_state == MicState::kOpen)
    UpdateUiMode();
}

void AssistantUiController::OnProactiveSuggestionsChanged(
    scoped_refptr<const ProactiveSuggestions> proactive_suggestions,
    scoped_refptr<const ProactiveSuggestions> old_proactive_suggestions) {
  using assistant::metrics::ProactiveSuggestionsShowAttempt;
  using assistant::metrics::ProactiveSuggestionsShowResult;

  const bool should_suppress_duplicates = chromeos::assistant::features::
      IsProactiveSuggestionsSuppressDuplicatesEnabled();

  // When proactive suggestions duplicate suppression is enabled, we'll need to
  // check if a set of proactive suggestions has already been shown before
  // showing it to the user. Per UX requirement, proactive suggestions are
  // considered duplicates for purposes of suppression if they share the same
  // content |description|, regardless of whether or not their respective |html|
  // matches. Note that we only calculate the hash here if needed.
  const size_t proactive_suggestions_hash =
      proactive_suggestions && should_suppress_duplicates
          ? base::FastHash(proactive_suggestions->description())
          : static_cast<size_t>(0);

  bool should_show = !!proactive_suggestions;
  if (should_show && should_suppress_duplicates) {
    should_show = !base::Contains(shown_proactive_suggestions_,
                                  proactive_suggestions_hash);
    if (!should_show) {
      // If this code is reached then the new proactive suggestion is not being
      // shown due to duplicate suppression. We need to record that event.
      assistant::metrics::RecordProactiveSuggestionsShowAttempt(
          proactive_suggestions->category(),
          ProactiveSuggestionsShowAttempt::kAbortedByDuplicateSuppression);
    }
  }

  // When proactive suggestions need to be shown, we show the associated view if
  // it isn't showing already. If it's already showing, no action need be taken.
  if (should_show) {
    if (!proactive_suggestions_view_) {
      CreateProactiveSuggestionsView();
      proactive_suggestions_view_->GetWidget()->ShowInactive();
    } else {
      // Since the view was previously shown, we can infer that the previously
      // shown proactive suggestion was replaced due to a context change.
      assistant::metrics::RecordProactiveSuggestionsShowResult(
          old_proactive_suggestions->category(),
          ProactiveSuggestionsShowResult::kCloseByContextChange);
    }

    // When suppressing duplicates, we need to cache the hash for the proactive
    // suggestions that are being shown so that we don't show them again.
    if (should_suppress_duplicates)
      shown_proactive_suggestions_.emplace(proactive_suggestions_hash);

    // Nothing has prevented the proactive suggestion from being shown so we can
    // record a successful show attempt.
    assistant::metrics::RecordProactiveSuggestionsShowAttempt(
        proactive_suggestions->category(),
        ProactiveSuggestionsShowAttempt::kSuccess);

    // The proactive suggestions widget will automatically be closed if the user
    // doesn't interact with it within a fixed interval.
    auto_close_proactive_suggestions_timer_.Start(
        FROM_HERE,
        chromeos::assistant::features::
            GetProactiveSuggestionsTimeoutThreshold(),
        base::BindRepeating(
            &AssistantUiController::ResetProactiveSuggestionsView,
            weak_factory_.GetWeakPtr(), proactive_suggestions->category(),
            assistant::metrics::ProactiveSuggestionsShowResult::
                kCloseByTimeout));
    return;
  }

  // When proactive suggestions should not be shown, we need to ensure that the
  // associated view is absent if it isn't already.
  if (proactive_suggestions_view_) {
    // Since the view was previously shown, we can infer that the previously
    // shown proactive suggestion was closed due to a context change.
    ResetProactiveSuggestionsView(
        old_proactive_suggestions->category(),
        ProactiveSuggestionsShowResult::kCloseByContextChange);
    DCHECK(!proactive_suggestions_view_);
  }
}

void AssistantUiController::OnScreenContextRequestStateChanged(
    ScreenContextRequestState request_state) {
  if (model_.visibility() != AssistantVisibility::kVisible)
    return;

  // TODO(wutao): Behavior is not defined.
  if (model_.ui_mode() == AssistantUiMode::kLauncherEmbeddedUi)
    return;

  // Once screen context request state has become idle, it is safe to activate
  // the Assistant widget without causing complications.
  if (container_view_ && request_state == ScreenContextRequestState::kIdle)
    container_view_->GetWidget()->Activate();
}

bool AssistantUiController::OnCaptionButtonPressed(AssistantButtonId id) {
  switch (id) {
    case AssistantButtonId::kBack:
      UpdateUiMode(app_list_features::IsEmbeddedAssistantUIEnabled()
                       ? AssistantUiMode::kLauncherEmbeddedUi
                       : AssistantUiMode::kMainUi);
      return true;
    case AssistantButtonId::kClose:
      CloseUi(AssistantExitPoint::kCloseButton);
      return true;
    case AssistantButtonId::kMinimize:
      UpdateUiMode(AssistantUiMode::kMiniUi);
      return true;
    default:
      // No action necessary.
      break;
  }
  return false;
}

// TODO(dmblack): This event doesn't need to be handled here anymore. Move it
// out of AssistantUiController.
void AssistantUiController::OnDialogPlateButtonPressed(AssistantButtonId id) {
  if (id != AssistantButtonId::kSettings)
    return;

  // Launch Assistant Settings via deep link.
  assistant_controller_->OpenUrl(
      assistant::util::CreateAssistantSettingsDeepLink());
}

void AssistantUiController::OnMiniViewPressed() {
  InputModality input_modality = assistant_controller_->interaction_controller()
                                     ->model()
                                     ->input_modality();

  // When not using stylus input modality, pressing the Assistant mini view
  // will cause the UI to expand.
  if (input_modality != InputModality::kStylus)
    UpdateUiMode(AssistantUiMode::kMainUi);
}

void AssistantUiController::OnProactiveSuggestionsCloseButtonPressed() {
  ResetProactiveSuggestionsView(
      assistant_controller_->suggestions_controller()
          ->model()
          ->GetProactiveSuggestions()
          ->category(),
      assistant::metrics::ProactiveSuggestionsShowResult::kCloseByUser);
  DCHECK(!proactive_suggestions_view_);
}

void AssistantUiController::OnProactiveSuggestionsViewPressed() {
  ResetProactiveSuggestionsView(
      assistant_controller_->suggestions_controller()
          ->model()
          ->GetProactiveSuggestions()
          ->category(),
      assistant::metrics::ProactiveSuggestionsShowResult::kClick);
  DCHECK(!proactive_suggestions_view_);

  ShowUi(AssistantEntryPoint::kProactiveSuggestions);
}

void AssistantUiController::OnHighlighterEnabledChanged(
    HighlighterEnabledState state) {
  if (app_list_features::IsEmbeddedAssistantUIEnabled()) {
    if (state == HighlighterEnabledState::kEnabled) {
      ShowToast(kStylusPromptToastId, IDS_ASH_ASSISTANT_PROMPT_STYLUS);
      CloseUi(AssistantExitPoint::kStylus);
    }
    return;
  }

  switch (state) {
    case HighlighterEnabledState::kEnabled:
      if (model_.visibility() != AssistantVisibility::kVisible)
        ShowUi(AssistantEntryPoint::kStylus);
      break;
    case HighlighterEnabledState::kDisabledByUser:
      if (model_.visibility() == AssistantVisibility::kVisible)
        HideUi(AssistantExitPoint::kStylus);
      break;
    case HighlighterEnabledState::kDisabledBySessionComplete:
    case HighlighterEnabledState::kDisabledBySessionAbort:
      // No action necessary.
      break;
  }
}

void AssistantUiController::OnAssistantControllerConstructed() {
  assistant_controller_->interaction_controller()->AddModelObserver(this);
  assistant_controller_->screen_context_controller()->AddModelObserver(this);
  assistant_controller_->suggestions_controller()->AddModelObserver(this);
  assistant_controller_->view_delegate()->AddObserver(this);
}

void AssistantUiController::OnAssistantControllerDestroying() {
  assistant_controller_->view_delegate()->RemoveObserver(this);
  assistant_controller_->suggestions_controller()->RemoveModelObserver(this);
  assistant_controller_->screen_context_controller()->RemoveModelObserver(this);
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);

  if (container_view_) {
    // Our view hierarchy should not outlive our controllers.
    container_view_->GetWidget()->CloseNow();
    DCHECK_EQ(nullptr, container_view_);
  }
}

void AssistantUiController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  if (!assistant::util::IsWebDeepLinkType(type, params))
    return;

  ShowUi(AssistantEntryPoint::kDeepLink);
  UpdateUiMode(AssistantUiMode::kWebUi);
}

void AssistantUiController::OnOpeningUrl(const GURL& url,
                                         bool in_background,
                                         bool from_server) {
  if (model_.visibility() != AssistantVisibility::kVisible)
    return;

  // If the specified |url| should be opening |in_background| with respect to
  // Assistant UI, we transition into mini state so as not to obstruct the
  // browser.
  // TODO(b/134931713): Desired behavior is not yet defined when Assistant is
  // embedded in the launcher.
  // We close the Assistant UI entirely when opening a new browser tab if the
  // navigation was initiated by a server response. Otherwise the navigation
  // was user initiated so we only hide the UI to retain session state. That way
  // the user can choose to resume their session if they are so inclined.
  // However, we close the UI if the feature |IsEmbeddedAssistantUIEnabled| is
  // enabled, where we only maintain |kVisible| and |kClosed| two states.
  if (in_background && !app_list_features::IsEmbeddedAssistantUIEnabled())
    UpdateUiMode(AssistantUiMode::kMiniUi);
  else if (from_server)
    CloseUi(AssistantExitPoint::kNewBrowserTabFromServer);
  else if (app_list_features::IsEmbeddedAssistantUIEnabled())
    CloseUi(AssistantExitPoint::kNewBrowserTabFromUser);
  else
    HideUi(AssistantExitPoint::kNewBrowserTabFromUser);
}

void AssistantUiController::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  AssistantState::Get()->NotifyStatusChanged(
      new_visibility == AssistantVisibility::kVisible
          ? mojom::AssistantState::VISIBLE
          : mojom::AssistantState::READY);

  switch (new_visibility) {
    case AssistantVisibility::kClosed:
      // When the UI is closed, we stop the auto close timer as it may be
      // running and also stop monitoring events.
      auto_close_timer_.Stop();
      event_monitor_.reset();
      break;
    case AssistantVisibility::kHidden:
      // When hiding the UI, we start a timer to automatically close ourselves
      // after |kAutoCloseThreshold|. This is to give the user an opportunity to
      // resume their previous session before it is automatically finished.
      auto_close_timer_.Start(
          FROM_HERE, kAutoCloseThreshold,
          base::BindRepeating(&AssistantUiController::CloseUi,
                              weak_factory_.GetWeakPtr(),
                              AssistantExitPoint::kUnspecified));

      // Because the UI is not visible we needn't monitor events.
      event_monitor_.reset();
      break;
    case AssistantVisibility::kVisible:
      // Upon becoming visible, we stop the auto close timer.
      auto_close_timer_.Stop();

      // Only record the entry point when Assistant UI becomes visible.
      assistant::util::RecordAssistantEntryPoint(entry_point.value());

      if (!container_view_) {
        DCHECK_EQ(AssistantUiMode::kLauncherEmbeddedUi, model_.ui_mode());
        event_monitor_.reset();
        break;
      }

      // We need to monitor events for the root window while we're visible to
      // give us an opportunity to dismiss Assistant UI when the user starts an
      // interaction outside of our bounds. TODO(dmblack): Investigate how this
      // behaves in a multi-display environment.
      gfx::NativeWindow root_window =
          container_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
      event_monitor_ = views::EventMonitor::CreateWindowMonitor(
          this, root_window, {ui::ET_MOUSE_PRESSED, ui::ET_TOUCH_PRESSED});

      // We also want to associate the window for Assistant UI with the active
      // user so that we don't leak across user sessions.
      auto* window_manager = MultiUserWindowManagerImpl::Get();
      if (window_manager) {
        const UserSession* user_session =
            Shell::Get()->session_controller()->GetUserSession(0);
        if (user_session) {
          container_view_->GetWidget()->GetNativeWindow()->SetProperty(
              aura::client::kCreatedByUserGesture, true);
          window_manager->SetWindowOwner(
              container_view_->GetWidget()->GetNativeWindow(),
              user_session->user_info.account_id);
        }
      }
      break;
  }

  // Metalayer should not be sticky. Disable when the UI is no longer visible.
  if (old_visibility == AssistantVisibility::kVisible) {
    if (exit_point != AssistantExitPoint::kStylus)
      Shell::Get()->highlighter_controller()->AbortSession();

    // Only record the exit point when Assistant UI becomes invisible to
    // avoid recording duplicate events (e.g. pressing ESC key).
    assistant::util::RecordAssistantExitPoint(exit_point.value());
  }
}

void AssistantUiController::ShowUi(AssistantEntryPoint entry_point) {
  // Skip if the opt-in window is active.
  auto* assistant_setup = AssistantSetup::GetInstance();
  if (assistant_setup && assistant_setup->BounceOptInWindowIfActive())
    return;

  auto* assistant_state = AssistantState::Get();

  if (!assistant_state->settings_enabled().value_or(false) ||
      assistant_state->locked_full_screen_enabled().value_or(false)) {
    return;
  }

  // TODO(dmblack): Show a more helpful message to the user.
  if (assistant_state->assistant_state() == mojom::AssistantState::NOT_READY) {
    ShowToast(kUnboundServiceToastId, IDS_ASH_ASSISTANT_ERROR_GENERIC);
    return;
  }

  if (!assistant_) {
    ShowToast(kUnboundServiceToastId, IDS_ASH_ASSISTANT_ERROR_GENERIC);
    return;
  }

  if (app_list_features::IsEmbeddedAssistantUIEnabled()) {
    model_.SetUiMode(AssistantUiMode::kLauncherEmbeddedUi);
    model_.SetVisible(entry_point);
    return;
  }

  DCHECK_NE(AssistantUiMode::kLauncherEmbeddedUi, model_.ui_mode());

  if (model_.visibility() == AssistantVisibility::kVisible) {
    // If Assistant window is already visible, we just try to retake focus.
    container_view_->GetWidget()->Activate();
    return;
  }

  if (!container_view_)
    CreateContainerView();

  // Note that we initially show the Assistant widget as inactive. This is
  // necessary due to limitations imposed by retrieving screen context. Once we
  // have finished retrieving screen context, the Assistant widget is activated.
  container_view_->GetWidget()->ShowInactive();
  model_.SetVisible(entry_point);
}

void AssistantUiController::HideUi(AssistantExitPoint exit_point) {
  if (model_.visibility() != AssistantVisibility::kVisible)
    return;

  model_.SetHidden(exit_point);

  if (container_view_)
    container_view_->GetWidget()->Hide();
}

void AssistantUiController::CloseUi(AssistantExitPoint exit_point) {
  if (model_.visibility() == AssistantVisibility::kClosed)
    return;

  model_.SetClosed(exit_point);

  if (container_view_) {
    container_view_->GetWidget()->CloseNow();
    DCHECK_EQ(nullptr, container_view_);
  }
}

void AssistantUiController::ToggleUi(
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  // When not visible, toggling will show the UI.
  if (model_.visibility() != AssistantVisibility::kVisible) {
    DCHECK(entry_point.has_value());
    ShowUi(entry_point.value());
    return;
  }

  // When in mini state, toggling will restore the main UI.
  if (model_.ui_mode() == AssistantUiMode::kMiniUi) {
    UpdateUiMode(AssistantUiMode::kMainUi);
    return;
  }

  // In all other cases, toggling closes the UI.
  DCHECK(exit_point.has_value());
  CloseUi(exit_point.value());
}

void AssistantUiController::UpdateUiMode(
    base::Optional<AssistantUiMode> ui_mode,
    bool due_to_interaction) {
  // If a UI mode is provided, we will use it in lieu of updating UI mode on the
  // basis of interaction/widget visibility state.
  if (ui_mode.has_value()) {
    AssistantUiMode mode = ui_mode.value();
    // TODO(wutao): Behavior is not defined.
    if (model_.ui_mode() == AssistantUiMode::kLauncherEmbeddedUi)
      DCHECK_NE(AssistantUiMode::kMiniUi, mode);
    model_.SetUiMode(mode, due_to_interaction);
    return;
  }

  if (app_list_features::IsEmbeddedAssistantUIEnabled()) {
    model_.SetUiMode(AssistantUiMode::kLauncherEmbeddedUi, due_to_interaction);
    return;
  }

  InputModality input_modality = assistant_controller_->interaction_controller()
                                     ->model()
                                     ->input_modality();

  // When stylus input modality is selected, we should be in mini UI mode.
  // Otherwise we fall back to main UI mode.
  model_.SetUiMode(input_modality == InputModality::kStylus
                       ? AssistantUiMode::kMiniUi
                       : AssistantUiMode::kMainUi,
                   due_to_interaction);
}

void AssistantUiController::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  DCHECK(container_view_ || proactive_suggestions_view_);

  // Check the display for root window and where the keyboard shows to handle
  // the case when there are multiple monitors and the virtual keyboard is shown
  // on a different display other than Assistant UI.
  // TODO(https://crbug.com/943446): Directly compare with the root window of
  // the virtual keyboard controller.
  aura::Window* root_window = GetRootWindow();

  display::Display keyboard_display =
      display::Screen::GetScreen()->GetDisplayMatching(new_bounds_in_screen);

  if (!new_bounds_in_screen.IsEmpty() &&
      root_window !=
          Shell::Get()->GetRootWindowForDisplayId(keyboard_display.id())) {
    return;
  }

  // Cache the keyboard workspace occluded bounds.
  keyboard_workspace_occluded_bounds_ = new_bounds_in_screen;

  // This keyboard event handles the Assistant UI change when:
  // 1. accessibility keyboard or normal virtual keyboard pops up or
  // dismisses. 2. display metrics change (zoom in/out or rotation) when
  // keyboard shows.
  UpdateUsableWorkArea(root_window);
}

void AssistantUiController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  DCHECK(container_view_ || proactive_suggestions_view_);

  // Disable this display event when virtual keyboard shows for solving the
  // inconsistency between normal virtual keyboard and accessibility keyboard in
  // changing the work area (accessibility keyboard will change the display work
  // area but virtual keyboard won't). Display metrics change with keyboard
  // showing is instead handled by OnKeyboardOccludedBoundsChanged.
  if (!keyboard_workspace_occluded_bounds_.IsEmpty())
    return;

  aura::Window* root_window = GetRootWindow();

  if (root_window == Shell::Get()->GetRootWindowForDisplayId(display.id()))
    UpdateUsableWorkArea(root_window);
}

void AssistantUiController::OnEvent(const ui::Event& event) {
  DCHECK(event.type() == ui::ET_MOUSE_PRESSED ||
         event.type() == ui::ET_TOUCH_PRESSED);

  // We're going to perform some checks below to see if the user has pressed
  // outside of the Assistant or virtual keyboard bounds to hide UI for the
  // user's convenience. When Assistant is in mini UI state, we shouldn't be
  // obstructing much content so we can remain visible and quit early.
  if (model_.ui_mode() == AssistantUiMode::kMiniUi)
    return;

  const ui::LocatedEvent* located_event = event.AsLocatedEvent();
  const gfx::Point screen_location =
      event.target() ? event.target()->GetScreenLocation(*located_event)
                     : located_event->root_location();

  const gfx::Rect screen_bounds =
      container_view_->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect keyboard_bounds = keyboard::KeyboardUIController::Get()
                                        ->GetWorkspaceOccludedBoundsInScreen();

  // Pressed events outside our widget bounds should result in hiding of the
  // Assistant UI. The exception to this rule is if the user is interacting
  // with the virtual keyboard in which case we should not dismiss Assistant UI.
  // Note that this event does not fire during a Metalayer session so we needn't
  // enforce logic to prevent hiding when using the stylus.
  if (!screen_bounds.Contains(screen_location) &&
      !keyboard_bounds.Contains(screen_location)) {
    HideUi(AssistantExitPoint::kOutsidePress);
  }
}

void AssistantUiController::UpdateUsableWorkArea(aura::Window* root_window) {
  gfx::Rect usable_work_area;
  gfx::Rect screen_bounds = root_window->GetBoundsInScreen();

  if (keyboard_workspace_occluded_bounds_.height() != 0) {
    // When keyboard shows. Unlike accessibility keyboard, normal virtual
    // keyboard won't change the display work area, so the new usable work
    // area needs to be calculated manually by subtracting the keyboard
    // occluded bounds from the screen bounds.
    usable_work_area = gfx::Rect(
        screen_bounds.x(), screen_bounds.y(), screen_bounds.width(),
        screen_bounds.height() - keyboard_workspace_occluded_bounds_.height());
  } else {
    // When keyboard hides, the new usable display work area is the same
    // as the whole display work area for the root window.
    display::Display display =
        display::Screen::GetScreen()->GetDisplayMatching(screen_bounds);
    usable_work_area = display.work_area();
  }

  usable_work_area.Inset(kMarginDip, kMarginDip);
  model_.SetUsableWorkArea(usable_work_area);
}

AssistantContainerView* AssistantUiController::GetViewForTest() {
  return container_view_;
}

void AssistantUiController::CreateContainerView() {
  DCHECK(!container_view_);
  DCHECK(!app_list_features::IsEmbeddedAssistantUIEnabled());

  container_view_ =
      new AssistantContainerView(assistant_controller_->view_delegate());
  container_view_->GetWidget()->AddObserver(this);

  UpdateUsableWorkAreaObservers();
}

void AssistantUiController::ResetContainerView() {
  DCHECK(container_view_);

  container_view_->GetWidget()->RemoveObserver(this);
  container_view_ = nullptr;

  UpdateUsableWorkAreaObservers();
}

void AssistantUiController::CreateProactiveSuggestionsView() {
  DCHECK(!proactive_suggestions_view_);

  proactive_suggestions_view_ =
      new ProactiveSuggestionsView(assistant_controller_->view_delegate());

  UpdateUsableWorkAreaObservers();
}

void AssistantUiController::ResetProactiveSuggestionsView(
    int category,
    assistant::metrics::ProactiveSuggestionsShowResult result) {
  DCHECK(proactive_suggestions_view_);

  auto_close_proactive_suggestions_timer_.Stop();

  proactive_suggestions_view_->GetWidget()->Close();
  proactive_suggestions_view_ = nullptr;

  assistant::metrics::RecordProactiveSuggestionsShowResult(category, result);

  UpdateUsableWorkAreaObservers();
}

aura::Window* AssistantUiController::GetRootWindow() {
  DCHECK(container_view_ || proactive_suggestions_view_);
  return container_view_
             ? container_view_->GetWidget()->GetNativeWindow()->GetRootWindow()
             : proactive_suggestions_view_->GetWidget()
                   ->GetNativeWindow()
                   ->GetRootWindow();
}

void AssistantUiController::UpdateUsableWorkAreaObservers() {
  // To save resources, we only observe the usable work area when Assistant UI
  // exists as we otherwise don't need to respond to events in realtime.
  const bool should_observe_usable_work_area =
      container_view_ || proactive_suggestions_view_;

  if (should_observe_usable_work_area == is_observing_usable_work_area_)
    return;

  is_observing_usable_work_area_ = should_observe_usable_work_area;

  if (!is_observing_usable_work_area_) {
    keyboard::KeyboardUIController::Get()->RemoveObserver(this);
    display::Screen::GetScreen()->RemoveObserver(this);
    return;
  }

  display::Screen::GetScreen()->AddObserver(this);
  keyboard::KeyboardUIController::Get()->AddObserver(this);

  // Retrieve the current keyboard occluded bounds.
  keyboard_workspace_occluded_bounds_ =
      keyboard::KeyboardUIController::Get()
          ->GetWorkspaceOccludedBoundsInScreen();

  // Set the initial usable work area.
  UpdateUsableWorkArea(GetRootWindow());
}

}  // namespace ash
