// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

TabStripRegionView::TabStripRegionView() {
  views::FlexLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());

  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded));
}

TabStripRegionView::~TabStripRegionView() = default;

TabStrip* TabStripRegionView::AddTabStrip(std::unique_ptr<TabStrip> tab_strip) {
  tab_strip->SetAvailableWidthCallback(
      base::BindRepeating(&TabStripRegionView::CalculateTabStripAvailableWidth,
                          base::Unretained(this)));
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    views::ScrollView* tab_strip_scroll_container =
        AddChildView(std::make_unique<views::ScrollView>());
    tab_strip_scroll_container->SetBackgroundColor(base::nullopt);
    tab_strip_scroll_container->SetHideHorizontalScrollBar(true);
    tab_strip_scroll_container->ClipHeightTo(0, GetLayoutConstant(TAB_HEIGHT));
    tab_strip_container_ = tab_strip_scroll_container;
    return tab_strip_scroll_container->SetContents(std::move(tab_strip));
  } else {
    TabStrip* raw_tab_strip = AddChildView(std::move(tab_strip));
    tab_strip_container_ = raw_tab_strip;
    return raw_tab_strip;
  }
}

const char* TabStripRegionView::GetClassName() const {
  return "TabStripRegionView";
}

void TabStripRegionView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

int TabStripRegionView::CalculateTabStripAvailableWidth() {
  Layout();
  base::Optional<int> available_width =
      GetAvailableSize(tab_strip_container_).width();
  // |available_width| might still be undefined in cases where the tabstrip is
  // hidden (e.g. presentation mode on MacOS). In these cases we don't care
  // about the resulting layout, since the tabstrip is not visible, so we can
  // substitute 0 to ensure that we relayout once the width is defined again.
  return available_width.value_or(0);
}
