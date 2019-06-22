// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/animation/ink_drop_impl.h"

namespace ash {

ShelfButton::ShelfButton(ShelfView* shelf_view)
    : Button(nullptr), shelf_view_(shelf_view), listener_(shelf_view) {
  DCHECK(shelf_view_);
  set_hide_ink_drop_when_showing_context_menu(false);
  set_ink_drop_base_color(kShelfInkDropBaseColor);
  set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kShelfFocusBorderColor, kFocusBorderThickness, gfx::InsetsF()));
}

ShelfButton::~ShelfButton() = default;

////////////////////////////////////////////////////////////////////////////////
// views::View

const char* ShelfButton::GetClassName() const {
  return "ash/ShelfButton";
}

void ShelfButton::AboutToRequestFocusFromTabTraversal(bool reverse) {
  shelf_view_->OnShelfButtonAboutToRequestFocusFromTabTraversal(this, reverse);
}

// Do not remove this function to avoid unnecessary ChromeVox announcement
// triggered by Button::GetAccessibleNodeData. (See https://crbug.com/932200)
void ShelfButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
}

////////////////////////////////////////////////////////////////////////////////
// views::Button

void ShelfButton::NotifyClick(const ui::Event& event) {
  Button::NotifyClick(event);
  if (listener_)
    listener_->ButtonPressed(this, event, GetInkDrop());
}

std::unique_ptr<views::InkDrop> ShelfButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      Button::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  return std::move(ink_drop);
}

}  // namespace ash
