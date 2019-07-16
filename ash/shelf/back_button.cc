// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/back_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget.h"

namespace ash {

// static
const char BackButton::kViewClassName[] = "ash/BackButton";

BackButton::BackButton(Shelf* shelf, ShelfButtonDelegate* shelf_button_delegate)
    : ShelfControlButton(shelf, shelf_button_delegate) {
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ASH_SHELF_BACK_BUTTON_TITLE));
}

BackButton::~BackButton() = default;

void BackButton::NotifyClick(const ui::Event& event) {
  Button::NotifyClick(event);

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

}  // namespace ash
