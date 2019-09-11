// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl.h"

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// TODO(steimel): We need to decide on the correct values here.
constexpr int kWidth = 400;
constexpr gfx::Size kNormalSize = gfx::Size(kWidth, 100);
constexpr gfx::Size kExpandedSize = gfx::Size(kWidth, 150);
constexpr gfx::Size kDismissButtonSize = gfx::Size(24, 24);
constexpr int kDismissButtonIconSize = 20;
constexpr SkColor kDefaultForegroundColor = SK_ColorBLACK;

}  // anonymous namespace

MediaNotificationContainerImpl::MediaNotificationContainerImpl(
    MediaDialogView* parent,
    MediaToolbarButtonController* controller,
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item)
    : parent_(parent),
      controller_(controller),
      id_(id),
      foreground_color_(kDefaultForegroundColor) {
  DCHECK(parent_);
  DCHECK(controller_);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPreferredSize(kNormalSize);

  dismiss_button_ = views::CreateVectorImageButton(this);
  dismiss_button_->SetPreferredSize(kDismissButtonSize);
  dismiss_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_GLOBAL_MEDIA_CONTROLS_DISMISS_ICON_TOOLTIP_TEXT));
  dismiss_button_->set_owned_by_client();
  dismiss_button_->SetVisible(false);
  UpdateDismissButtonIcon();

  view_ = std::make_unique<media_message_center::MediaNotificationView>(
      this, std::move(item), dismiss_button_.get(), base::string16());
  view_->set_owned_by_client();

  AddChildView(view_.get());
}

MediaNotificationContainerImpl::~MediaNotificationContainerImpl() = default;

void MediaNotificationContainerImpl::OnExpanded(bool expanded) {
  SetPreferredSize(expanded ? kExpandedSize : kNormalSize);
  PreferredSizeChanged();
  parent_->OnAnchorBoundsChanged();
}

void MediaNotificationContainerImpl::OnMediaSessionInfoChanged(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  dismiss_button_->SetVisible(
      !session_info || session_info->playback_state !=
                           media_session::mojom::MediaPlaybackState::kPlaying);
}

void MediaNotificationContainerImpl::OnForegoundColorChanged(SkColor color) {
  foreground_color_ = color;
  UpdateDismissButtonIcon();
}

void MediaNotificationContainerImpl::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  DCHECK_EQ(dismiss_button_.get(), sender);
  controller_->OnDismissButtonClicked(id_);
}

void MediaNotificationContainerImpl::UpdateDismissButtonIcon() {
  views::SetImageFromVectorIcon(dismiss_button_.get(),
                                vector_icons::kCloseRoundedIcon,
                                kDismissButtonIconSize, foreground_color_);
}
