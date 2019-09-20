// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_header.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

TabGroupHeader::TabGroupHeader(TabController* controller, TabGroupId group)
    : controller_(controller), group_(group) {
  DCHECK(controller);

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  title_chip_ = AddChildView(std::make_unique<views::View>());
  title_chip_->SetLayoutManager(std::make_unique<views::FillLayout>());
  // Color and border are set in VisualsChanged().

  title_ = title_chip_->AddChildView(std::make_unique<views::Label>());
  title_->SetCollapseWhenHidden(true);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_->SetElideBehavior(gfx::FADE_TAIL);

  VisualsChanged();
}

bool TabGroupHeader::OnMousePressed(const ui::MouseEvent& event) {
  // Ignore the click if the editor is already open. Do this so clicking
  // on us again doesn't re-trigger the editor.
  //
  // Though the bubble is deactivated before we receive a mouse event,
  // the actual widget destruction happens in a posted task. That task
  // gets run after we receive the mouse event. If this sounds brittle,
  // that's because it is!
  if (editor_bubble_tracker_.is_open())
    return false;

  editor_bubble_tracker_.Opened(
      TabGroupEditorBubbleView::Show(this, controller_, group_));
  return true;
}

TabSizeInfo TabGroupHeader::GetTabSizeInfo() const {
  TabSizeInfo size_info;
  // Group headers have a fixed width based on |title_|'s width.
  const int width = CalculateWidth();
  size_info.pinned_tab_width = width;
  size_info.min_active_width = width;
  size_info.min_inactive_width = width;
  size_info.standard_width = width;
  return size_info;
}

int TabGroupHeader::CalculateWidth() const {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int title_chip_outside_margin =
      provider->GetDistanceMetric(DISTANCE_TAB_GROUP_TITLE_CHIP_MARGIN);
  // We don't want tabs to visually overlap group headers, so we add that space
  // to the width to compensate. We don't want to actually remove the overlap
  // during layout however; that would cause an the margin to be visually uneven
  // when the header is in the first slot and thus wouldn't overlap anything to
  // the left.
  const int overlap_margin = TabStyle::GetTabOverlap() * 2;
  return title_chip_->GetPreferredSize().width() + title_chip_outside_margin +
         overlap_margin;
}

void TabGroupHeader::VisualsChanged() {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const TabGroupVisualData* data = controller_->GetVisualDataForGroup(group_);
  if (data->title().empty()) {
    title_->SetVisible(false);
    constexpr gfx::Rect kEmptyTitleChipSize(12, 12);
    title_chip_->SetBorder(
        views::CreateEmptyBorder(kEmptyTitleChipSize.InsetsFrom(gfx::Rect())));
  } else {
    title_->SetVisible(true);
    title_->SetEnabledColor(
        color_utils::GetColorWithMaxContrast(data->color()));
    title_->SetText(data->title());
    title_chip_->SetBorder(views::CreateEmptyBorder(
        provider->GetInsetsMetric(INSETS_TAB_GROUP_TITLE_CHIP)));
  }
  title_chip_->SetBackground(views::CreateRoundedRectBackground(
      data->color(),
      provider->GetCornerRadiusMetric(views::EMPHASIS_MAXIMUM,
                                      title_chip_->GetPreferredSize())));
}

void TabGroupHeader::EditorBubbleTracker::Opened(views::Widget* bubble_widget) {
  DCHECK(bubble_widget);
  DCHECK(!is_open_);
  is_open_ = true;
  bubble_widget->AddObserver(this);
}

void TabGroupHeader::EditorBubbleTracker::OnWidgetDestroyed(
    views::Widget* widget) {
  is_open_ = false;
}
