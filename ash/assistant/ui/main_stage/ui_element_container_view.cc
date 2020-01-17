// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/ui_element_container_view.h"

#include <string>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/ui/assistant_card_element.h"
#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/animated_container_view.h"
#include "ash/assistant/ui/main_stage/assistant_card_element_view.h"
#include "ash/assistant/ui/main_stage/assistant_ui_element_view_factory.h"
#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kEmbeddedUiFirstCardMarginTopDip = 8;
constexpr int kEmbeddedUiPaddingBottomDip = 8;
constexpr int kMainUiFirstCardMarginTopDip = 40;
constexpr int kMainUiPaddingBottomDip = 24;

// Helpers ---------------------------------------------------------------------

int GetFirstCardMarginTopDip() {
  return app_list_features::IsAssistantLauncherUIEnabled()
             ? kEmbeddedUiFirstCardMarginTopDip
             : kMainUiFirstCardMarginTopDip;
}

int GetPaddingBottomDip() {
  return app_list_features::IsAssistantLauncherUIEnabled()
             ? kEmbeddedUiPaddingBottomDip
             : kMainUiPaddingBottomDip;
}

}  // namespace

// UiElementContainerView ------------------------------------------------------

UiElementContainerView::UiElementContainerView(AssistantViewDelegate* delegate)
    : AnimatedContainerView(delegate),
      view_factory_(std::make_unique<AssistantUiElementViewFactory>(delegate)) {
  SetID(AssistantViewID::kUiElementContainer);
  InitLayout();
}

UiElementContainerView::~UiElementContainerView() = default;

const char* UiElementContainerView::GetClassName() const {
  return "UiElementContainerView";
}

gfx::Size UiElementContainerView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int UiElementContainerView::GetHeightForWidth(int width) const {
  return content_view()->GetHeightForWidth(width);
}

gfx::Size UiElementContainerView::GetMinimumSize() const {
  // AssistantMainStage uses BoxLayout's flex property to grow/shrink
  // UiElementContainerView to fill available space as needed. When height is
  // shrunk to zero, as is temporarily the case during the initial container
  // growth animation for the first Assistant response, UiElementContainerView
  // will be laid out with zero width. We do not recover from this state until
  // the next layout pass, which causes Assistant cards for the first response
  // to be laid out with zero width. We work around this by imposing a minimum
  // height restriction of 1 dip that is factored into BoxLayout's flex
  // calculations to make sure that our width is never being set to zero.
  return gfx::Size(INT_MAX, 1);
}

void UiElementContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  const int preferred_height = content_view->GetHeightForWidth(width());
  content_view->SetSize(gfx::Size(width(), preferred_height));
}

void UiElementContainerView::InitLayout() {
  content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kUiElementHorizontalMarginDip, GetPaddingBottomDip(),
                  kUiElementHorizontalMarginDip),
      kSpacingDip));
}

void UiElementContainerView::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  // Scroll to the top to play nice with the transition animation.
  ScrollToPosition(vertical_scroll_bar(), 0);

  AnimatedContainerView::OnCommittedQueryChanged(query);
}

void UiElementContainerView::HandleResponse(const AssistantResponse& response) {
  for (const auto& ui_element : response.GetUiElements()) {
    // TODO(dmblack): Remove after deprecating standalone UI.
    if (ui_element->type() == AssistantUiElementType::kCard) {
      OnCardElementAdded(
          static_cast<const AssistantCardElement*>(ui_element.get()));
      continue;
    }
    // Add a new view for the |ui_element| to the view hierarchy, bind an
    // animator to handle all of its animations, and prepare its animation layer
    // for the initial fade-in.
    auto view = view_factory_->Create(ui_element.get());
    auto* view_ptr = content_view()->AddChildView(std::move(view));
    AddElementAnimator(view_ptr->CreateAnimator());
    view_ptr->GetLayerForAnimating()->SetOpacity(0.f);
  }
}

// TODO(dmblack): Remove after deprecating standalone UI.
void UiElementContainerView::OnCardElementAdded(
    const AssistantCardElement* card_element) {
  // The card, for some reason, is not embeddable so we'll have to ignore it.
  if (!card_element->contents_view())
    return;

  auto* card_element_view =
      new AssistantCardElementView(delegate(), card_element);
  if (is_first_card_) {
    is_first_card_ = false;

    // The first card requires a top margin of |GetFirstCardMarginTopDip()|, but
    // we need to account for child spacing because the first card is not
    // necessarily the first UI element.
    const int top_margin_dip =
        GetFirstCardMarginTopDip() - (children().empty() ? 0 : kSpacingDip);

    // We effectively create a top margin by applying an empty border.
    card_element_view->SetBorder(
        views::CreateEmptyBorder(top_margin_dip, 0, 0, 0));
  }

  content_view()->AddChildView(card_element_view);

  // The view will be animated on its own layer, so we need to do some initial
  // layer setup. We're going to fade the view in, so hide it.
  card_element_view->native_view()->layer()->SetFillsBoundsOpaquely(false);
  card_element_view->native_view()->layer()->SetOpacity(0.f);

  // We set the animator to handle all animations for this view.
  AddElementAnimator(card_element_view->CreateAnimator());
}

void UiElementContainerView::OnAllViewsRemoved() {
  // Reset state for the next response.
  is_first_card_ = true;
}

void UiElementContainerView::OnAllViewsAnimatedIn() {
  // Let screen reader read the query result. This includes the text response
  // and the card fallback text, but webview result is not included.
  // We don't read when there is TTS to avoid speaking over the server response.
  const AssistantResponse* response =
      delegate()->GetInteractionModel()->response();
  DCHECK(response);
  if (!response->has_tts())
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

}  // namespace ash
