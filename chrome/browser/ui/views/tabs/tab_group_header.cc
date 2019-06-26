// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_header.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
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

  const TabGroupData* data = GetGroupData();
  const SkColor color = GetGroupData()->color();
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto title_chip = std::make_unique<views::View>();
  title_chip->SetBackground(views::CreateRoundedRectBackground(
      color, provider->GetCornerRadiusMetric(views::EMPHASIS_LOW)));
  title_chip->SetBorder(views::CreateEmptyBorder(
      provider->GetInsetsMetric(INSETS_TAB_GROUP_TITLE_CHIP)));
  title_chip->SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* title_chip_ptr = AddChildView(std::move(title_chip));
  title_chip_ptr->SetProperty(views::kFlexBehaviorKey,
                              views::FlexSpecification::ForSizeRule(
                                  views::MinimumFlexSizeRule::kScaleToZero,
                                  views::MaximumFlexSizeRule::kPreferred));

  auto title = std::make_unique<views::Label>(data->title());
  title->SetAutoColorReadabilityEnabled(false);
  title->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title->SetElideBehavior(gfx::FADE_TAIL);
  title->SetEnabledColor(color_utils::GetColorWithMaxContrast(color));
  title_chip_ptr->AddChildView(std::move(title));
}

const TabGroupData* TabGroupHeader::GetGroupData() {
  return controller_->GetDataForGroup(group_);
}
