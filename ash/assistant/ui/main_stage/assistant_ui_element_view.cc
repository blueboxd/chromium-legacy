// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"

#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"

namespace ash {

namespace {

using assistant::util::CreateLayerAnimationSequence;
using assistant::util::CreateOpacityElement;
using assistant::util::CreateTransformElement;
using assistant::util::StartLayerAnimationSequence;
using assistant::util::StartLayerAnimationSequencesTogether;

// Main UI animation.
constexpr base::TimeDelta kMainUiElementAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kMainUiElementAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kMainUiElementAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(167);

// Embedded UI animation.
constexpr base::TimeDelta kEmbeddedUiElementAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kEmbeddedUiElementAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(200);
constexpr base::TimeDelta kEmbeddedUiElementAnimationTranslateUpDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr int kEmbeddedUiElementAnimationTranslateUpDistanceDip = 32;

// AssistantUiElementViewAnimator ----------------------------------------------

class AssistantUiElementViewAnimator : public ElementAnimator {
 public:
  explicit AssistantUiElementViewAnimator(AssistantUiElementView* view)
      : ElementAnimator(view), view_(view) {}
  explicit AssistantUiElementViewAnimator(
      AssistantUiElementViewAnimator& copy) = delete;
  AssistantUiElementViewAnimator& operator=(
      AssistantUiElementViewAnimator& assign) = delete;
  ~AssistantUiElementViewAnimator() override = default;

  // ElementAnimator:
  void AnimateIn(ui::CallbackLayerAnimationObserver* observer) override {
    if (app_list_features::IsAssistantLauncherUIEnabled()) {
      // As part of the animation we will translate the element up from the
      // bottom so we need to start by translating it down.
      TranslateDown();
      StartLayerAnimationSequencesTogether(layer()->GetAnimator(),
                                           {
                                               CreateFadeInAnimation(),
                                               CreateTranslateUpAnimation(),
                                           },
                                           observer);
    } else {
      StartLayerAnimationSequence(
          layer()->GetAnimator(),
          CreateLayerAnimationSequence(
              ui::LayerAnimationElement::CreatePauseElement(
                  ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                  kMainUiElementAnimationFadeInDelay),
              CreateOpacityElement(1.f, kMainUiElementAnimationFadeInDuration)),
          observer);
    }
  }

  void AnimateOut(ui::CallbackLayerAnimationObserver* observer) override {
    if (app_list_features::IsAssistantLauncherUIEnabled()) {
      StartLayerAnimationSequence(
          layer()->GetAnimator(),
          CreateLayerAnimationSequence(
              CreateOpacityElement(kMinimumAnimateOutOpacity,
                                   kEmbeddedUiElementAnimationFadeOutDuration)),
          observer);
    } else {
      StartLayerAnimationSequence(
          layer()->GetAnimator(),
          CreateLayerAnimationSequence(CreateOpacityElement(
              kMinimumAnimateOutOpacity, kMainUiElementAnimationFadeOutDuration,
              gfx::Tween::Type::FAST_OUT_SLOW_IN)),
          observer);
    }
  }

  // TODO(dmblack): Remove this override after deprecating standalone UI. It
  // handles a one-off case for standalone UI that didn't seem worth abstracting
  // out given that standalone UI is soon to be removed from the code base.
  void FadeOut(ui::CallbackLayerAnimationObserver* observer) override {
    if (!app_list_features::IsAssistantLauncherUIEnabled() &&
        strcmp(view_->GetClassName(), "AssistantTextElementView") == 0) {
      // Text elements in standalone UI must fade out completely as the thinking
      // dots will appear in the location of the first text element.
      StartLayerAnimationSequence(
          layer()->GetAnimator(),
          assistant::util::CreateLayerAnimationSequence(
              assistant::util::CreateOpacityElement(0.f, kFadeOutDuration)),
          observer);
    } else {
      ElementAnimator::FadeOut(observer);
    }
  }

  ui::Layer* layer() const override { return view_->GetLayerForAnimating(); }

 private:
  void TranslateDown() const {
    DCHECK(app_list_features::IsAssistantLauncherUIEnabled());
    gfx::Transform transform;
    transform.Translate(0, kEmbeddedUiElementAnimationTranslateUpDistanceDip);
    layer()->SetTransform(transform);
  }

  ui::LayerAnimationSequence* CreateFadeInAnimation() const {
    DCHECK(app_list_features::IsAssistantLauncherUIEnabled());
    return CreateLayerAnimationSequence(
        CreateOpacityElement(1.f, kEmbeddedUiElementAnimationFadeInDuration,
                             gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  ui::LayerAnimationSequence* CreateTranslateUpAnimation() const {
    DCHECK(app_list_features::IsAssistantLauncherUIEnabled());
    return CreateLayerAnimationSequence(CreateTransformElement(
        gfx::Transform(), kEmbeddedUiElementAnimationTranslateUpDuration,
        gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  AssistantUiElementView* const view_;
};

}  // namespace

// AssistantUiElementView ------------------------------------------------------

AssistantUiElementView::AssistantUiElementView() = default;

AssistantUiElementView::~AssistantUiElementView() = default;

const char* AssistantUiElementView::GetClassName() const {
  return "AssistantUiElementView";
}

std::unique_ptr<ElementAnimator> AssistantUiElementView::CreateAnimator() {
  return std::make_unique<AssistantUiElementViewAnimator>(this);
}

}  // namespace ash
