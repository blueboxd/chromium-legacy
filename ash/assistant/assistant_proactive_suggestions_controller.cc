// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_proactive_suggestions_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_suggestions_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/ui/proactive_suggestions_rich_view.h"
#include "ash/assistant/ui/proactive_suggestions_simple_view.h"
#include "ash/assistant/ui/proactive_suggestions_view.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/public/cpp/assistant/util/histogram_util.h"
#include "chromeos/services/assistant/public/features.h"
#include "components/viz/common/vertical_scroll_direction.h"
#include "ui/views/widget/widget.h"

namespace ash {

using assistant::util::DeepLinkParam;
using assistant::util::DeepLinkType;
using assistant::util::ProactiveSuggestionsAction;

AssistantProactiveSuggestionsController::
    AssistantProactiveSuggestionsController(
        AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  DCHECK(chromeos::assistant::features::IsProactiveSuggestionsEnabled());
  assistant_controller_->AddObserver(this);
}

AssistantProactiveSuggestionsController::
    ~AssistantProactiveSuggestionsController() {
  auto* client = ProactiveSuggestionsClient::Get();
  if (client)
    client->SetDelegate(nullptr);

  assistant_controller_->RemoveObserver(this);
}

void AssistantProactiveSuggestionsController::
    OnAssistantControllerConstructed() {
  assistant_controller_->suggestions_controller()->AddModelObserver(this);
  assistant_controller_->view_delegate()->AddObserver(this);
}

void AssistantProactiveSuggestionsController::
    OnAssistantControllerDestroying() {
  assistant_controller_->view_delegate()->RemoveObserver(this);
  assistant_controller_->suggestions_controller()->RemoveModelObserver(this);
}

void AssistantProactiveSuggestionsController::OnAssistantReady() {
  // The proactive suggestions client initializes late so we need to wait for
  // the ready signal before binding as its delegate.
  auto* client = ProactiveSuggestionsClient::Get();
  if (client)
    client->SetDelegate(this);
}

void AssistantProactiveSuggestionsController::OnDeepLinkReceived(
    DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  // This controller only handles proactive suggestions deep links.
  if (type != DeepLinkType::kProactiveSuggestions)
    return;

  // Proactive suggestions deep links should specify an |action|.
  const base::Optional<assistant::util::ProactiveSuggestionsAction> action =
      assistant::util::GetDeepLinkParamAsProactiveSuggestionsAction(
          params, DeepLinkParam::kAction);
  if (!action.has_value())
    return;

  switch (action.value()) {
    case ProactiveSuggestionsAction::kCardClick:
      OnCardClickDeepLinkReceived(params);
      return;
    case ProactiveSuggestionsAction::kEntryPointClick:
      OnEntryPointClickDeepLinkReceived(params);
      return;
    case ProactiveSuggestionsAction::kEntryPointClose:
      OnProactiveSuggestionsCloseButtonPressed();
      return;
  }
}

void AssistantProactiveSuggestionsController::
    OnProactiveSuggestionsClientDestroying() {
  auto* client = ProactiveSuggestionsClient::Get();
  if (client)
    client->SetDelegate(nullptr);
}

void AssistantProactiveSuggestionsController::OnProactiveSuggestionsChanged(
    scoped_refptr<const ProactiveSuggestions> proactive_suggestions) {
  // This method is invoked from the ProactiveSuggestionsClient to inform us
  // that the active set of proactive suggestions has changed. Because we have
  // read-only access to the Assistant suggestions model, we forward this event
  // to the Assistant suggestions controller to cache state. We will then react
  // to the change in model state in OnProactiveSuggestionsChanged(new, old).
  assistant_controller_->suggestions_controller()
      ->OnProactiveSuggestionsChanged(std::move(proactive_suggestions));
}

void AssistantProactiveSuggestionsController::
    OnSourceVerticalScrollDirectionChanged(
        viz::VerticalScrollDirection scroll_direction) {
  if (!chromeos::assistant::features::
          IsProactiveSuggestionsShowOnScrollEnabled()) {
    // If show-on-scroll is disabled, presentation logic of the view executes
    // immediately as a result of changes in the active set of proactive
    // suggestions.
    return;
  }

  switch (scroll_direction) {
    case viz::VerticalScrollDirection::kDown:
      // When the user scrolls down, the proactive suggestions widget is hidden
      // as this is a strong signal that the user is engaging with the page.
      HideUi();
      return;
    case viz::VerticalScrollDirection::kUp:
      // When the user scrolls up, the proactive suggestions widget will be
      // shown to the user as this is a signal that the user may be interested
      // in engaging with related content as part of a deeper dive. Note that
      // this method will no-op if there is no active set of proactive
      // suggestions or if the set has been blacklisted from being shown.
      MaybeShowUi();
      return;
    case viz::VerticalScrollDirection::kNull:
      // The value |kNull| only exists to indicate the absence of a vertical
      // scroll direction and should never be propagated in a change event.
      NOTREACHED();
      return;
  }
}

void AssistantProactiveSuggestionsController::OnProactiveSuggestionsChanged(
    scoped_refptr<const ProactiveSuggestions> proactive_suggestions,
    scoped_refptr<const ProactiveSuggestions> old_proactive_suggestions) {
  // If a set of earlier proactive suggestions had been shown, it's safe to
  // assume they are now being closed due to a change in context. Note that this
  // will no-op if the proactive suggestions view does not exist.
  CloseUi(ProactiveSuggestionsShowResult::kCloseByContextChange);

  if (chromeos::assistant::features::
          IsProactiveSuggestionsShowOnScrollEnabled()) {
    // When show-on-scroll is enabled, presentation of the proactive suggestions
    // view is handled in response to changes in vertical scroll direction.
    return;
  }

  // We'll now attempt to show the proactive suggestions view to the user. Note
  // that this method will no-op if there is no active set of proactive
  // suggestions or if the set of proactive suggestions is blacklisted from
  // being shown to the user.
  MaybeShowUi();
}

void AssistantProactiveSuggestionsController::
    OnProactiveSuggestionsCloseButtonPressed() {
  // When the user explicitly closes the proactive suggestions view, that's a
  // strong signal that they don't want to see the same suggestions again so we
  // add it to our blacklist.
  proactive_suggestions_blacklist_.emplace(
      view_->proactive_suggestions()->hash());

  CloseUi(ProactiveSuggestionsShowResult::kCloseByUser);
}

void AssistantProactiveSuggestionsController::
    OnProactiveSuggestionsViewHoverChanged(bool is_hovering) {
  // Hover changed events may occur during the proactive suggestion view's
  // closing sequence. When this occurs, no further action is necessary.
  if (!view_ || !view_->GetWidget() || view_->GetWidget()->IsClosed())
    return;

  if (!is_hovering) {
    // When the user is no longer hovering over the proactive suggestions view
    // we need to reset the timer so that it will auto-close appropriately.
    auto_close_timer_.Reset();
    return;
  }

  const base::TimeDelta remaining_time =
      auto_close_timer_.desired_run_time() - base::TimeTicks::Now();

  // The user is now hovering over the proactive suggestions view so we need to
  // pause the auto-close timer until we are no longer in a hovering state. Once
  // we leave hovering state, we will resume the auto-close timer with whatever
  // |remaining_time| is left. To accomplish this, we schedule the auto-close
  // timer to fire in the future...
  auto_close_timer_.Start(FROM_HERE, remaining_time,
                          auto_close_timer_.user_task());

  // ...but immediately stop it so that when we reset the auto-close timer upon
  // leaving hovering state, the timer will appropriately fire only after the
  // |remaining_time| has elapsed.
  auto_close_timer_.Stop();
}

void AssistantProactiveSuggestionsController::
    OnProactiveSuggestionsViewPressed() {
  CloseUi(ProactiveSuggestionsShowResult::kClick);

  // Clicking on the proactive suggestions view causes the user to enter
  // Assistant UI where a proactive suggestions interaction will be initiated.
  assistant_controller_->ui_controller()->ShowUi(
      AssistantEntryPoint::kProactiveSuggestions);
}

void AssistantProactiveSuggestionsController::OnCardClickDeepLinkReceived(
    const std::map<std::string, std::string>& params) {
  // A deep link of action type |kCardClick| should specify an |kHref| value
  // which is a URL to be handled by Assistant (likely opened in the browser).
  const base::Optional<GURL> url =
      assistant::util::GetDeepLinkParamAsGURL(params, DeepLinkParam::kHref);
  if (url)
    assistant_controller_->OpenUrl(url.value());

  // For metrics tracking, obtain the |category| of the content associated w/
  // the proactive suggestion card that was clicked...
  const base::Optional<int> category =
      assistant::util::GetDeepLinkParamAsInt(params, DeepLinkParam::kCategory);

  // ...as well as the |index| of the card within its list...
  const base::Optional<int> index =
      assistant::util::GetDeepLinkParamAsInt(params, DeepLinkParam::kIndex);

  // ...and finally the VE ID associated w/ the type of card itself.
  const base::Optional<int> veId =
      assistant::util::GetDeepLinkParamAsInt(params, DeepLinkParam::kVeId);

  // We record all of these metrics in UMA to track engagement.
  assistant::metrics::RecordProactiveSuggestionsCardClick(category, index,
                                                          veId);
}

void AssistantProactiveSuggestionsController::OnEntryPointClickDeepLinkReceived(
    const std::map<std::string, std::string>& params) {
  // A deeplink resulting from a click on the proactive suggestions entry point
  // may optionally specify an |kHref| value to teleport to.
  const base::Optional<GURL> teleport =
      assistant::util::GetDeepLinkParamAsGURL(params, DeepLinkParam::kHref);

  // If an |kHref| was specified to teleport to, we'll handle it instead of
  // opening Assistant UI to show the associated set of proactive suggestions.
  // This allows the rich, content-forward entry point to teleport the user
  // directly to a result in the browser if so desired instead of always having
  // to direct the user to the complete inline collection.
  if (teleport.has_value()) {
    CloseUi(ProactiveSuggestionsShowResult::kTeleport);
    assistant_controller_->OpenUrl(teleport.value());
    return;
  }

  // When no |kHref| is specified, we handle this as a normal click on the entry
  // point to open the set of proactive suggestions inline in Assistant UI.
  CloseUi(ProactiveSuggestionsShowResult::kClick);
  assistant_controller_->ui_controller()->ShowUi(
      AssistantEntryPoint::kProactiveSuggestions);
}

void AssistantProactiveSuggestionsController::MaybeShowUi() {
  if (view_) {
    // If the |view_| already exists, calling MaybeShowUi() will just ensure
    // that its widget is showing to the user.
    view_->ShowWhenReady();
    return;
  }

  // Retrieve the cached set of proactive suggestions.
  scoped_refptr<const ProactiveSuggestions> proactive_suggestions =
      assistant_controller_->suggestions_controller()
          ->model()
          ->GetProactiveSuggestions();

  // There's nothing to show if there are no proactive suggestions in the cache.
  if (!proactive_suggestions)
    return;

  // If the cached set of proactive suggestions is blacklisted, it should not be
  // shown to the user. A set of proactive suggestions may be blacklisted as a
  // result of duplicate suppression or as a result of the user explicitly
  // closing the proactive suggestions view.
  if (base::Contains(proactive_suggestions_blacklist_,
                     proactive_suggestions->hash())) {
    RecordProactiveSuggestionsShowAttempt(
        proactive_suggestions->category(),
        ProactiveSuggestionsShowAttempt::kAbortedByDuplicateSuppression);
    return;
  }

  // Depending on which is enabled, we'll either use a rich, content-forward UI
  // affordance or a simple UI affordance as the feature entry point.
  if (chromeos::assistant::features::
          IsProactiveSuggestionsShowRichEntryPointEnabled()) {
    view_ = new ProactiveSuggestionsRichView(
        assistant_controller_->view_delegate());
  } else {
    view_ = new ProactiveSuggestionsSimpleView(
        assistant_controller_->view_delegate());
  }

  view_->Init();
  view_->ShowWhenReady();

  RecordProactiveSuggestionsShowAttempt(
      view_->proactive_suggestions()->category(),
      ProactiveSuggestionsShowAttempt::kSuccess);

  // If duplicate suppression is enabled, the user should not be presented with
  // this set of proactive suggestions again so we add it to our blacklist.
  if (chromeos::assistant::features::
          IsProactiveSuggestionsSuppressDuplicatesEnabled()) {
    proactive_suggestions_blacklist_.emplace(
        view_->proactive_suggestions()->hash());
  }

  // The proactive suggestions view will automatically be closed if the user
  // doesn't interact with it within a fixed time interval.
  auto_close_timer_.Start(
      FROM_HERE,
      chromeos::assistant::features::GetProactiveSuggestionsTimeoutThreshold(),
      base::BindRepeating(&AssistantProactiveSuggestionsController::CloseUi,
                          weak_factory_.GetWeakPtr(),
                          ProactiveSuggestionsShowResult::kCloseByTimeout));
}

void AssistantProactiveSuggestionsController::CloseUi(
    ProactiveSuggestionsShowResult result) {
  if (!view_)
    return;

  RecordProactiveSuggestionsShowResult(
      view_->proactive_suggestions()->category(), result);

  auto_close_timer_.Stop();

  view_->Close();
  view_ = nullptr;
}

void AssistantProactiveSuggestionsController::HideUi() {
  if (view_)
    view_->Hide();
}

}  // namespace ash
