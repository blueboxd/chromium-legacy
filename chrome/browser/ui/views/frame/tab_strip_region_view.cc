// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

// Size calculation used for tabstrip scroll container. Equivalent to using
// a (kPreferredScaleToMinimum, kPreferred) flex specification on the tabstrip
// itself, bypassing the ScrollView.
gfx::Size TabScrollContainerFlexRule(const views::View* tab_strip,
                                     const views::View* view,
                                     const views::SizeBounds& size_bounds) {
  const gfx::Size preferred_size = tab_strip->GetPreferredSize();
  int width = preferred_size.width();
  if (size_bounds.width().has_value())
    width = std::min(width, size_bounds.width().value());
  return gfx::Size(width, preferred_size.height());
}

}  // namespace

TabStripRegionView::TabStripRegionView(std::unique_ptr<TabStrip> tab_strip) {
  views::FlexLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());

  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal);

  tab_strip_ = tab_strip.get();
  tab_strip->SetAvailableWidthCallback(
      base::BindRepeating(&TabStripRegionView::CalculateTabStripAvailableWidth,
                          base::Unretained(this)));
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    views::ScrollView* tab_strip_scroll_container =
        AddChildView(std::make_unique<views::ScrollView>());
    tab_strip_scroll_container->SetBackgroundColor(base::nullopt);
    tab_strip_scroll_container->SetHideHorizontalScrollBar(true);
    tab_strip_container_ = tab_strip_scroll_container;
    tab_strip_scroll_container->SetContents(std::move(tab_strip));
    tab_strip_scroll_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(base::BindRepeating(
            &TabScrollContainerFlexRule, base::Unretained(tab_strip_))));
  } else {
    tab_strip_container_ = AddChildView(std::move(tab_strip));
    tab_strip_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred));
  }
}

TabStripRegionView::~TabStripRegionView() = default;

const char* TabStripRegionView::GetClassName() const {
  return "TabStripRegionView";
}

void TabStripRegionView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

gfx::Size TabStripRegionView::GetMinimumSize() const {
  gfx::Size tab_strip_min_size = tab_strip_->GetMinimumSize();
  // Cap the tabstrip minimum width to a reasonable value so browser windows
  // aren't forced to grow arbitrarily wide.
  const int max_min_width = 520;
  tab_strip_min_size.set_width(
      std::min(max_min_width, tab_strip_min_size.width()));
  return tab_strip_min_size;
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
