// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

#include <memory>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

ToolbarIconContainerView::ToolbarIconContainerView(bool uses_highlight)
    : uses_highlight_(uses_highlight) {
  auto layout_manager = std::make_unique<views::FlexLayout>();
  layout_manager->SetCollapseMargins(true).SetDefaultChildMargins(
      gfx::Insets(0, 0, 0, GetLayoutConstant(TOOLBAR_ELEMENT_PADDING)));
  SetLayoutManager(std::move(layout_manager));
}

ToolbarIconContainerView::~ToolbarIconContainerView() = default;

void ToolbarIconContainerView::UpdateAllIcons() {}

void ToolbarIconContainerView::AddMainView(views::View* main_view) {
  DCHECK(!main_view_);
  // Set empty margins from this view to negate the default ones set in the
  // constructor.
  main_view->SetProperty(views::kMarginsKey, gfx::Insets());
  main_view_ = main_view;
  AddChildView(main_view_);
}

void ToolbarIconContainerView::OnHighlightChanged(
    views::Button* observed_button,
    bool highlighted) {
  // TODO(crbug.com/932818): Pass observed button type to container.
  highlighted_view_ = highlighted ? observed_button : nullptr;
  UpdateHighlight();
}

void ToolbarIconContainerView::OnStateChanged(
    views::Button* observed_button,
    views::Button::ButtonState old_state) {
  UpdateHighlight();
}

void ToolbarIconContainerView::OnViewFocused(views::View* observed_view) {
  UpdateHighlight();
}

void ToolbarIconContainerView::OnViewBlurred(views::View* observed_view) {
  UpdateHighlight();
}

void ToolbarIconContainerView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHighlight();
}

void ToolbarIconContainerView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHighlight();
}

void ToolbarIconContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ToolbarIconContainerView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

gfx::Insets ToolbarIconContainerView::GetInsets() const {
  // Use empty insets to have the border paint into the view instead of around
  // it. This prevents inadvertently increasing its size while the stroke is
  // drawn.
  return gfx::Insets();
}

void ToolbarIconContainerView::UpdateHighlight() {
  if (!uses_highlight_)
    return;

  bool highlighted = false;

  // If this container is mouse hovered and the main view is not mouse hovered
  // the container should be highlighted.
  if (!main_view_ || !main_view_->IsMouseHovered())
    highlighted = IsMouseHovered();

  // The container should also be highlighted if a dialog is anchored to.
  if (highlighted_view_)
    highlighted = true;

  // The container should also highlight if any of the child buttons are
  // either pressed or hovered.
  for (views::View* child : children()) {
    // The main view should not be considered for focus / button state.
    if (child == main_view_)
      continue;
    if (child->HasFocus())
      highlighted = true;
    views::Button* button = views::Button::AsButton(child);
    if (!button)
      continue;
    if (button->state() == views::Button::ButtonState::STATE_PRESSED ||
        button->state() == views::Button::ButtonState::STATE_HOVERED) {
      highlighted = true;
    }
  }

  SetBorder(highlighted
                ? views::CreateRoundedRectBorder(
                      1,
                      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
                          views::EMPHASIS_MAXIMUM, size()),
                      SkColorSetA(GetToolbarInkDropBaseColor(this),
                                  kToolbarButtonBackgroundAlpha))
                : nullptr);
}
