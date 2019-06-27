// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

using media_session::mojom::MediaSessionAction;

// static
MediaDialogView* MediaDialogView::instance_ = nullptr;

// static
void MediaDialogView::ShowDialog(views::View* anchor_view,
                                 service_manager::Connector* connector) {
  DCHECK(!instance_);
  instance_ = new MediaDialogView(anchor_view, connector);

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();
}

// static
void MediaDialogView::HideDialog() {
  if (IsShowing())
    instance_->GetWidget()->Close();

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
  // Do nothing if we're already showing this media session.
  if (active_session_id_.has_value() && *active_session_id_ == id)
    return;

  active_session_id_ = id;

  auto active_session_container =
      std::make_unique<MediaNotificationContainerImpl>(this, item);
  active_session_container_ = AddChildView(std::move(active_session_container));
  OnAnchorBoundsChanged();
}

void MediaDialogView::HideMediaSession(const std::string& id) {
  // Do nothing if we're not showing this media session.
  if (!active_session_id_.has_value() || active_session_id_ != id)
    return;

  active_session_id_ = base::nullopt;

  RemoveChildView(active_session_container_);
  active_session_container_ = nullptr;
  OnAnchorBoundsChanged();
}

bool MediaDialogView::ShouldShowCloseButton() const {
  return true;
}

int MediaDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool MediaDialogView::Close() {
  return Cancel();
}

gfx::Size MediaDialogView::CalculatePreferredSize() const {
  // If we have an active session, then fit to it.
  if (active_session_container_)
    return views::BubbleDialogDelegateView::CalculatePreferredSize();

  // Otherwise, use a standard size for bubble dialogs.
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

MediaDialogView::MediaDialogView(views::View* anchor_view,
                                 service_manager::Connector* connector)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      controller_(connector, this) {}

MediaDialogView::~MediaDialogView() = default;

void MediaDialogView::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets(provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                  0,
                  provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  controller_.Initialize();
}

void MediaDialogView::WindowClosing() {
  if (instance_ == this)
    instance_ = nullptr;
}
