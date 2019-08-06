// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_ANIMATION_STATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_ANIMATION_STATE_H_

#include <vector>

struct TabLayoutConstants;
struct TabSizeInfo;

// Contains the data necessary to determine the bounds of a tab even while
// it's in the middle of animating between states.  Immutable (except for
// replacement via assignment).
class TabAnimationState {
 public:
  enum class TabOpenness { kOpen, kClosed };

  enum class TabPinnedness { kPinned, kUnpinned };

  enum class TabActiveness { kActive, kInactive };

  // Returns the TabAnimationState that expresses the provided
  // ideal tab state. These correspond to the endpoints of animations.
  // |open| controls whether the returned TabAnimationState is fully open or
  // closed. |pinned| and |active| are analogous. |tab_index_offset| is the
  // distance, in tab indices, away from its current model position the tab
  // should be drawn at. It may be negative.
  static TabAnimationState ForIdealTabState(TabOpenness open,
                                            TabPinnedness pinned,
                                            TabActiveness active,
                                            int tab_index_offset);

  // Interpolates from |origin| to |target| by |value|.
  // |value| should be in the [0, 1] interval, where 0
  // corresponds to |origin| and 1 to |target|.
  static TabAnimationState Interpolate(float value,
                                       TabAnimationState origin,
                                       TabAnimationState target);

  TabAnimationState WithOpenness(TabOpenness open) const;
  TabAnimationState WithPinnedness(TabPinnedness pinned) const;
  TabAnimationState WithActiveness(TabActiveness active) const;

  // The smallest width this tab should ever have.
  float GetMinimumWidth(const TabLayoutConstants& layout_constants,
                        const TabSizeInfo& size_info) const;

  // The width this tab should have at the crossover point between the
  // tabstrip's two layout domains.  Above this width, inactive tabs have the
  // same width as active tabs.  Below this width, inactive tabs are smaller
  // than active tabs.
  float GetLayoutCrossoverWidth(const TabLayoutConstants& layout_constants,
                                const TabSizeInfo& size_info) const;

  // The width this tab would like to have, if space is available.
  float GetPreferredWidth(const TabLayoutConstants& layout_constants,
                          const TabSizeInfo& size_info) const;

  int GetLeadingEdgeOffset(std::vector<int> tab_widths, int my_index) const;

  bool IsFullyClosed() const;

 private:
  friend class TabAnimationTest;
  friend class TabStripAnimatorTest;

  TabAnimationState(float openness,
                    float pinnedness,
                    float activeness,
                    float normalized_leading_edge_x)
      : openness_(openness),
        pinnedness_(pinnedness),
        activeness_(activeness),
        normalized_leading_edge_x_(normalized_leading_edge_x) {}

  // All widths are affected by pinnedness and activeness in the same way.
  float TransformForPinnednessAndOpenness(
      const TabLayoutConstants& layout_constants,
      const TabSizeInfo& size_info,
      float width) const;

  // The degree to which the tab is open. 1 if it is, 0 if it is not, and in
  // between if it's in the process of animating between open and closed.
  float openness_;

  // The degree to which the tab is pinned. 1 if it is, 0 if it is not, and in
  // between if it's in the process of animating between pinned and unpinned.
  float pinnedness_;

  // The degree to which the tab is active. 1 if it is, 0 if it is not, and in
  // between if it's in the process of animating between active and inactive.
  float activeness_;

  // The offset, in number of tab slots, of the tab's bounds relative to the
  // space dedicated for it in the tabstrip.
  float normalized_leading_edge_x_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_ANIMATION_STATE_H_
