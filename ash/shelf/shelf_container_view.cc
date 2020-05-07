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
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  shelf_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  shelf_view_->layer()->SetFillsBoundsOpaquely(false);
  AddChildView(shelf_view_);
}

gfx::Size ShelfContainerView::CalculatePreferredSize() const {
  return CalculateIdealSize();
}

void ShelfContainerView::ChildPreferredSizeChanged(views::View* child) {
  // The CL (https://crrev.com/c/1876128) modifies View::PreferredSizeChanged
  // by moving InvalidateLayout() after ChildPreferredSizeChanged(). Meanwhile,
  // the parent view of ShelfContainerView overrides ChildPreferredSizeChanged
  // with calling Layout(). Due to the CL above, ShelfContainerView is not
  // labeled as |needs_layout_| when the parent view updates the layout. As a
  // result, Calling Layout() in the parent view may not trigger the update in
  // child view. So we have to invalidate the layout here explicitly.
  InvalidateLayout();

  PreferredSizeChanged();
}

const char* ShelfContainerView::GetClassName() const {
  return "ShelfContainerView";
}

void ShelfContainerView::TranslateShelfView(const gfx::Vector2dF& offset) {
  gfx::Transform transform_matrix;
  transform_matrix.Translate(-offset);
  shelf_view_->SetTransform(transform_matrix);
  shelf_view_->NotifyAccessibilityEvent(ax::mojom::Event::kLocationChanged,
                                        true);
}

gfx::Size ShelfContainerView::CalculateIdealSize() const {
  const int button_size = shelf_view_->GetButtonSize();
  const int width =
      ShelfView::GetSizeOfAppButtons(shelf_view_->last_visible_index() -
                                         shelf_view_->first_visible_index() + 1,
                                     button_size);
  return shelf_view_->shelf()->IsHorizontalAlignment()
             ? gfx::Size(width, button_size)
             : gfx::Size(button_size, width);
}

}  // namespace ash
