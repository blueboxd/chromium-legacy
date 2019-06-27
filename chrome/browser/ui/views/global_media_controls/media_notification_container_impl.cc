// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl.h"

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// TODO(steimel): We need to decide on the correct values here.
constexpr int kWidth = 400;
constexpr gfx::Size kNormalSize = gfx::Size(kWidth, 100);
constexpr gfx::Size kExpandedSize = gfx::Size(kWidth, 150);

}  // anonymous namespace

MediaNotificationContainerImpl::MediaNotificationContainerImpl(
    MediaDialogView* parent,
    base::WeakPtr<media_message_center::MediaNotificationItem> item)
    : parent_(parent), view_(this, std::move(item), nullptr, base::string16()) {
  DCHECK(parent_);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPreferredSize(kNormalSize);

  view_.set_owned_by_client();

  AddChildView(&view_);
}

MediaNotificationContainerImpl::~MediaNotificationContainerImpl() = default;

void MediaNotificationContainerImpl::OnExpanded(bool expanded) {
  SetPreferredSize(expanded ? kExpandedSize : kNormalSize);
  PreferredSizeChanged();
  parent_->OnAnchorBoundsChanged();
}
