// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl.h"

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// TODO(steimel): We need to decide on the correct values here.
constexpr int kWidth = 400;
constexpr gfx::Size kNormalSize = gfx::Size(kWidth, 100);
constexpr gfx::Size kExpandedSize = gfx::Size(kWidth, 150);
constexpr gfx::Size kDismissButtonSize = gfx::Size(30, 30);
constexpr int kDismissButtonIconSize = 20;
constexpr int kDismissButtonBackgroundRadius = 15;
constexpr SkColor kDefaultForegroundColor = SK_ColorBLACK;
constexpr SkColor kDefaultBackgroundColor = SK_ColorTRANSPARENT;

// The minimum number of enabled and visible user actions such that we should
// force the MediaNotificationView to be expanded.
constexpr int kMinVisibleActionsForExpanding = 4;

}  // anonymous namespace

class MediaNotificationContainerImpl::DismissButton
    : public views::ImageButton {
 public:
  explicit DismissButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {
    views::ConfigureVectorImageButton(this);
  }

  ~DismissButton() override = default;

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::CircleInkDropMask>(
        size(), GetLocalBounds().CenterPoint(), kDismissButtonBackgroundRadius);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DismissButton);
};

MediaNotificationContainerImpl::MediaNotificationContainerImpl(
    MediaDialogView* parent,
    MediaToolbarButtonController* controller,
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item)
    : parent_(parent),
      controller_(controller),
      id_(id),
      foreground_color_(kDefaultForegroundColor),
      background_color_(kDefaultBackgroundColor) {
  DCHECK(parent_);
  DCHECK(controller_);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPreferredSize(kNormalSize);

  dismiss_button_container_ = std::make_unique<views::View>();
  dismiss_button_container_->set_owned_by_client();
  dismiss_button_container_->SetPreferredSize(kDismissButtonSize);
  dismiss_button_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  dismiss_button_container_->SetVisible(true);

  auto dismiss_button = std::make_unique<DismissButton>(this);
  dismiss_button->SetPreferredSize(kDismissButtonSize);
  dismiss_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_GLOBAL_MEDIA_CONTROLS_DISMISS_ICON_TOOLTIP_TEXT));
  dismiss_button_ =
      dismiss_button_container_->AddChildView(std::move(dismiss_button));
  UpdateDismissButtonIcon();

  view_ = std::make_unique<media_message_center::MediaNotificationView>(
      this, std::move(item), dismiss_button_container_.get(), base::string16());
  view_->set_owned_by_client();
  ForceExpandedState();

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
  dismiss_button_container_->SetVisible(
      !session_info || session_info->playback_state !=
                           media_session::mojom::MediaPlaybackState::kPlaying);
}

void MediaNotificationContainerImpl::OnVisibleActionsChanged(
    const std::set<media_session::mojom::MediaSessionAction>& actions) {
  has_many_actions_ = actions.size() >= kMinVisibleActionsForExpanding;
  ForceExpandedState();
}

void MediaNotificationContainerImpl::OnMediaArtworkChanged(
    const gfx::ImageSkia& image) {
  has_artwork_ = !image.isNull();

  UpdateDismissButtonBackground();
  ForceExpandedState();
}

void MediaNotificationContainerImpl::OnColorsChanged(SkColor foreground,
                                                     SkColor background) {
  if (foreground_color_ != foreground) {
    foreground_color_ = foreground;
    UpdateDismissButtonIcon();
  }
  if (background_color_ != background) {
    background_color_ = background;
    UpdateDismissButtonBackground();
  }
}

void MediaNotificationContainerImpl::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  DCHECK_EQ(dismiss_button_, sender);
  controller_->OnDismissButtonClicked(id_);
}

void MediaNotificationContainerImpl::UpdateDismissButtonIcon() {
  views::SetImageFromVectorIconWithColor(
      dismiss_button_, vector_icons::kCloseRoundedIcon, kDismissButtonIconSize,
      foreground_color_);
}

void MediaNotificationContainerImpl::UpdateDismissButtonBackground() {
  if (!has_artwork_) {
    dismiss_button_container_->SetBackground(nullptr);
    return;
  }

  dismiss_button_container_->SetBackground(views::CreateRoundedRectBackground(
      background_color_, kDismissButtonBackgroundRadius));
}

void MediaNotificationContainerImpl::ForceExpandedState() {
  if (view_) {
    bool should_expand = has_many_actions_ || has_artwork_;
    view_->SetForcedExpandedState(&should_expand);
  }
}
