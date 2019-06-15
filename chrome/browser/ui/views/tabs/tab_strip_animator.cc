// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_animator.h"

#include "base/time/time.h"

// TODO(965227): align animation ticks to compositor events.
constexpr base::TimeDelta kTickInterval =
    base::TimeDelta::FromMilliseconds(1000 / 60.0);

TabStripAnimator::TabStripAnimator(
    base::RepeatingClosure on_animation_progressed_callback)
    : on_animation_progressed_callback_(on_animation_progressed_callback) {}

TabStripAnimator::~TabStripAnimator() = default;

std::vector<TabAnimationState> TabStripAnimator::GetCurrentTabStates() const {
  std::vector<TabAnimationState> result;
  for (const TabAnimation& animation : animations_) {
    result.push_back(animation.GetCurrentState());
  }
  return result;
}

bool TabStripAnimator::IsAnimating() const {
  return timer_.IsRunning();
}

void TabStripAnimator::InsertTabAtNoAnimation(
    int index,
    base::OnceClosure tab_removed_callback,
    TabAnimationState::TabActiveness active,
    TabAnimationState::TabPinnedness pinned) {
  animations_.insert(
      animations_.begin() + index,
      TabAnimation::ForStaticState(
          TabAnimationState::ForIdealTabState(
              TabAnimationState::TabOpenness::kOpen, pinned, active, 0),
          std::move(tab_removed_callback)));
}

void TabStripAnimator::MoveTabNoAnimation(int prev_index, int new_index) {
  TabAnimation moved_animation = std::move(animations_[prev_index]);
  animations_.erase(animations_.begin() + prev_index);
  animations_.insert(animations_.begin() + new_index,
                     std::move(moved_animation));
}

void TabStripAnimator::RemoveTabNoAnimation(int index) {
  animations_.erase(animations_.begin() + index);
}

void TabStripAnimator::SetPinnednessNoAnimation(
    int index,
    TabAnimationState::TabPinnedness pinned) {
  animations_[index].AnimateTo(
      animations_[index].target_state().WithPinnedness(pinned));
  animations_[index].CompleteAnimation();
}

void TabStripAnimator::InsertTabAt(int index,
                                   base::OnceClosure tab_removed_callback,
                                   TabAnimationState::TabActiveness active,
                                   TabAnimationState::TabPinnedness pinned) {
  animations_.insert(
      animations_.begin() + index,
      TabAnimation::ForStaticState(
          TabAnimationState::ForIdealTabState(
              TabAnimationState::TabOpenness::kClosed, pinned, active, 0),
          std::move(tab_removed_callback)));
  AnimateTabTo(index, animations_[index].target_state().WithOpenness(
                          TabAnimationState::TabOpenness::kOpen));
}

void TabStripAnimator::RemoveTab(int index) {
  AnimateTabTo(index, animations_[index].target_state().WithOpenness(
                          TabAnimationState::TabOpenness::kClosed));
}

void TabStripAnimator::SetActiveTab(int prev_active_index,
                                    int new_active_index) {
  // Set activeness without animating by retargeting the existing animation.
  if (prev_active_index >= 0) {
    animations_[prev_active_index].RetargetTo(
        animations_[prev_active_index].target_state().WithActiveness(
            TabAnimationState::TabActiveness::kInactive));
  }
  if (new_active_index >= 0) {
    animations_[new_active_index].RetargetTo(
        animations_[new_active_index].target_state().WithActiveness(
            TabAnimationState::TabActiveness::kActive));
  }
}

void TabStripAnimator::CompleteAnimations() {
  CompleteAnimationsWithoutDestroyingTabs();
  RemoveClosedTabs();
}

void TabStripAnimator::CompleteAnimationsWithoutDestroyingTabs() {
  for (TabAnimation& animation : animations_) {
    animation.CompleteAnimation();
  }
  timer_.Stop();
}

void TabStripAnimator::AnimateTabTo(int index, TabAnimationState target_state) {
  animations_[index].AnimateTo(target_state);
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, kTickInterval, this,
                 &TabStripAnimator::TickAnimations);
    // Tick animations immediately so that the animation starts from the
    // beginning instead of kTickInterval ms into the animation.
    TickAnimations();
  }
}

void TabStripAnimator::TickAnimations() {
  bool all_animations_completed = true;
  for (TabAnimation& animation : animations_) {
    all_animations_completed &= animation.GetTimeRemaining().is_zero();
  }
  RemoveClosedTabs();

  if (all_animations_completed) {
    timer_.Stop();
  }

  on_animation_progressed_callback_.Run();
}

void TabStripAnimator::RemoveClosedTabs() {
  for (auto it = animations_.begin(); it != animations_.end();) {
    if (it->GetTimeRemaining().is_zero() &&
        it->GetCurrentState().IsFullyClosed()) {
      it->NotifyCloseCompleted();
      it = animations_.erase(it);
    } else {
      it++;
    }
  }
}
