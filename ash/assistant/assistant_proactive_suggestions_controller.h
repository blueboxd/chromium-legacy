// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_PROACTIVE_SUGGESTIONS_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_PROACTIVE_SUGGESTIONS_CONTROLLER_H_

#include <memory>
#include <set>

#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/proactive_suggestions_client.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class AssistantControllerImpl;
class ProactiveSuggestions;
class ProactiveSuggestionsView;

namespace assistant {
namespace metrics {
enum class ProactiveSuggestionsShowAttempt;
enum class ProactiveSuggestionsShowResult;
}  // namespace metrics
}  // namespace assistant

// Controller for the Assistant proactive suggestions feature.
class AssistantProactiveSuggestionsController
    : public AssistantControllerObserver,
      public ProactiveSuggestionsClient::Delegate,
      public AssistantSuggestionsModelObserver,
      public AssistantViewDelegateObserver,
      public views::WidgetObserver {
 public:
  using ProactiveSuggestionsShowAttempt =
      assistant::metrics::ProactiveSuggestionsShowAttempt;
  using ProactiveSuggestionsShowResult =
      assistant::metrics::ProactiveSuggestionsShowResult;

  explicit AssistantProactiveSuggestionsController(
      AssistantControllerImpl* assistant_controller);
  ~AssistantProactiveSuggestionsController() override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnAssistantReady() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // ProactiveSuggestionsClient::Delegate:
  void OnProactiveSuggestionsClientDestroying() override;
  void OnProactiveSuggestionsChanged(
      scoped_refptr<const ProactiveSuggestions> proactive_suggestions) override;
  void OnSourceVerticalScrollDirectionChanged(
      viz::VerticalScrollDirection scroll_direction) override;

  // AssistantSuggestionsModelObserver:
  void OnProactiveSuggestionsChanged(
      scoped_refptr<const ProactiveSuggestions> proactive_suggestions,
      scoped_refptr<const ProactiveSuggestions> old_proactive_suggestions)
      override;

  // AssistantViewDelegateObserver:
  void OnProactiveSuggestionsCloseButtonPressed() override;
  void OnProactiveSuggestionsViewHoverChanged(bool is_hovering) override;
  void OnProactiveSuggestionsViewPressed() override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void OnCardClickDeepLinkReceived(
      const std::map<std::string, std::string>& params);
  void OnEntryPointClickDeepLinkReceived(
      const std::map<std::string, std::string>& params);
  void OnViewImpressionDeepLinkReceived(
      const std::map<std::string, std::string>& params);

  void MaybeShowUi();
  void CloseUi(ProactiveSuggestionsShowResult result);
  void HideUi();

  AssistantControllerImpl* const assistant_controller_;  // Owned by Shell.

  ProactiveSuggestionsView* view_ = nullptr;  // Owned by view hierarchy.

  // When shown, the proactive suggestions view will automatically be closed if
  // the user doesn't interact with it within a fixed time interval.
  base::RetainingOneShotTimer auto_close_timer_;

  // If the hash for a set of proactive suggestions is found in this collection,
  // they should not be shown to the user. A set of proactive suggestions may be
  // added to the blacklist as a result of duplicate suppression or as a result
  // of the user explicitly closing the proactive suggestions view.
  std::set<size_t> proactive_suggestions_blacklist_;

  // We record different histograms the first time that a set of proactive
  // suggestions are shown than we do on subsequent shows. This allows us to
  // measure user engagement the first time the entry point is presented in
  // comparison to follow up presentations of the same content.
  std::set<size_t> proactive_suggestions_seen_by_user_;

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

  base::WeakPtrFactory<AssistantProactiveSuggestionsController> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(AssistantProactiveSuggestionsController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_PROACTIVE_SUGGESTIONS_CONTROLLER_H_
