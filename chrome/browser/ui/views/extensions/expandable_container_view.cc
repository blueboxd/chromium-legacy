// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/expandable_container_view.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

// ExpandableContainerView::DetailsView ----------------------------------------
ExpandableContainerView::DetailsView::~DetailsView() = default;

ExpandableContainerView::DetailsView::DetailsView(
    const std::vector<base::string16>& details) {
  // Spacing between this and the "Hide Details" link.
  const int bottom_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, 0, bottom_padding, 0),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL)));

  for (const auto& detail : details) {
    auto detail_label = std::make_unique<views::Label>(
        detail, CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_SECONDARY);
    detail_label->SetMultiLine(true);
    detail_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(std::move(detail_label));
  }
}

gfx::Size ExpandableContainerView::DetailsView::CalculatePreferredSize() const {
  return expanded_ ? views::View::CalculatePreferredSize() : gfx::Size();
}

void ExpandableContainerView::DetailsView::ToggleExpanded() {
  expanded_ = !expanded_;
  PreferredSizeChanged();
}

// ExpandableContainerView -----------------------------------------------------

ExpandableContainerView::ExpandableContainerView(
    const std::vector<base::string16>& details,
    int available_width) {
  DCHECK(!details.empty());

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  constexpr int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);

  // Even though we only have one column, using a GridLayout here will
  // properly handle a 0 height row when |details_view_| is collapsed.
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kFixed, available_width,
                        0);

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  details_view_ = layout->AddView(std::make_unique<DetailsView>(details));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  auto details_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_DETAILS));
  details_link->set_callback(base::BindRepeating(
      &ExpandableContainerView::ToggleDetailLevel, base::Unretained(this)));
  details_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  details_link_ = layout->AddView(std::move(details_link));
}

ExpandableContainerView::~ExpandableContainerView() = default;

void ExpandableContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ExpandableContainerView::ToggleDetailLevel() {
  details_view_->ToggleExpanded();
  details_link_->SetText(l10n_util::GetStringUTF16(
      details_view_->expanded() ? IDS_EXTENSIONS_HIDE_DETAILS
                                : IDS_EXTENSIONS_SHOW_DETAILS));
}
