// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_ANIMATOR_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_ANIMATOR_H_

#include <vector>

#include "base/callback.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/tabs/tab_animation.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"

// Animates the tabs in the tabstrip. Must be kept abreast of non-
// animated changes to tabs as well via the *NoAnimation methods.
class TabStripAnimator {
 public:
  explicit TabStripAnimator(base::RepeatingClosure on_animation_progressed);
  ~TabStripAnimator();

  size_t animation_count() const { return animations_.size(); }

  std::vector<TabAnimationState> GetCurrentTabStates() const;

  // Returns whether any animations in progress.
  bool IsAnimating() const;

  TabAnimation::ViewType GetAnimationViewTypeAt(int index) const;

  // Inserts, without animation, a new tab at |index|.
  // |tab_removed_callback| will be invoked if the tab is removed
  // at the end of a remove animation.
  void InsertTabAtNoAnimation(TabAnimation::ViewType view_type,
                              int index,
                              base::OnceClosure tab_removed_callback,
                              TabAnimationState::TabActiveness active,
                              TabAnimationState::TabPinnedness pinned);

  // Removes the tab at |index| from the strip without playing any animation.
  // Does not invoke the associated |tab_removed_callback|.
  void RemoveTabNoAnimation(int index);

  // Sets the tab at |index|'s pinnedness to |pinnedness|. TODO(958173): Should
  // be animated.
  void SetPinnednessNoAnimation(int index,
                                TabAnimationState::TabPinnedness pinnedness);

  // Moves the tabs in |moving_tabs| to |new_index|. The vector of tabs should
  // be sorted by increasing index. TODO(958173): Should be animated.
  void MoveTabsNoAnimation(std::vector<int> moving_tabs, int new_index);

  // Animates the insertion of a new tab at |index|.
  // |tab_removed_callback| will be invoked if the tab is removed
  // at the end of a remove animation.
  void InsertTabAt(TabAnimation::ViewType view_type,
                   int index,
                   base::OnceClosure tab_removed_callback,
                   TabAnimationState::TabActiveness active,
                   TabAnimationState::TabPinnedness pinned);

  // Animates the tab being removed from the strip. That animation will
  // invoke the associated |tab_removed_callback| when complete.
  void RemoveTab(int index);

  // Changes the active tab from |prev_active_index| to |new_active_index|.
  void SetActiveTab(int prev_active_index, int new_active_index);

  // Finishes all in-progress animations.
  void CompleteAnimations();

  // TODO(958173): Temporary method that completes running animations,
  // without invoking the callback to destroy removed tabs. Use to hand
  // off animation (and removed tab destruction) responsibilities from
  // this animator to elsewhere without teleporting tabs or destroying
  // the same tab more than once.
  void CompleteAnimationsWithoutDestroyingTabs();

 private:
  void AnimateTabTo(int index, TabAnimationState target_state);
  void TickAnimations();
  void RemoveClosedTabs();

  base::RepeatingClosure on_animation_progressed_callback_;
  std::vector<TabAnimation> animations_;

  base::RepeatingTimer timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_ANIMATOR_H_
