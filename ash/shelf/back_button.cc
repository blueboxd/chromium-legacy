// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/back_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

// static
const char BackButton::kViewClassName[] = "ash/BackButton";

BackButton::BackButton(Shelf* shelf) : ShelfControlButton(shelf, this) {
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ASH_SHELF_BACK_BUTTON_TITLE));
  Shell::Get()->system_tray_model()->virtual_keyboard()->AddObserver(this);
}

BackButton::~BackButton() {
  Shell::Get()->system_tray_model()->virtual_keyboard()->RemoveObserver(this);
}

void BackButton::PaintButtonContents(gfx::Canvas* canvas) {
  // Use PaintButtonContents instead of SetImage so the icon gets drawn at
  // |GetCenterPoint| coordinates instead of always in the center.
  gfx::ImageSkia img = CreateVectorIcon(kShelfBackIcon, SK_ColorWHITE);
  canvas->DrawImageInt(img, GetCenterPoint().x() - img.width() / 2,
                       GetCenterPoint().y() - img.height() / 2);
}

const char* BackButton::GetClassName() const {
  return kViewClassName;
}

base::string16 BackButton::GetTooltipText(const gfx::Point& p) const {
  return GetAccessibleName();
}

void BackButton::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {
  DCHECK_EQ(button, this);
  if (!reverse) {
    // We're trying to focus this button by advancing from the last view of
    // the shelf. Let the focus manager advance to the status area instead.
    shelf()->shelf_focus_cycler()->FocusOut(reverse,
                                            SourceView::kShelfNavigationView);
  }
}

void BackButton::OnVirtualKeyboardVisibilityChanged() {
  gfx::Transform rotation;
  // Rotate the back button when the virtual keyboard is visible.
  if (Shell::Get()->system_tray_model()->virtual_keyboard()->visible())
    rotation.Rotate(270.0);
  layer()->SetTransform(TransformAboutPivot(GetCenterPoint(), rotation));
}

void BackButton::ButtonPressed(views::Button* sender,
                               const ui::Event& event,
                               views::InkDrop* ink_drop) {
  // Send up event as well as down event as ARC++ clients expect this sequence.
  // TODO: Investigate if we should be using the current modifiers.
  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::KeyEvent press_key_event(ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_BACK,
                               ui::EF_NONE);
  ignore_result(root_window->GetHost()->SendEventToSink(&press_key_event));
  ui::KeyEvent release_key_event(ui::ET_KEY_RELEASED, ui::VKEY_BROWSER_BACK,
                                 ui::EF_NONE);
  ignore_result(root_window->GetHost()->SendEventToSink(&release_key_event));
}

}  // namespace ash
