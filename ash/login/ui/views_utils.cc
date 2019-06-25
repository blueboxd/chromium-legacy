// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/views_utils.h"

#include "ash/login/ui/non_accessible_view.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace login_views_utils {

std::unique_ptr<views::View> WrapViewForPreferredSize(
    std::unique_ptr<views::View> view) {
  auto proxy = std::make_unique<NonAccessibleView>();
  auto layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  proxy->SetLayoutManager(std::move(layout_manager));
  proxy->AddChildView(std::move(view));
  return proxy;
}

bool ShouldShowLandscape(const views::Widget* widget) {
  // |widget| is null when the view is being constructed. Default to landscape
  // in that case. A new layout will happen when the view is attached to a
  // widget (see LockContentsView::AddedToWidget), which will let us fetch the
  // correct display orientation.
  if (!widget)
    return true;

  // Get the orientation for |widget|.
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());

  // The display bounds are updated after a rotation. This means that if the
  // device has resolution 800x600, and the rotation is
  // display::Display::ROTATE_0, bounds() is 800x600. On
  // display::Display::ROTATE_90, bounds() is 600x800.
  //
  // ash/login/ui assumes landscape means width>height, and portrait means
  // height>width.
  //
  // Considering the actual rotation of the device introduces edge-cases, ie,
  // when the device resolution in display::Display::ROTATE_0 is 768x1024, such
  // as in https://crbug.com/858858.
  return display.bounds().width() > display.bounds().height();
}

bool HasFocusInAnyChildView(views::View* view) {
  // Find the topmost ancestor of the focused view, or |view|, whichever comes
  // first.
  views::View* search = view->GetFocusManager()->GetFocusedView();
  while (search && search != view)
    search = search->parent();
  return search == view;
}

views::Label* CreateBubbleLabel(const base::string16& message, SkColor color) {
  views::Label* label = new views::Label(message, views::style::CONTEXT_LABEL,
                                         views::style::STYLE_PRIMARY);
  label->SetLineHeight(20);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(color);
  label->SetSubpixelRenderingEnabled(false);
  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  label->SetFontList(base_font_list.Derive(0, gfx::Font::FontStyle::NORMAL,
                                           gfx::Font::Weight::NORMAL));
  return label;
}

views::View* GetTopLevelParentView(views::View* view) {
  views::View* v = view;

  while (v->parent() != nullptr)
    v = v->parent();

  return v;
}

}  // namespace login_views_utils
}  // namespace ash
