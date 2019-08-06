// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Inactive tabs have a smaller minimum width than the active tab. Layout has
// different behavior when inactive tabs are smaller than the active tab
// than it does when they are the same size.
enum class LayoutDomain {
  // There is not enough space for inactive tabs to match the active tab's
  // width.
  kInactiveWidthBelowActiveWidth,
  // There is enough space for inactive tabs to match the active tab's width.
  kInactiveWidthEqualsActiveWidth
};

// Determines the size of each tab given information on the overall amount
// of space available relative to how much the tabs could use.
class TabSizer {
 public:
  TabSizer(const TabLayoutConstants& layout_constants,
           LayoutDomain domain,
           float space_fraction_available)
      : layout_constants_(layout_constants),
        domain_(domain),
        space_fraction_available_(space_fraction_available) {}

  int CalculateTabWidth(const TabLayoutInfo& tab) {
    switch (domain_) {
      case LayoutDomain::kInactiveWidthBelowActiveWidth:
        return std::floor(gfx::Tween::FloatValueBetween(
            space_fraction_available_,
            tab.animation_state.GetMinimumWidth(layout_constants_,
                                                tab.size_info),
            tab.animation_state.GetLayoutCrossoverWidth(layout_constants_,
                                                        tab.size_info)));
      case LayoutDomain::kInactiveWidthEqualsActiveWidth:
        return std::floor(gfx::Tween::FloatValueBetween(
            space_fraction_available_,
            tab.animation_state.GetLayoutCrossoverWidth(layout_constants_,
                                                        tab.size_info),
            tab.animation_state.GetPreferredWidth(layout_constants_,
                                                  tab.size_info)));
    }
  }

  // Returns true iff it's OK for this tab to be one pixel wider than
  // CalculateTabWidth(|tab|).
  bool TabAcceptsExtraSpace(const TabLayoutInfo& tab) {
    if (space_fraction_available_ == 0.0f || space_fraction_available_ == 1.0f)
      return false;
    switch (domain_) {
      case LayoutDomain::kInactiveWidthBelowActiveWidth:
        return tab.animation_state.GetMinimumWidth(layout_constants_,
                                                   tab.size_info) <
               tab.animation_state.GetLayoutCrossoverWidth(layout_constants_,
                                                           tab.size_info);
      case LayoutDomain::kInactiveWidthEqualsActiveWidth:
        return tab.animation_state.GetLayoutCrossoverWidth(layout_constants_,
                                                           tab.size_info) <
               tab.animation_state.GetPreferredWidth(layout_constants_,
                                                     tab.size_info);
    }
  }

  bool IsAlreadyPreferredWidth() {
    return domain_ == LayoutDomain::kInactiveWidthEqualsActiveWidth &&
           space_fraction_available_ == 1;
  }

 private:
  const TabLayoutConstants& layout_constants_;
  const LayoutDomain domain_;

  // The proportion of space requirements we can fulfill within the layout
  // domain we're in.
  const float space_fraction_available_;
};

// Solve layout constraints to determine how much space is available for tabs
// to use relative to how much they want to use.
TabSizer CalculateSpaceFractionAvailable(
    const TabLayoutConstants& layout_constants,
    const std::vector<TabLayoutInfo>& tabs,
    int width) {
  float minimum_width = 0;
  float crossover_width = 0;
  float preferred_width = 0;
  for (const TabLayoutInfo& tab : tabs) {
    // Add the tab's width, less the width of its trailing foot (which would
    // be double counting).
    minimum_width +=
        tab.animation_state.GetMinimumWidth(layout_constants, tab.size_info) -
        layout_constants.tab_overlap;
    crossover_width += tab.animation_state.GetLayoutCrossoverWidth(
                           layout_constants, tab.size_info) -
                       layout_constants.tab_overlap;
    preferred_width +=
        tab.animation_state.GetPreferredWidth(layout_constants, tab.size_info) -
        layout_constants.tab_overlap;
  }

  // Add back the width of the trailing foot of the last tab.
  minimum_width += layout_constants.tab_overlap;
  crossover_width += layout_constants.tab_overlap;
  preferred_width += layout_constants.tab_overlap;

  LayoutDomain domain;
  float space_fraction_available;
  if (width < crossover_width) {
    domain = LayoutDomain::kInactiveWidthBelowActiveWidth;
    space_fraction_available =
        (width - minimum_width) / (crossover_width - minimum_width);
  } else {
    domain = LayoutDomain::kInactiveWidthEqualsActiveWidth;
    space_fraction_available =
        preferred_width == crossover_width
            ? 1
            : (width - crossover_width) / (preferred_width - crossover_width);
  }

  space_fraction_available =
      base::ClampToRange(space_fraction_available, 0.0f, 1.0f);
  return TabSizer(layout_constants, domain, space_fraction_available);
}

// Because TabSizer::CalculateTabWidth() rounds down, the fractional part of tab
// widths go unused.  Retroactively round up tab widths from left to right to
// use up that width.
void AllocateExtraSpace(std::vector<gfx::Rect>& bounds,
                        const std::vector<TabLayoutInfo>& tabs,
                        int width,
                        TabSizer tab_sizer) {
  // Don't expand tabs if they are already at their preferred width.
  if (tab_sizer.IsAlreadyPreferredWidth())
    return;

  const int extra_space = width - bounds.back().right();
  int allocated_extra_space = 0;
  for (size_t i = 0; i < tabs.size(); i++) {
    const TabLayoutInfo& tab = tabs[i];
    bounds[i].set_x(bounds[i].x() + allocated_extra_space);
    if (allocated_extra_space < extra_space &&
        tab_sizer.TabAcceptsExtraSpace(tab)) {
      allocated_extra_space++;
      bounds[i].set_width(bounds[i].width() + 1);
    }
  }
}

}  // namespace

std::vector<gfx::Rect> CalculateTabBounds(
    const TabLayoutConstants& layout_constants,
    const std::vector<TabLayoutInfo>& tabs,
    int width) {
  if (tabs.empty())
    return std::vector<gfx::Rect>();

  TabSizer tab_sizer =
      CalculateSpaceFractionAvailable(layout_constants, tabs, width);

  int next_x = 0;
  std::vector<gfx::Rect> bounds;
  for (const TabLayoutInfo& tab : tabs) {
    const int tab_width = tab_sizer.CalculateTabWidth(tab);
    bounds.push_back(
        gfx::Rect(next_x, 0, tab_width, layout_constants.tab_height));
    next_x += tab_width - layout_constants.tab_overlap;
  }

  AllocateExtraSpace(bounds, tabs, width, tab_sizer);

  return bounds;
}

std::vector<gfx::Rect> CalculatePinnedTabBounds(
    const TabLayoutConstants& layout_constants,
    const std::vector<TabLayoutInfo>& pinned_tabs) {
  // Pinned tabs are always the same size regardless of the available width.
  return CalculateTabBounds(layout_constants, pinned_tabs, 0);
}
