// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_

#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/ui/base/assistant_scroll_view.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/view_observer.h"

namespace ash {

class AssistantResponse;
class AssistantCardElement;
class AssistantTextElement;
class AssistantViewDelegate;

// UiElementContainerView is the child of AssistantMainView concerned with
// laying out text views and embedded card views in response to Assistant
// interaction model UI element events.
class COMPONENT_EXPORT(ASSISTANT_UI) UiElementContainerView
    : public AssistantScrollView,
      public AssistantInteractionModelObserver {
 public:
  explicit UiElementContainerView(AssistantViewDelegate* delegate);
  ~UiElementContainerView() override;

  // AssistantScrollView:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  gfx::Size GetMinimumSize() const override;
  void OnContentsPreferredSizeChanged(views::View* content_view) override;
  void PreferredSizeChanged() override;

  // AssistantInteractionModelObserver:
  void OnCommittedQueryChanged(const AssistantQuery& query) override;
  void OnResponseChanged(
      const std::shared_ptr<AssistantResponse>& response) override;
  void OnResponseCleared() override;

 private:
  void InitLayout();

  void OnResponseAdded(std::shared_ptr<const AssistantResponse> response);
  void OnCardElementAdded(const AssistantCardElement* card_element);
  void OnTextElementAdded(const AssistantTextElement* text_element);
  void OnAllUiElementsAdded();

  // Sets whether or not PreferredSizeChanged events should be propagated.
  void SetPropagatePreferredSizeChanged(bool propagate);

  AssistantViewDelegate* const delegate_;

  // Shared pointers to the response that is currently on stage as well as the
  // pending response to be presented following the former's animated exit. We
  // use shared pointers to ensure that underlying UI elements are not destroyed
  // before we have an opportunity to remove their associated views.
  std::shared_ptr<const AssistantResponse> response_;
  std::shared_ptr<const AssistantResponse> pending_response_;

  // Whether we should allow propagation of PreferredSizeChanged events. Because
  // we only animate views in/out in batches, we can prevent over-propagation of
  // PreferredSizeChanged events by waiting until the entirety of a response has
  // been added/removed before propagating. This reduces layout passes.
  bool propagate_preferred_size_changed_ = true;

  // UI elements will be animated on their own layers. We track the desired
  // opacity to which each layer should be animated when processing the next
  // query response.
  std::vector<std::pair<ui::LayerOwner*, float>> ui_element_views_;

  // Whether or not the card we are adding is the first card for the current
  // Assistant response. The first card requires the addition of a top margin.
  bool is_first_card_ = true;

  base::WeakPtrFactory<UiElementContainerView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UiElementContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_
