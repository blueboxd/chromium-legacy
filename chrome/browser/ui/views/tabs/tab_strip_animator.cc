// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_animator.h"

#include <utility>

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

TabAnimation::ViewType TabStripAnimator::GetAnimationViewTypeAt(
    int index) const {
  return animations_.at(index).view_type();
}

void TabStripAnimator::InsertTabAtNoAnimation(
    TabAnimation::ViewType view_type,
    int index,
    base::OnceClosure tab_removed_callback,
    TabAnimationState::TabActiveness active,
    TabAnimationState::TabPinnedness pinned) {
  animations_.insert(
      animations_.begin() + index,
      TabAnimation::ForStaticState(
          view_type,
          TabAnimationState::ForIdealTabState(
              TabAnimationState::TabOpenness::kOpen, pinned, active, 0),
          std::move(tab_removed_callback)));
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

void TabStripAnimator::MoveTabsNoAnimation(std::vector<int> moving_tabs,
                                           int new_index) {
  DCHECK_GT(moving_tabs.size(), 0u);
  std::vector<TabAnimation> moved_animations;
  // Remove the rightmost tab first to avoid perturbing the indices of the other
  // tabs in the list.
  std::reverse(std::begin(moving_tabs), std::end(moving_tabs));
  for (int tab : moving_tabs) {
    moved_animations.push_back(std::move(animations_[tab]));
    animations_.erase(animations_.begin() + tab);
  }
  // Insert the leftmost tab first, for the same reason.
  std::reverse(std::begin(moved_animations), std::end(moved_animations));
  for (size_t i = 0; i < moved_animations.size(); i++) {
    animations_.insert(animations_.begin() + new_index + i,
                       std::move(moved_animations[i]));
  }
}

void TabStripAnimator::InsertTabAt(TabAnimation::ViewType view_type,
                                   int index,
                                   base::OnceClosure tab_removed_callback,
                                   TabAnimationState::TabActiveness active,
                                   TabAnimationState::TabPinnedness pinned) {
  animations_.insert(
      animations_.begin() + index,
      TabAnimation::ForStaticState(
          view_type,
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
