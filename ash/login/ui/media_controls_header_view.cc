// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/media_controls_header_view.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/font_list.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Insets kMediaControlsHeaderInsets = gfx::Insets(0, 25, 25, 25);
constexpr int kIconSize = 20;
constexpr int kHeaderTextFontSize = 14;
constexpr int kMediaControlsHeaderChildSpacing = 10;
constexpr gfx::Insets kIconPadding = gfx::Insets(1, 1, 1, 1);
constexpr int kIconCornerRadius = 2;

}  // namespace

MediaControlsHeaderView::MediaControlsHeaderView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kMediaControlsHeaderInsets,
      kMediaControlsHeaderChildSpacing));

  auto app_icon_view = std::make_unique<views::ImageView>();
  app_icon_view->SetImageSize(gfx::Size(kIconSize, kIconSize));
  app_icon_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  app_icon_view->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  app_icon_view->SetBorder(views::CreateEmptyBorder(kIconPadding));
  app_icon_view->SetBackground(
      views::CreateRoundedRectBackground(SK_ColorWHITE, kIconCornerRadius));
  app_icon_view_ = AddChildView(std::move(app_icon_view));

  // Font list for text views.
  gfx::Font default_font;
  int font_size_delta = kHeaderTextFontSize - default_font.GetFontSize();
  gfx::Font font = default_font.Derive(font_size_delta, gfx::Font::NORMAL,
                                       gfx::Font::Weight::NORMAL);
  gfx::FontList font_list(font);

  auto app_name_view = std::make_unique<views::Label>();
  app_name_view->SetFontList(font_list);
  app_name_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  app_name_view->SetEnabledColor(SK_ColorWHITE);
  app_name_view->SetAutoColorReadabilityEnabled(false);
  app_name_view_ = AddChildView(std::move(app_name_view));
}

MediaControlsHeaderView::~MediaControlsHeaderView() = default;

void MediaControlsHeaderView::SetAppIcon(const gfx::ImageSkia& img) {
  app_icon_view_->SetImage(img);
}

void MediaControlsHeaderView::SetAppName(const base::string16& name) {
  app_name_view_->SetText(name);
}

void MediaControlsHeaderView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(app_name_view_->GetText());
}

const base::string16& MediaControlsHeaderView::app_name_for_testing() const {
  return app_name_view_->GetText();
}

const views::ImageView* MediaControlsHeaderView::app_icon_for_testing() const {
  return app_icon_view_;
}

}  // namespace ash