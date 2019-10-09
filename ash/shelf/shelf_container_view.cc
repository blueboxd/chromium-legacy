// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_container_view.h"

#include "ash/public/cpp/shelf_config.h"

namespace ash {

ShelfContainerView::ShelfContainerView(ShelfView* shelf_view)
    : shelf_view_(shelf_view) {}

ShelfContainerView::~ShelfContainerView() = default;

void ShelfContainerView::Initialize() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  shelf_view_->SetPaintToLayer();
  shelf_view_->layer()->SetFillsBoundsOpaquely(false);
  AddChildView(shelf_view_);
}

gfx::Size ShelfContainerView::CalculatePreferredSize() const {
  return CalculateIdealSize();
}

void ShelfContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ShelfContainerView::Layout() {
  // Should not use ShelfView::GetPreferredSize in replace of
  // CalculateIdealSize. Because ShelfView::CalculatePreferredSize relies on the
  // bounds of app icon. Meanwhile, the icon's bounds may be updated by
  // animation.
  const gfx::Rect ideal_bounds = gfx::Rect(CalculateIdealSize());

  const gfx::Rect local_bounds = GetLocalBounds();
  gfx::Rect shelf_view_bounds =
      local_bounds.Contains(ideal_bounds) ? local_bounds : ideal_bounds;

  // Offsets |shelf_view_bounds| to ensure the sufficient space for the ripple
  // ring of the first shelf item.
  if (shelf_view_->shelf()->IsHorizontalAlignment())
    shelf_view_bounds.Offset(
        ShelfConfig::Get()->scrollable_shelf_ripple_padding(), 0);
  else
    shelf_view_bounds.Offset(
        0, ShelfConfig::Get()->scrollable_shelf_ripple_padding());

  shelf_view_->SetBoundsRect(shelf_view_bounds);
}

const char* ShelfContainerView::GetClassName() const {
  return "ShelfContainerView";
}

void ShelfContainerView::TranslateShelfView(const gfx::Vector2dF& offset) {
  gfx::Transform transform_matrix;
  transform_matrix.Translate(-offset);
  shelf_view_->SetTransform(transform_matrix);
}

gfx::Size ShelfContainerView::CalculateIdealSize() const {
  const int width =
      ShelfView::GetSizeOfAppIcons(shelf_view_->last_visible_index() -
                                       shelf_view_->first_visible_index() + 1,
                                   false);
  const int height = ShelfConfig::Get()->button_size();
  return shelf_view_->shelf()->IsHorizontalAlignment()
             ? gfx::Size(width, height)
             : gfx::Size(height, width);
}

}  // namespace ash
