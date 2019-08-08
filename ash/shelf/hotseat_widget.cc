// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_widget.h"

#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"

namespace ash {

HotseatWidget::HotseatWidget(Shelf* shelf, ShelfView* shelf_view)
    : shelf_view_(shelf_view) {
  DCHECK(shelf_view_);
}

void HotseatWidget::Initialize(aura::Window* container) {
  DCHECK(container);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "HotseatWidget";
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = container;
  Init(std::move(params));
  set_focus_on_creation(false);
  GetFocusManager()->set_arrow_key_traversal_enabled_for_widget(true);
}

}  // namespace ash
