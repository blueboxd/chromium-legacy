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
  layout_manager->SetCollapseMargins(true).SetDefault(
      views::kMarginsKey,
      gfx::Insets(0, 0, 0, GetLayoutConstant(TOOLBAR_ELEMENT_PADDING)));
  SetLayoutManager(std::move(layout_manager));
}

ToolbarIconContainerView::~ToolbarIconContainerView() = default;

void ToolbarIconContainerView::UpdateAllIcons() {}

void ToolbarIconContainerView::AddMainButton(views::Button* main_button) {
  DCHECK(!main_button_);
  // Set empty margins from this view to remove the default ones set in the
  // constructor.
  main_button->SetProperty(views::kMarginsKey, gfx::Insets());
  main_button->AddObserver(this);
  main_button->AddButtonObserver(this);
  main_button_ = main_button;
  AddChildView(main_button_);
}

void ToolbarIconContainerView::OnHighlightChanged(
    views::Button* observed_button,
    bool highlighted) {
  if (highlighted) {
    DCHECK(observed_button);
    DCHECK(observed_button->GetVisible());
  }

  // TODO(crbug.com/932818): Pass observed button type to container.
  highlighted_button_ = highlighted ? observed_button : nullptr;

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

void ToolbarIconContainerView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  // Update the highlight as this might have changed the number of visible
  // children.
  UpdateHighlight();
}

void ToolbarIconContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ToolbarIconContainerView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
  // Update the highlight as this might have changed the number of visible
  // children.
  UpdateHighlight();
}

gfx::Insets ToolbarIconContainerView::GetInsets() const {
  // Use empty insets to have the border paint into the view instead of around
  // it. This prevents inadvertently increasing its size while the stroke is
  // drawn.
  return gfx::Insets();
}

bool ToolbarIconContainerView::ShouldDisplayHighlight() {
  if (!uses_highlight_)
    return false;

  const int num_visible_children =
      std::count_if(children().begin(), children().end(),
                    [](views::View* child) { return child->GetVisible(); });

  // If there's only one visible child we never need to draw a border stroke to
  // connect them.
  if (num_visible_children <= 1)
    return false;

  // The container should also be highlighted if a dialog is anchored to.
  if (highlighted_button_)
    return true;

  if (IsMouseHovered())
    return true;

  // Focused, pressed or hovered children should trigger the highlight.
  for (views::View* child : children()) {
    if (child->HasFocus())
      return true;
    views::Button* button = views::Button::AsButton(child);
    if (!button)
      continue;
    if (button->state() == views::Button::ButtonState::STATE_PRESSED ||
        button->state() == views::Button::ButtonState::STATE_HOVERED) {
      return true;
    }
  }
  return false;
}

void ToolbarIconContainerView::UpdateHighlight() {
  SetBorder(ShouldDisplayHighlight()
                ? views::CreateRoundedRectBorder(
                      1,
                      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
                          views::EMPHASIS_MAXIMUM, size()),
                      SkColorSetA(GetToolbarInkDropBaseColor(this),
                                  kToolbarButtonBackgroundAlpha))
                : nullptr);
}
