// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_browser_caption_button_container.h"

#include <memory>

#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/windows_10_caption_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/layout/flex_layout.h"

namespace {

std::unique_ptr<Windows10CaptionButton> CreateCaptionButton(
    GlassBrowserFrameView* frame_view,
    ViewID button_type,
    int accessible_name_resource_id) {
  return std::make_unique<Windows10CaptionButton>(
      frame_view, button_type,
      l10n_util::GetStringUTF16(accessible_name_resource_id));
}

bool HitTestCaptionButton(Windows10CaptionButton* button,
                          const gfx::Point& point) {
  return button && button->GetVisible() && button->bounds().Contains(point);
}

}  // anonymous namespace

GlassBrowserCaptionButtonContainer::GlassBrowserCaptionButtonContainer(
    GlassBrowserFrameView* frame_view)
    : frame_view_(frame_view),
      minimize_button_(
          AddChildView(CreateCaptionButton(frame_view,
                                           VIEW_ID_MINIMIZE_BUTTON,
                                           IDS_APP_ACCNAME_MINIMIZE))),
      maximize_button_(
          AddChildView(CreateCaptionButton(frame_view,
                                           VIEW_ID_MAXIMIZE_BUTTON,
                                           IDS_APP_ACCNAME_MAXIMIZE))),
      restore_button_(
          AddChildView(CreateCaptionButton(frame_view,
                                           VIEW_ID_RESTORE_BUTTON,
                                           IDS_APP_ACCNAME_RESTORE))),
      close_button_(AddChildView(CreateCaptionButton(frame_view,
                                                     VIEW_ID_CLOSE_BUTTON,
                                                     IDS_APP_ACCNAME_CLOSE))) {
  // Layout is horizontal, with buttons placed at the trailing end of the view.
  // This allows the container to expand to become a faux titlebar/drag handle.
  auto* const layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
}

GlassBrowserCaptionButtonContainer::~GlassBrowserCaptionButtonContainer() {}

int GlassBrowserCaptionButtonContainer::NonClientHitTest(
    const gfx::Point& point) const {
  DCHECK(HitTestPoint(point))
      << "should only be called with a point inside this view's bounds";
  if (HitTestCaptionButton(minimize_button_, point))
    return HTMINBUTTON;
  if (HitTestCaptionButton(maximize_button_, point))
    return HTMAXBUTTON;
  if (HitTestCaptionButton(restore_button_, point))
    return HTMAXBUTTON;
  if (HitTestCaptionButton(close_button_, point))
    return HTCLOSE;
  return HTCAPTION;
}

void GlassBrowserCaptionButtonContainer::ResetWindowControls() {
  minimize_button_->SetState(views::Button::STATE_NORMAL);
  maximize_button_->SetState(views::Button::STATE_NORMAL);
  restore_button_->SetState(views::Button::STATE_NORMAL);
  close_button_->SetState(views::Button::STATE_NORMAL);
  InvalidateLayout();
}

void GlassBrowserCaptionButtonContainer::ButtonPressed(views::Button* sender) {
  if (sender == minimize_button_)
    frame_view_->frame()->Minimize();
  else if (sender == maximize_button_)
    frame_view_->frame()->Maximize();
  else if (sender == restore_button_)
    frame_view_->frame()->Restore();
  else if (sender == close_button_)
    frame_view_->frame()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
}

void GlassBrowserCaptionButtonContainer::AddedToWidget() {
  views::Widget* const widget = GetWidget();
  if (!widget_observer_.IsObserving(widget))
    widget_observer_.Add(widget);
  UpdateButtonVisibility();
}

void GlassBrowserCaptionButtonContainer::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  UpdateButtonVisibility();
}

void GlassBrowserCaptionButtonContainer::UpdateButtonVisibility() {
  const bool is_maximized = frame_view_->IsMaximized();
  restore_button_->SetVisible(is_maximized);
  maximize_button_->SetVisible(!is_maximized);
  InvalidateLayout();
}
