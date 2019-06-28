// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_

#include <map>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "chrome/browser/ui/views/tabs/tab_strip_animator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_model.h"

class Tab;
class TabGroupHeader;
class TabGroupId;
class TabStripController;

// Helper class for TabStrip, that is responsible for calculating and assigning
// layouts for tabs and group headers. It tracks animations and changes to the
// model so that it has all necessary information for layout purposes.
class TabStripLayoutHelper {
 public:
  using GetTabsCallback = base::RepeatingCallback<views::ViewModelT<Tab>*()>;
  using GetGroupHeadersCallback =
      base::RepeatingCallback<std::map<TabGroupId, TabGroupHeader*>()>;

  TabStripLayoutHelper(const TabStripController* controller,
                       GetTabsCallback get_tabs_callback,
                       GetGroupHeadersCallback get_group_headers_callback,
                       base::RepeatingClosure on_animation_progressed);
  ~TabStripLayoutHelper();

  // Returns whether any animations for tabs or group headers are in progress.
  bool IsAnimating() const;

  int active_tab_width() { return active_tab_width_; }
  int inactive_tab_width() { return inactive_tab_width_; }
  int first_non_pinned_tab_index() { return first_non_pinned_tab_index_; }
  int first_non_pinned_tab_x() { return first_non_pinned_tab_x_; }

  // Returns the number of pinned tabs in the tabstrip.
  int GetPinnedTabCount() const;

  // Inserts a new tab at |index|, without animation. |tab_removed_callback|
  // will be invoked if the tab is removed at the end of a remove animation.
  void InsertTabAtNoAnimation(int index,
                              base::OnceClosure tab_removed_callback,
                              TabAnimationState::TabActiveness active,
                              TabAnimationState::TabPinnedness pinned);

  // Inserts a new tab at |index|, with animation. |tab_removed_callback| will
  // be invoked if the tab is removed at the end of a remove animation.
  void InsertTabAt(int index,
                   base::OnceClosure tab_removed_callback,
                   TabAnimationState::TabActiveness active,
                   TabAnimationState::TabPinnedness pinned);

  // Removes the tab at |index|. TODO(958173): This should invoke the associated
  // |tab_removed_callback| but currently does not, as it would duplicate
  // TabStrip::RemoveTabDelegate::AnimationEnded.
  void RemoveTabAt(int index);

  // Moves the tab at |prev_index| with group |group_at_prev_index| to
  // |new_index|. Also updates the group header's location if necessary.
  void MoveTab(base::Optional<TabGroupId> group_at_prev_index,
               int prev_index,
               int new_index);

  // Sets the tab at |index|'s pinnedness to |pinnedness|.
  void SetTabPinnedness(int index, TabAnimationState::TabPinnedness pinnedness);

  // Inserts a new group header for |group|. |header_removed_callback| will be
  // invoked if the group is removed at the end of a remove animation.
  void InsertGroupHeader(TabGroupId group,
                         base::OnceClosure header_removed_callback);

  // Removes the group header for |group|. TODO(958173): This should invoke the
  // associated |header_removed_callback| but currently does not because
  // RemoveTabAt also does not, and they share codepaths.
  void RemoveGroupHeader(TabGroupId group);

  // Changes the active tab from |prev_active_index| to |new_active_index|.
  void SetActiveTab(int prev_active_index, int new_active_index);

  // Finishes all in-progress animations.
  void CompleteAnimations();

  // TODO(958173): Temporary method. Like CompleteAnimations, but does not call
  // any associated |tab_removed_callback| or |header_removed_callback|. See
  // comment on TabStripAnimator::CompleteAnimationsWithoutDestroyingTabs.
  void CompleteAnimationsWithoutDestroyingTabs();

  // Generates and sets the ideal bounds for the views in |tabs| and
  // |group_headers|. Updates the cached widths in |active_tab_width_| and
  // |inactive_tab_width_|.
  // TODO(958173): The notion of ideal bounds is going away. Delete this.
  void UpdateIdealBounds(int available_width);

  // Generates and sets the ideal bounds for |tabs|. Updates
  // the cached values in |first_non_pinned_tab_index_| and
  // |first_non_pinned_tab_x_|.
  // TODO(958173): The notion of ideal bounds is going away. Delete this.
  void UpdateIdealBoundsForPinnedTabs();

  // Lays out tabs and group headers to their current bounds. Returns the
  // x-coordinate of the trailing edge of the trailing-most tab.
  int LayoutTabs(int available_width);

 private:
  struct TabSlot;

  // Recalculate |cached_slots_|, called whenever state changes.
  void UpdateCachedTabSlots();

  // Finds the index of the TabAnimation in |animator_| for the tab at
  // |tab_model_index|.
  int AnimatorIndexForTab(int tab_model_index) const;

  // Finds the index of the TabAnimation in |animator_| for |group|.
  int AnimatorIndexForGroupHeader(TabGroupId group) const;

  // Compares |cached_slots_| to the TabAnimations in |animator_| and DCHECKs if
  // the TabAnimation::ViewType do not match. Prevents bugs that could cause the
  // wrong callback being run when a tab or group is deleted.
  void VerifyAnimationsMatchTabSlots() const;

  // Updates the value of either |active_tab_width_| or |inactive_tab_width_|,
  // as appropriate.
  void UpdateCachedTabWidth(int tab_index, int tab_width, bool active);

  // The owning tabstrip's controller.
  const TabStripController* const controller_;

  // Callbacks to get the necessary View objects from the owning tabstrip.
  GetTabsCallback get_tabs_callback_;
  GetGroupHeadersCallback get_group_headers_callback_;

  // Responsible for tracking the animations of tabs and group headers.
  TabStripAnimator animator_;

  // Tracks changes to the tab strip in order to properly collate group headers
  // with tabs.
  std::vector<TabSlot> cached_slots_;

  // The current widths of tabs. If the space for tabs is not evenly divisible
  // into these widths, the initial tabs in the strip will be 1 px larger.
  int active_tab_width_;
  int inactive_tab_width_;

  int first_non_pinned_tab_index_;
  int first_non_pinned_tab_x_;

  DISALLOW_COPY_AND_ASSIGN(TabStripLayoutHelper);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_
