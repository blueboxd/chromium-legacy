// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_controls_view.h"

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/media_controls_header_view.h"
#include "ash/media/media_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "components/media_message_center/media_notification_util.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr SkColor kMediaControlsBackground = SkColorSetA(SK_ColorDKGRAY, 100);

// Dimensions.
constexpr int kMediaControlsTotalWidthDp = 320;
constexpr int kMediaControlsTotalHeightDp = 400;
constexpr int kMediaControlsCornerRadius = 8;
constexpr gfx::Insets kMediaControlsInsets = gfx::Insets(25, 25, 50, 25);
constexpr int kMediaControlsChildSpacing = 50;
constexpr int kMinimumIconSize = 16;
constexpr int kDesiredIconSize = 20;
constexpr int kIconSize = 20;
constexpr int kMinimumArtworkSize = 200;
constexpr int kDesiredArtworkSize = 300;
constexpr int kArtworkViewWidth = 270;
constexpr int kArtworkViewHeight = 200;

// How long to wait (in milliseconds) for a new media session to begin.
constexpr base::TimeDelta kNextMediaDelay =
    base::TimeDelta::FromMilliseconds(2500);

constexpr const char kLockScreenMediaControlsViewName[] =
    "LockScreenMediaControlsView";

// Scales |size| to fit |view_size| while preserving proportions.
gfx::Size ScaleSizeToFitView(const gfx::Size& size,
                             const gfx::Size& view_size) {
  // If |size| is too big in either dimension or two small in both
  // dimensions, scale it appropriately.
  if ((size.width() > view_size.width() ||
       size.height() > view_size.height()) ||
      (size.width() < view_size.width() &&
       size.height() < view_size.height())) {
    const float scale =
        std::min(view_size.width() / static_cast<float>(size.width()),
                 view_size.height() / static_cast<float>(size.height()));
    return gfx::ScaleToFlooredSize(size, scale);
  }

  return size;
}

}  // namespace

LockScreenMediaControlsView::LockScreenMediaControlsView(
    service_manager::Connector* connector,
    LockContentsView* view)
    : view_(view),
      connector_(connector),
      hide_controls_timer_(new base::OneShotTimer()) {
  SetBackground(views::CreateRoundedRectBackground(kMediaControlsBackground,
                                                   kMediaControlsCornerRadius));
  middle_spacing_ = std::make_unique<NonAccessibleView>();
  middle_spacing_->set_owned_by_client();

  // Media controls have not been dismissed initially.
  Shell::Get()->media_controller()->SetMediaControlsDismissed(false);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kMediaControlsInsets,
      kMediaControlsChildSpacing));

  header_row_ = AddChildView(std::make_unique<MediaControlsHeaderView>());

  auto session_artwork = std::make_unique<views::ImageView>();
  session_artwork->SetPreferredSize(
      gfx::Size(kArtworkViewWidth, kArtworkViewHeight));
  session_artwork_ = AddChildView(std::move(session_artwork));

  // Set child view data to default values initially, until the media controller
  // observers are triggered by a change in media session state..
  MediaSessionMetadataChanged(base::nullopt);
  MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, SkBitmap());
  SetArtwork(base::nullopt);

  // |connector_| can be null in tests.
  if (!connector_)
    return;

  // Connect to the MediaControllerManager and create a MediaController that
  // controls the active session so we can observe it.
  media_session::mojom::MediaControllerManagerPtr controller_manager_ptr;
  connector_->BindInterface(media_session::mojom::kServiceName,
                            mojo::MakeRequest(&controller_manager_ptr));
  controller_manager_ptr->CreateActiveMediaController(
      mojo::MakeRequest(&media_controller_ptr_));

  // Observe the active media controller for changes.
  media_session::mojom::MediaControllerObserverPtr media_controller_observer;
  observer_binding_.Bind(mojo::MakeRequest(&media_controller_observer));
  media_controller_ptr_->AddObserver(std::move(media_controller_observer));

  mojo::PendingRemote<media_session::mojom::MediaControllerImageObserver>
      artwork_observer;
  artwork_observer_receiver_.Bind(
      artwork_observer.InitWithNewPipeAndPassReceiver());
  media_controller_ptr_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kArtwork,
      kMinimumArtworkSize, kDesiredArtworkSize, std::move(artwork_observer));

  mojo::PendingRemote<media_session::mojom::MediaControllerImageObserver>
      icon_observer;
  icon_observer_receiver_.Bind(icon_observer.InitWithNewPipeAndPassReceiver());
  media_controller_ptr_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kSourceIcon,
      kMinimumIconSize, kDesiredIconSize, std::move(icon_observer));
}

LockScreenMediaControlsView::~LockScreenMediaControlsView() = default;

const char* LockScreenMediaControlsView::GetClassName() const {
  return kLockScreenMediaControlsViewName;
}

gfx::Size LockScreenMediaControlsView::CalculatePreferredSize() const {
  return gfx::Size(kMediaControlsTotalWidthDp, kMediaControlsTotalHeightDp);
}

void LockScreenMediaControlsView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListItem;
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(
          IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACCESSIBLE_NAME));

  if (!accessible_name_.empty())
    node_data->SetName(accessible_name_);
}

views::View* LockScreenMediaControlsView::GetMiddleSpacingView() {
  return middle_spacing_.get();
}

void LockScreenMediaControlsView::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  // If a new media session began while waiting, keep showing the controls.
  if (hide_controls_timer_->IsRunning() && session_info)
    hide_controls_timer_->Stop();

  // If controls aren't enabled or there is no session to show, don't show the
  // controls.
  if (!view_->AreMediaControlsEnabled() ||
      (!session_info && !media_session_info_)) {
    view_->HideMediaControlsLayout();
  } else if (!session_info && media_session_info_) {
    // If there is no current session but there was a previous one, wait to see
    // if a new session starts before hiding the controls.
    hide_controls_timer_->Start(
        FROM_HERE, kNextMediaDelay,
        base::BindOnce(&LockContentsView::HideMediaControlsLayout,
                       base::Unretained(view_)));
  } else if (!IsDrawn() &&
             session_info->playback_state ==
                 media_session::mojom::MediaPlaybackState::kPaused) {
    // If the screen is locked while media is paused, don't show the controls.
    view_->HideMediaControlsLayout();
  } else if (!IsDrawn()) {
    view_->CreateMediaControlsLayout();
  }
  media_session_info_ = std::move(session_info);
}

void LockScreenMediaControlsView::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  media_session::MediaMetadata session_metadata =
      metadata.value_or(media_session::MediaMetadata());
  base::string16 source_title =
      session_metadata.source_title.empty()
          ? message_center::MessageCenter::Get()->GetSystemNotificationAppName()
          : session_metadata.source_title;
  header_row_->SetAppName(source_title);

  accessible_name_ =
      media_message_center::GetAccessibleNameFromMetadata(session_metadata);
}

void LockScreenMediaControlsView::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  switch (type) {
    case media_session::mojom::MediaSessionImageType::kArtwork: {
      base::Optional<gfx::ImageSkia> session_artwork =
          gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
      SetArtwork(session_artwork);
      break;
    }
    case media_session::mojom::MediaSessionImageType::kSourceIcon: {
      gfx::ImageSkia session_icon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
      if (session_icon.isNull()) {
        session_icon = gfx::CreateVectorIcon(message_center::kProductIcon,
                                             kIconSize, gfx::kChromeIconGrey);
      }
      header_row_->SetAppIcon(session_icon);
    }
  }
}

void LockScreenMediaControlsView::SetArtwork(
    base::Optional<gfx::ImageSkia> img) {
  if (!img.has_value()) {
    session_artwork_->SetImage(nullptr);
    return;
  }

  session_artwork_->SetImageSize(ScaleSizeToFitView(
      img->size(), gfx::Size(kArtworkViewWidth, kArtworkViewHeight)));
  session_artwork_->SetImage(*img);
}

}  // namespace ash
