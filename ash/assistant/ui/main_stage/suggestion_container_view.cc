// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/animated_container_view.h"
#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/assistant/util/assistant_util.h"
#include "base/bind.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

using assistant::util::CreateLayerAnimationSequence;
using assistant::util::CreateOpacityElement;
using assistant::util::CreateTransformElement;
using assistant::util::StartLayerAnimationSequence;
using assistant::util::StartLayerAnimationSequencesTogether;

// Animation.
constexpr int kChipMoveUpDistanceDip = 24;
constexpr base::TimeDelta kSelectedChipAnimateInDelay =
    base::TimeDelta::FromMilliseconds(150);
constexpr base::TimeDelta kChipFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kChipMoveUpDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kChipFadeOutDuration =
    base::TimeDelta::FromMilliseconds(200);

// Appearance.
constexpr int kPreferredHeightDip = 48;

}  // namespace

// SuggestionChipAnimator -----------------------------------------------------

class SuggestionChipAnimator : public ElementAnimator {
 public:
  SuggestionChipAnimator(SuggestionChipView* chip,
                         const SuggestionContainerView* parent)
      : ElementAnimator(chip), parent_(parent) {}
  ~SuggestionChipAnimator() override = default;

  void AnimateIn(ui::CallbackLayerAnimationObserver* observer) override {
    // As part of the animation we will move up the chip from the bottom
    // so we need to start by moving it down.
    MoveDown();
    layer()->SetOpacity(0.f);

    StartLayerAnimationSequencesTogether(layer()->GetAnimator(),
                                         {
                                             CreateFadeInAnimation(),
                                             CreateMoveUpAnimation(),
                                         },
                                         observer);
  }

  void AnimateOut(ui::CallbackLayerAnimationObserver* observer) override {
    StartLayerAnimationSequence(layer()->GetAnimator(),
                                CreateAnimateOutAnimation(), observer);
  }

  void FadeOut(ui::CallbackLayerAnimationObserver* observer) override {
    // If the user pressed a chip we do not fade it out.
    if (!IsSelectedChip())
      ElementAnimator::FadeOut(observer);
  }

 private:
  bool IsSelectedChip() const { return view() == parent_->selected_chip(); }

  void MoveDown() const {
    gfx::Transform transform;
    transform.Translate(0, kChipMoveUpDistanceDip);
    layer()->SetTransform(transform);
  }

  ui::LayerAnimationSequence* CreateFadeInAnimation() const {
    return CreateLayerAnimationSequence(
        ui::LayerAnimationElement::CreatePauseElement(
            ui::LayerAnimationElement::AnimatableProperty::OPACITY,
            kSelectedChipAnimateInDelay),
        CreateOpacityElement(1.f, kChipFadeInDuration,
                             gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  ui::LayerAnimationSequence* CreateMoveUpAnimation() const {
    return CreateLayerAnimationSequence(
        ui::LayerAnimationElement::CreatePauseElement(
            ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
            kSelectedChipAnimateInDelay),
        CreateTransformElement(gfx::Transform(), kChipMoveUpDuration,
                               gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  ui::LayerAnimationSequence* CreateAnimateOutAnimation() const {
    return CreateLayerAnimationSequence(CreateOpacityElement(
        0.f, kChipFadeOutDuration, gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  const SuggestionContainerView* const parent_;  // |parent_| owns |this|.

  DISALLOW_COPY_AND_ASSIGN(SuggestionChipAnimator);
};

// SuggestionContainerView -----------------------------------------------------

SuggestionContainerView::SuggestionContainerView(
    AssistantViewDelegate* delegate)
    : AnimatedContainerView(delegate) {
  SetID(AssistantViewID::kSuggestionContainer);
  InitLayout();

  // The AssistantViewDelegate should outlive SuggestionContainerView.
  delegate->AddSuggestionsModelObserver(this);
  delegate->AddUiModelObserver(this);
}

SuggestionContainerView::~SuggestionContainerView() {
  delegate()->RemoveUiModelObserver(this);
  delegate()->RemoveSuggestionsModelObserver(this);
  delegate()->RemoveInteractionModelObserver(this);
}

const char* SuggestionContainerView::GetClassName() const {
  return "SuggestionContainerView";
}

gfx::Size SuggestionContainerView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int SuggestionContainerView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void SuggestionContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  // Our contents should never be smaller than our container width because when
  // showing conversation starters we will be center aligned.
  const int width =
      std::max(content_view->GetPreferredSize().width(), this->width());
  content_view->SetSize(gfx::Size(width, kPreferredHeightDip));
}

void SuggestionContainerView::InitLayout() {
  layout_manager_ =
      content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip), kSpacingDip));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // We center align when showing conversation starters.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
}

void SuggestionContainerView::OnConversationStartersChanged(
    const std::vector<const AssistantSuggestion*>& conversation_starters) {
  // If we've received a response we should ignore changes to the cache of
  // conversation starters as we are past the state in which they should be
  // presented. To present them now would incorrectly associate the conversation
  // starters with a response.
  if (has_received_response_)
    return;

  RemoveAllViews();

  std::vector<std::unique_ptr<ElementAnimator>> animators;
  for (const auto* conversation_starter : conversation_starters) {
    auto animator = AddSuggestionChip(conversation_starter);
    if (animator)
      AddElementAnimator(std::move(animator));
  }

  AnimateIn();
}

std::unique_ptr<ElementAnimator> SuggestionContainerView::HandleSuggestion(
    const AssistantSuggestion* suggestion) {
  has_received_response_ = true;

  // When no longer showing conversation starters, we start align our content.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  return AddSuggestionChip(suggestion);
}

void SuggestionContainerView::OnAllViewsRemoved() {
  // Clear the selected button.
  selected_chip_ = nullptr;

  // Note that we don't reset |has_received_response_| here because that refers
  // to whether we've received a response during the current Assistant session,
  // not whether we are currently displaying a response.
}

std::unique_ptr<ElementAnimator> SuggestionContainerView::AddSuggestionChip(
    const AssistantSuggestion* suggestion) {
  auto suggestion_chip_view = std::make_unique<SuggestionChipView>(
      delegate(), suggestion, /*listener=*/this);

  // The chip will be animated on its own layer.
  suggestion_chip_view->SetPaintToLayer();
  suggestion_chip_view->layer()->SetFillsBoundsOpaquely(false);

  // Add to the view hierarchy and return the animator for the suggestion chip.
  return std::make_unique<SuggestionChipAnimator>(
      contents()->AddChildView(std::move(suggestion_chip_view)), this);
}

void SuggestionContainerView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  // Remember which chip was selected, so we can give it a special animation.
  selected_chip_ = static_cast<SuggestionChipView*>(sender);
  delegate()->OnSuggestionChipPressed(selected_chip_->suggestion());
}

void SuggestionContainerView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (assistant::util::IsStartingSession(new_visibility, old_visibility) &&
      entry_point.value() != AssistantEntryPoint::kLauncherSearchResult) {
    // Show conversation starters at the start of a new Assistant session except
    // when the user already started a query in Launcher quick search box (QSB).
    OnConversationStartersChanged(
        delegate()->GetSuggestionsModel()->GetConversationStarters());
    return;
  }

  if (!assistant::util::IsFinishingSession(new_visibility))
    return;

  // When Assistant is finishing a session, we need to reset view state.
  has_received_response_ = false;

  // When we start a new session we will be showing conversation starters so
  // we need to center align our content.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
}

}  // namespace ash
