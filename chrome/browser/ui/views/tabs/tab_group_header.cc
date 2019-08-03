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
#include "ui/views/view_class_properties.h"

TabGroupHeader::TabGroupHeader(TabController* controller, TabGroupId group)
    : controller_(controller), group_(group) {
  DCHECK(controller);

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  title_chip_ = AddChildView(std::make_unique<views::View>());
  title_chip_->SetBorder(views::CreateEmptyBorder(
      provider->GetInsetsMetric(INSETS_TAB_GROUP_TITLE_CHIP)));
  title_chip_->SetLayoutManager(std::make_unique<views::FillLayout>());
  title_chip_->SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification::ForSizeRule(
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));

  title_ = title_chip_->AddChildView(std::make_unique<views::Label>());
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_->SetElideBehavior(gfx::FADE_TAIL);

  VisualsChanged();
}

void TabGroupHeader::VisualsChanged() {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const TabGroupVisualData* data = controller_->GetVisualDataForGroup(group_);
  title_chip_->SetBackground(views::CreateRoundedRectBackground(
      data->color(), provider->GetCornerRadiusMetric(views::EMPHASIS_LOW)));
  title_->SetEnabledColor(color_utils::GetColorWithMaxContrast(data->color()));
  title_->SetText(data->title());
}

bool TabGroupHeader::OnMousePressed(const ui::MouseEvent& event) {
  TabGroupEditorBubbleView::Show(this, controller_, group_);
  return true;
}
