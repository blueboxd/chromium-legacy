// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"

#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/assistant/util/animation_util.h"
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

// Animation.
constexpr base::TimeDelta kAnimateOutDuration =
    base::TimeDelta::FromMilliseconds(200);
constexpr base::TimeDelta kFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kTranslateUpDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr int kTranslateUpDistanceDip = 32;

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
    // As part of the animation we will translate the element up from the
    // bottom so we need to start by translating it down.
    TranslateDown();
    StartLayerAnimationSequencesTogether(layer()->GetAnimator(),
                                         {
                                             CreateFadeInAnimation(),
                                             CreateTranslateUpAnimation(),
                                         },
                                         observer);
  }

  void AnimateOut(ui::CallbackLayerAnimationObserver* observer) override {
    StartLayerAnimationSequence(
        layer()->GetAnimator(),
        CreateLayerAnimationSequence(CreateOpacityElement(
            kMinimumAnimateOutOpacity, kAnimateOutDuration)),
        observer);
  }

  ui::Layer* layer() const override { return view_->GetLayerForAnimating(); }

 private:
  void TranslateDown() const {
    gfx::Transform transform;
    transform.Translate(0, kTranslateUpDistanceDip);
    layer()->SetTransform(transform);
  }

  ui::LayerAnimationSequence* CreateFadeInAnimation() const {
    return CreateLayerAnimationSequence(CreateOpacityElement(
        1.f, kFadeInDuration, gfx::Tween::Type::FAST_OUT_SLOW_IN));
  }

  ui::LayerAnimationSequence* CreateTranslateUpAnimation() const {
    return CreateLayerAnimationSequence(
        CreateTransformElement(gfx::Transform(), kTranslateUpDuration,
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
