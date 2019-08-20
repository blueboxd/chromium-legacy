// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_

#include <map>
#include <memory>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/base/assistant_scroll_view.h"
#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/views/controls/scroll_view.h"

namespace views {
class BoxLayout;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

// SuggestionContainerView is the child of AssistantMainView concerned with
// laying out SuggestionChipViews in response to Assistant interaction model
// suggestion events.
class COMPONENT_EXPORT(ASSISTANT_UI) SuggestionContainerView
    : public AssistantScrollView,
      public AssistantInteractionModelObserver,
      public AssistantSuggestionsModelObserver,
      public AssistantUiModelObserver,
      public views::ButtonListener {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
  using AssistantSuggestionPtr =
      chromeos::assistant::mojom::AssistantSuggestionPtr;

  explicit SuggestionContainerView(AssistantViewDelegate* delegate);
  ~SuggestionContainerView() override;

  // AssistantScrollView:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnContentsPreferredSizeChanged(views::View* content_view) override;

  // AssistantInteractionModelObserver:
  void OnResponseChanged(
      const std::shared_ptr<AssistantResponse>& response) override;
  void OnResponseCleared() override;

  // AssistantSuggestionsModelObserver:
  void OnConversationStartersChanged(
      const std::map<int, const AssistantSuggestion*>& conversation_starters)
      override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void InitLayout();

  void OnSuggestionsChanged(
      const std::map<int, const AssistantSuggestion*>& suggestions);
  void OnSuggestionsCleared();

  // Invoked on suggestion chip icon downloaded event.
  void OnSuggestionChipIconDownloaded(int id, const gfx::ImageSkia& icon);

  AssistantViewDelegate* const delegate_;  // Owned by Shell.

  views::BoxLayout* layout_manager_;  // Owned by view hierarchy.

  // Cache of suggestion chip views owned by the view hierarchy. The key for the
  // map is the unique identifier by which the Assistant interaction model
  // identifies the view's underlying suggestion.
  std::map<int, SuggestionChipView*> suggestion_chip_views_;

  // True if we have received a query response during this Assistant UI session,
  // false otherwise.
  bool has_received_response_ = false;

  // Weak pointer factory used for image downloading requests.
  base::WeakPtrFactory<SuggestionContainerView> download_request_weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(SuggestionContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_
