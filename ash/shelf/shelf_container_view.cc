// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_container_view.h"

#include "ash/shelf/shelf_constants.h"

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
  const int width =
      ShelfView::GetSizeOfAppIcons(shelf_view_->last_visible_index() -
                                       shelf_view_->first_visible_index() + 1,
                                   false);
  const int height = ShelfConstants::button_size();
  return shelf_view_->shelf()->IsHorizontalAlignment()
             ? gfx::Size(width, height)
             : gfx::Size(height, width);
}

void ShelfContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ShelfContainerView::Layout() {
  shelf_view_->SetBoundsRect(gfx::Rect(shelf_view_->GetPreferredSize()));
}

const char* ShelfContainerView::GetClassName() const {
  return "ShelfContainerView";
}

void ShelfContainerView::TranslateShelfView(const gfx::Vector2dF& offset) {
  gfx::Transform transform_matrix;
  transform_matrix.Translate(-offset);
  shelf_view_->SetTransform(transform_matrix);
}

}  // namespace ash
