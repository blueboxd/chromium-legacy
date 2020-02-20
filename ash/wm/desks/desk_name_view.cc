// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_name_view.h"

#include <memory>

#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"

namespace ash {

DeskNameView::DeskNameView() {
  auto border = std::make_unique<WmHighlightItemBorder>(/*corner_radius=*/4);
  border_ptr_ = border.get();
  SetBorder(std::move(border));

  SetBackgroundColor(SK_ColorTRANSPARENT);
  SetTextColor(SK_ColorWHITE);
  SetCursorEnabled(true);
  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
}

DeskNameView::~DeskNameView() = default;

void DeskNameView::SetTextAndElideIfNeeded(const base::string16& text) {
  SetText(gfx::ElideText(text, GetFontList(), GetContentsBounds().width(),
                         gfx::ELIDE_TAIL));
}

const char* DeskNameView::GetClassName() const {
  return "DeskNameView";
}

gfx::Size DeskNameView::CalculatePreferredSize() const {
  const auto& text = GetText();
  int width = 0;
  int height = 0;
  gfx::Canvas::SizeStringInt(text, GetFontList(), &width, &height, 0,
                             gfx::Canvas::NO_ELLIPSIS);
  gfx::Size size{width + GetCaretBounds().width(), height};
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

bool DeskNameView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // The default behavior of the tab key is that it moves the focus to the next
  // available DeskNameView. We want the focus to remain in this view until the
  // changes are committed via Enter or Esc. Tabbing is handled by the
  // OverviewHighlightController.
  if (event.key_code() == ui::VKEY_TAB)
    return true;

  return false;
}

views::View* DeskNameView::GetView() {
  return this;
}

void DeskNameView::MaybeActivateHighlightedView() {
  RequestFocus();
}

void DeskNameView::MaybeCloseHighlightedView() {}

void DeskNameView::OnViewHighlighted() {
  UpdateBorderState();
}

void DeskNameView::OnViewUnhighlighted() {
  UpdateBorderState();
}

void DeskNameView::UpdateBorderState() {
  border_ptr_->set_color(IsViewHighlighted() ? gfx::kGoogleBlue300
                                             : SK_ColorTRANSPARENT);
  SchedulePaint();
}

}  // namespace ash
