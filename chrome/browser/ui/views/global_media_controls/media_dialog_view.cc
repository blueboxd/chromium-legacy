// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_list_view.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

using media_session::mojom::MediaSessionAction;

// static
MediaDialogView* MediaDialogView::instance_ = nullptr;

// static
void MediaDialogView::ShowDialog(views::View* anchor_view,
                                 MediaToolbarButtonController* controller,
                                 service_manager::Connector* connector) {
  DCHECK(!instance_);
  DCHECK(controller);
  instance_ = new MediaDialogView(anchor_view, controller, connector);

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();
}

// static
void MediaDialogView::HideDialog() {
  if (IsShowing()) {
    instance_->controller_->SetDialogDelegate(nullptr);
    instance_->GetWidget()->Close();
  }

  // Set |instance_| to nullptr so that |IsShowing()| returns false immediately.
  // We also set to nullptr in |WindowClosing()| (which happens asynchronously),
  // since |HideDialog()| is not always called.
  instance_ = nullptr;
}

// static
bool MediaDialogView::IsShowing() {
  return instance_ != nullptr;
}

void MediaDialogView::ShowMediaSession(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  active_sessions_view_->ShowNotification(
      id, std::make_unique<MediaNotificationContainerImpl>(this, controller_,
                                                           id, item));
  OnAnchorBoundsChanged();
}

void MediaDialogView::HideMediaSession(const std::string& id) {
  active_sessions_view_->HideNotification(id);

  if (active_sessions_view_->empty())
    HideDialog();
  else
    OnAnchorBoundsChanged();
}

int MediaDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool MediaDialogView::Close() {
  return Cancel();
}

void MediaDialogView::AddedToWidget() {
  views::BubbleFrameView* frame = GetBubbleFrameView();
  if (frame) {
    frame->SetCornerRadius(views::LayoutProvider::Get()->GetCornerRadiusMetric(
        views::EMPHASIS_HIGH));
  }

  controller_->SetDialogDelegate(this);
}

gfx::Size MediaDialogView::CalculatePreferredSize() const {
  // If we have active sessions, then fit to them.
  if (!active_sessions_view_->empty())
    return views::BubbleDialogDelegateView::CalculatePreferredSize();

  // Otherwise, use a standard size for bubble dialogs.
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

MediaDialogView::MediaDialogView(views::View* anchor_view,
                                 MediaToolbarButtonController* controller,
                                 service_manager::Connector* connector)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      controller_(controller),
      active_sessions_view_(
          AddChildView(std::make_unique<MediaNotificationListView>())) {
  DCHECK(controller_);
}

MediaDialogView::~MediaDialogView() = default;

void MediaDialogView::Init() {
  // Remove margins.
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void MediaDialogView::WindowClosing() {
  if (instance_ == this) {
    instance_ = nullptr;
    controller_->SetDialogDelegate(nullptr);
  }
}
