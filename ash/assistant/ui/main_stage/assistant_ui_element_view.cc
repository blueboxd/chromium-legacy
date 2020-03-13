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
using assistant::util::StartLayerAnimationSequence;

// Animation.
constexpr base::TimeDelta kAnimateInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kAnimateOutDuration =
    base::TimeDelta::FromMilliseconds(200);

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
    StartLayerAnimationSequence(
        layer()->GetAnimator(),
        CreateLayerAnimationSequence(CreateOpacityElement(
            1.f, kAnimateInDuration, gfx::Tween::Type::FAST_OUT_SLOW_IN)),
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
