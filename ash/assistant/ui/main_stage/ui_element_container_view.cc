// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/ui_element_container_view.h"

#include <string>

#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/main_stage/assistant_card_element_view.h"
#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kEmbeddedUiFirstCardMarginTopDip = 8;
constexpr int kEmbeddedUiPaddingBottomDip = 8;
constexpr int kMainUiFirstCardMarginTopDip = 40;
constexpr int kMainUiPaddingBottomDip = 24;

// Card element animation.
constexpr float kCardElementAnimationFadeOutOpacity = 0.26f;

// Text element animation.
constexpr float kEmbeddedUiTextElementAnimationFadeOutOpacity = 0.26f;
constexpr float kMainUiTextElementAnimationFadeOutOpacity = 0.f;

// UI element animation.
constexpr base::TimeDelta kUiElementAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kUiElementAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kUiElementAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(167);

// Helpers ---------------------------------------------------------------------

int GetFirstCardMarginTopDip() {
  return app_list_features::IsEmbeddedAssistantUIEnabled()
             ? kEmbeddedUiFirstCardMarginTopDip
             : kMainUiFirstCardMarginTopDip;
}

int GetPaddingBottomDip() {
  return app_list_features::IsEmbeddedAssistantUIEnabled()
             ? kEmbeddedUiPaddingBottomDip
             : kMainUiPaddingBottomDip;
}

float GetTextElementAnimationFadeOutOpacity() {
  return app_list_features::IsEmbeddedAssistantUIEnabled()
             ? kEmbeddedUiTextElementAnimationFadeOutOpacity
             : kMainUiTextElementAnimationFadeOutOpacity;
}

}  // namespace

// UiElementContainerView ------------------------------------------------------

UiElementContainerView::UiElementContainerView(AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  InitLayout();

  // The AssistantViewDelegate should outlive UiElementContainerView.
  delegate_->AddInteractionModelObserver(this);
}

UiElementContainerView::~UiElementContainerView() {
  delegate_->RemoveInteractionModelObserver(this);
}

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

void UiElementContainerView::PreferredSizeChanged() {
  // Because views are added/removed in batches, we attempt to prevent over-
  // propagation of the PreferredSizeChanged event during batched view hierarchy
  // add/remove operations. This helps to reduce layout passes.
  if (propagate_preferred_size_changed_)
    AssistantScrollView::PreferredSizeChanged();
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
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;

  // We don't allow processing of events while waiting for the next query
  // response. The contents will be faded out, so it should not be interactive.
  // We also scroll to the top to play nice with the transition animation.
  set_can_process_events_within_subtree(false);
  ScrollToPosition(vertical_scroll_bar(), 0);

  // When a query is committed, we fade out the views for the previous response
  // until the next Assistant response has been received.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    pair.first->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(CreateOpacityElement(
            /*opacity=*/pair.second, kUiElementAnimationFadeOutDuration)));
  }
}

void UiElementContainerView::OnResponseChanged(
    const std::shared_ptr<AssistantResponse>& response) {
  // We may have to pend the response while we animate the previous response off
  // stage. We use a shared pointer to ensure that any views we add to the view
  // hierarchy can be removed before the underlying UI elements are destroyed.
  pending_response_ = std::shared_ptr<const AssistantResponse>(response);

  // If we don't have any pre-existing content, there is nothing to animate off
  // stage so we we can proceed to add the new response.
  if (content_view()->children().empty()) {
    OnResponseAdded(std::move(pending_response_));
    return;
  }

  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;
  using assistant::util::StartLayerAnimationSequence;

  // There is a previous response on stage, so we'll animate it off before
  // adding the new response. The new response will be added upon invocation of
  // the exit animation ended callback.
  auto* exit_animation_observer = new ui::CallbackLayerAnimationObserver(
      /*animation_ended_callback=*/base::BindRepeating(
          [](const base::WeakPtr<UiElementContainerView>& weak_ptr,
             const ui::CallbackLayerAnimationObserver& observer) {
            // If the UiElementContainerView is destroyed we just return true
            // to delete our observer. No futher action is needed.
            if (!weak_ptr)
              return true;

            // If the exit animation was aborted, we just return true to delete
            // our observer. No further action is needed.
            if (observer.aborted_count())
              return true;

            // All UI elements have finished their exit animations so it's safe
            // to perform clearing of their views and managed resources.
            weak_ptr->OnResponseCleared();

            // It is safe to add our pending response, if one exists, to the
            // view hierarchy now that we've cleared the previous response
            // from the stage.
            if (weak_ptr->pending_response_)
              weak_ptr->OnResponseAdded(std::move(weak_ptr->pending_response_));

            // We return true to delete our observer.
            return true;
          },
          weak_factory_.GetWeakPtr()));

  // Animate the layer belonging to each view in the previous response.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    StartLayerAnimationSequence(
        pair.first->layer()->GetAnimator(),
        // Fade out the opacity to 0%. Note that we approximate 0% by actually
        // using 0.01%. We do this to workaround a DCHECK that requires
        // aura::Windows to have a target opacity > 0% when shown. Because our
        // window will be removed after it reaches this value, it should be safe
        // to circumnavigate this DCHECK.
        CreateLayerAnimationSequence(
            CreateOpacityElement(0.0001f, kUiElementAnimationFadeOutDuration)),
        // Observe the animation.
        exit_animation_observer);
  }

  // Set the observer to active so that we receive callback events.
  exit_animation_observer->SetActive();
}

void UiElementContainerView::OnResponseCleared() {
  // We explicitly abort all in progress animations here because we will remove
  // their views immediately and we want to ensure that any animation observers
  // will be notified of an abort, not an animation completion. Otherwise there
  // is potential to enter into a bad state (see crbug/952996).
  for (auto& ui_element_view : ui_element_views_)
    ui_element_view.first->layer()->GetAnimator()->AbortAllAnimations();

  // We can prevent over-propagation of the PreferredSizeChanged event by
  // stopping propagation during batched view hierarchy add/remove operations.
  SetPropagatePreferredSizeChanged(false);
  content_view()->RemoveAllChildViews(/*delete_children=*/true);
  ui_element_views_.clear();
  SetPropagatePreferredSizeChanged(true);

  // Once the response has been cleared from the stage, we can are free to
  // release our shared pointer. This allows resources associated with the
  // underlying UI elements to be freed, provided there are no other usages.
  response_.reset();

  // Reset state for the next response.
  is_first_card_ = true;
}

void UiElementContainerView::OnResponseAdded(
    std::shared_ptr<const AssistantResponse> response) {
  // The response should be fully processed before it is presented.
  DCHECK_EQ(AssistantResponse::ProcessingState::kProcessed,
            response->processing_state());

  // We cache a reference to the |response| to ensure that the instance is not
  // destroyed before we have removed associated views from the view hierarchy.
  response_ = std::move(response);

  // Because the views for the response are animated in together, we can stop
  // propagation of PreferredSizeChanged events until all views have been added
  // to the view hierarchy to reduce layout passes.
  SetPropagatePreferredSizeChanged(false);

  for (const auto& ui_element : response_->GetUiElements()) {
    switch (ui_element->GetType()) {
      case AssistantUiElementType::kCard:
        OnCardElementAdded(
            static_cast<const AssistantCardElement*>(ui_element.get()));
        break;
      case AssistantUiElementType::kText:
        OnTextElementAdded(
            static_cast<const AssistantTextElement*>(ui_element.get()));
        break;
    }
  }

  OnAllUiElementsAdded();
}

void UiElementContainerView::OnCardElementAdded(
    const AssistantCardElement* card_element) {
  // The card, for some reason, is not embeddable so we'll have to ignore it.
  if (!card_element->contents())
    return;

  auto* card_element_view =
      new AssistantCardElementView(delegate_, card_element);
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
  // layer setup. We're going to fade the view in, so hide it. Note that we
  // approximate 0% opacity by actually using 0.01%. We do this to workaround
  // a DCHECK that requires aura::Windows to have a target opacity > 0% when
  // shown. Because our window will be animated to full opacity from this
  // value, it should be safe to circumnavigate this DCHECK.
  card_element_view->native_view()->layer()->SetFillsBoundsOpaquely(false);
  card_element_view->native_view()->layer()->SetOpacity(0.0001f);

  // We cache the native view for use during animations and its desired
  // opacity that we'll animate to while processing the next query response.
  ui_element_views_.push_back(std::pair<ui::LayerOwner*, float>(
      card_element_view->native_view(), kCardElementAnimationFadeOutOpacity));
}

void UiElementContainerView::OnTextElementAdded(
    const AssistantTextElement* text_element) {
  views::View* text_element_view = new AssistantTextElementView(text_element);

  // The view will be animated on its own layer, so we need to do some initial
  // layer setup. We're going to fade the view in, so hide it.
  text_element_view->SetPaintToLayer();
  text_element_view->layer()->SetFillsBoundsOpaquely(false);
  text_element_view->layer()->SetOpacity(0.f);

  // We cache the view for use during animations and its desired opacity that
  // we'll animate to while processing the next query response.
  ui_element_views_.push_back(std::pair<ui::LayerOwner*, float>(
      text_element_view, GetTextElementAnimationFadeOutOpacity()));

  content_view()->AddChildView(text_element_view);
}

void UiElementContainerView::OnAllUiElementsAdded() {
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;

  // Now that the response for the current query has been added to the view
  // hierarchy, we can re-enable processing of events. We can also restart
  // propagation of PreferredSizeChanged events since all views have been added
  // to the view hierarchy.
  set_can_process_events_within_subtree(true);
  SetPropagatePreferredSizeChanged(true);

  // Now that we've received and added all UI elements for the current query
  // response, we can animate them in.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    // We fade in the views to full opacity after a slight delay.
    pair.first->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                kUiElementAnimationFadeInDelay),
            CreateOpacityElement(1.f, kUiElementAnimationFadeInDuration)));
  }

  // Let screen reader read the query result. This includes the text response
  // and the card fallback text, but webview result is not included.
  // We don't read when there is TTS to avoid speaking over the server response.
  const AssistantResponse* response =
      delegate_->GetInteractionModel()->response();
  if (!response->has_tts())
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

void UiElementContainerView::SetPropagatePreferredSizeChanged(bool propagate) {
  if (propagate == propagate_preferred_size_changed_)
    return;

  propagate_preferred_size_changed_ = propagate;

  // When we are no longer stopping propagation of PreferredSizeChanged events,
  // we fire an event off to ensure the view hierarchy is properly laid out.
  if (propagate_preferred_size_changed_)
    PreferredSizeChanged();
}

}  // namespace ash
