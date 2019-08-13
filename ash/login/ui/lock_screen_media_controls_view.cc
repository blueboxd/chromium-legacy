// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_controls_view.h"

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/media_controls_header_view.h"
#include "ash/media/media_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "components/media_message_center/media_controls_progress_view.h"
#include "components/media_message_center/media_notification_util.h"
#include "services/media_session/public/cpp/util.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

using media_session::mojom::MediaSessionAction;

namespace {

constexpr SkColor kMediaControlsBackground = SkColorSetA(SK_ColorDKGRAY, 150);
constexpr SkColor kMediaButtonColor = SK_ColorWHITE;

// Maximum number of actions that should be displayed on |button_row_|.
constexpr size_t kMaxActions = 5;

// Dimensions.
constexpr gfx::Insets kMediaControlsInsets = gfx::Insets(15, 15, 15, 15);
constexpr int kMediaControlsCornerRadius = 8;
constexpr int kMinimumIconSize = 16;
constexpr int kDesiredIconSize = 20;
constexpr int kIconSize = 20;
constexpr int kMinimumArtworkSize = 50;
constexpr int kDesiredArtworkSize = 80;
constexpr gfx::Size kArtworkSize = gfx::Size(80, 80);
constexpr gfx::Size kMediaButtonSize = gfx::Size(38, 38);
constexpr int kMediaButtonRowSeparator = 24;
constexpr gfx::Insets kButtonRowInsets = gfx::Insets(10, 0, 0, 0);
constexpr int kPlayPauseIconSize = 28;
constexpr int kChangeTrackIconSize = 14;
constexpr int kSeekingIconsSize = 26;
constexpr gfx::Size kMediaControlsButtonRowSize =
    gfx::Size(300, kMediaButtonSize.height());

constexpr int kDragVelocityThreshold = -6;
constexpr int kHeightDismissalThreshold = 20;
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(200);

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

const gfx::VectorIcon& GetVectorIconForMediaAction(MediaSessionAction action) {
  switch (action) {
    case MediaSessionAction::kPreviousTrack:
      return kLockScreenPreviousTrackIcon;
    case MediaSessionAction::kPause:
      return kLockScreenPauseIcon;
    case MediaSessionAction::kNextTrack:
      return kLockScreenNextTrackIcon;
    case MediaSessionAction::kPlay:
      return kLockScreenPlayIcon;
    case MediaSessionAction::kSeekBackward:
      return kLockScreenSeekBackwardIcon;
    case MediaSessionAction::kSeekForward:
      return kLockScreenSeekForwardIcon;

    // The following actions are not yet supported on the controls.
    case MediaSessionAction::kStop:
    case MediaSessionAction::kSkipAd:
    case MediaSessionAction::kSeekTo:
    case MediaSessionAction::kScrubTo:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return gfx::kNoneIcon;
}

}  // namespace

LockScreenMediaControlsView::Callbacks::Callbacks() = default;

LockScreenMediaControlsView::Callbacks::~Callbacks() = default;

LockScreenMediaControlsView::LockScreenMediaControlsView(
    service_manager::Connector* connector,
    const Callbacks& callbacks)
    : connector_(connector),
      hide_controls_timer_(new base::OneShotTimer()),
      media_controls_enabled_(callbacks.media_controls_enabled),
      hide_media_controls_(callbacks.hide_media_controls),
      show_media_controls_(callbacks.show_media_controls) {
  DCHECK(callbacks.media_controls_enabled);
  DCHECK(callbacks.hide_media_controls);
  DCHECK(callbacks.show_media_controls);

  // Media controls have not been dismissed initially.
  Shell::Get()->media_controller()->SetMediaControlsDismissed(false);

  middle_spacing_ = std::make_unique<NonAccessibleView>();
  middle_spacing_->set_owned_by_client();

  set_notify_enter_exit_on_child(true);

  contents_view_ = AddChildView(std::make_unique<views::View>());
  contents_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kMediaControlsInsets));
  contents_view_->SetBackground(views::CreateRoundedRectBackground(
      kMediaControlsBackground, kMediaControlsCornerRadius));

  contents_view_->SetPaintToLayer();  // Needed for opacity animation.
  contents_view_->layer()->SetFillsBoundsOpaquely(false);

  // |header_row_| contains the app icon and source title of the current media
  // session. It also contains the close button.
  header_row_ = contents_view_->AddChildView(
      std::make_unique<MediaControlsHeaderView>(base::BindOnce(
          &LockScreenMediaControlsView::Dismiss, base::Unretained(this))));

  auto session_artwork = std::make_unique<views::ImageView>();
  session_artwork->SetPreferredSize(kArtworkSize);
  session_artwork->SetHorizontalAlignment(
      views::ImageView::Alignment::kLeading);
  session_artwork_ = contents_view_->AddChildView(std::move(session_artwork));

  progress_ = contents_view_->AddChildView(
      std::make_unique<media_message_center::MediaControlsProgressView>(
          base::BindRepeating(&LockScreenMediaControlsView::SeekTo,
                              base::Unretained(this))));

  // |button_row_| contains the buttons for controlling playback.
  auto button_row = std::make_unique<NonAccessibleView>();
  auto* button_row_layout =
      button_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kButtonRowInsets,
          kMediaButtonRowSeparator));
  button_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  button_row_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  button_row->SetPreferredSize(kMediaControlsButtonRowSize);
  button_row_ = contents_view_->AddChildView(std::move(button_row));

  CreateMediaButton(
      kChangeTrackIconSize, MediaSessionAction::kPreviousTrack,
      l10n_util::GetStringUTF16(
          IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_PREVIOUS_TRACK));

  CreateMediaButton(
      kSeekingIconsSize, MediaSessionAction::kSeekBackward,
      l10n_util::GetStringUTF16(
          IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_SEEK_BACKWARD));

  // |play_pause_button_| toggles playback. If the current media is playing, the
  // tag will be |MediaSessionAction::kPause|. If the current media is paused,
  // the tag will be |MediaSessionAction::kPlay|.
  auto play_pause_button = views::CreateVectorToggleImageButton(this);
  play_pause_button->set_tag(static_cast<int>(MediaSessionAction::kPause));
  play_pause_button->SetPreferredSize(kMediaButtonSize);
  play_pause_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  play_pause_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_PAUSE));
  play_pause_button->SetToggledTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_PLAY));
  play_pause_button_ = button_row_->AddChildView(std::move(play_pause_button));

  // |play_pause_button_| icons.
  views::SetImageFromVectorIcon(
      play_pause_button_,
      GetVectorIconForMediaAction(MediaSessionAction::kPause),
      kPlayPauseIconSize, kMediaButtonColor);
  views::SetToggledImageFromVectorIcon(
      play_pause_button_,
      GetVectorIconForMediaAction(MediaSessionAction::kPlay),
      kPlayPauseIconSize, kMediaButtonColor);

  CreateMediaButton(
      kSeekingIconsSize, MediaSessionAction::kSeekForward,
      l10n_util::GetStringUTF16(
          IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_SEEK_FORWARD));

  CreateMediaButton(kChangeTrackIconSize, MediaSessionAction::kNextTrack,
                    l10n_util::GetStringUTF16(
                        IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_NEXT_TRACK));

  // Set child view data to default values initially, until the media controller
  // observers are triggered by a change in media session state.
  MediaSessionMetadataChanged(base::nullopt);
  MediaSessionPositionChanged(base::nullopt);
  MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, SkBitmap());
  SetArtwork(base::nullopt);

  // |connector_| can be null in tests.
  if (!connector_)
    return;

  // Connect to the MediaControllerManager and create a MediaController that
  // controls the active session so we can observe it.
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  connector_->Connect(media_session::mojom::kServiceName,
                      controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_controller_remote_.BindNewPipeAndPassReceiver());

  // Observe the active media controller for changes.
  media_controller_remote_->AddObserver(
      observer_receiver_.BindNewPipeAndPassRemote());

  media_controller_remote_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kArtwork,
      kMinimumArtworkSize, kDesiredArtworkSize,
      artwork_observer_receiver_.BindNewPipeAndPassRemote());

  media_controller_remote_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kSourceIcon,
      kMinimumIconSize, kDesiredIconSize,
      icon_observer_receiver_.BindNewPipeAndPassRemote());
}

LockScreenMediaControlsView::~LockScreenMediaControlsView() = default;

const char* LockScreenMediaControlsView::GetClassName() const {
  return kLockScreenMediaControlsViewName;
}

gfx::Size LockScreenMediaControlsView::CalculatePreferredSize() const {
  return contents_view_->GetPreferredSize();
}

void LockScreenMediaControlsView::Layout() {
  contents_view_->SetBoundsRect(GetContentsBounds());
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

void LockScreenMediaControlsView::OnMouseEntered(const ui::MouseEvent& event) {
  if (is_in_drag_ || contents_view_->layer()->GetAnimator()->is_animating())
    return;

  header_row_->SetCloseButtonVisibility(true);
}

void LockScreenMediaControlsView::OnMouseExited(const ui::MouseEvent& event) {
  if (is_in_drag_ || contents_view_->layer()->GetAnimator()->is_animating())
    return;

  header_row_->SetCloseButtonVisibility(false);
}

views::View* LockScreenMediaControlsView::GetMiddleSpacingView() {
  return middle_spacing_.get();
}

void LockScreenMediaControlsView::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (hide_controls_timer_->IsRunning())
    return;

  // If controls aren't enabled or there is no session to show, don't show the
  // controls.
  if (!media_controls_enabled_.Run() || !session_info) {
    hide_media_controls_.Run();
  } else if (!IsDrawn() &&
             session_info->playback_state ==
                 media_session::mojom::MediaPlaybackState::kPaused) {
    // If the screen is locked while media is paused, don't show the controls.
    hide_media_controls_.Run();
  } else if (!IsDrawn()) {
    show_media_controls_.Run();
  }

  if (IsDrawn()) {
    SetIsPlaying(session_info &&
                 session_info->playback_state ==
                     media_session::mojom::MediaPlaybackState::kPlaying);
  }
}

void LockScreenMediaControlsView::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  if (hide_controls_timer_->IsRunning())
    return;

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

void LockScreenMediaControlsView::MediaSessionActionsChanged(
    const std::vector<MediaSessionAction>& actions) {
  if (hide_controls_timer_->IsRunning())
    return;

  enabled_actions_ =
      std::set<MediaSessionAction>(actions.begin(), actions.end());

  UpdateActionButtonsVisibility();
}

void LockScreenMediaControlsView::MediaSessionChanged(
    const base::Optional<base::UnguessableToken>& request_id) {
  if (!media_session_id_.has_value()) {
    media_session_id_ = request_id;
    return;
  }

  // If |media_session_id_| resumed while waiting, don't hide the controls.
  if (hide_controls_timer_->IsRunning() && request_id == media_session_id_) {
    hide_controls_timer_->Stop();
    enabled_actions_.clear();
  }

  // If this session is different than the previous one, wait to see if the
  // previous one resumes before hiding the controls.
  if (request_id != media_session_id_) {
    hide_controls_timer_->Start(FROM_HERE, kNextMediaDelay,
                                hide_media_controls_);
  }
}

void LockScreenMediaControlsView::MediaSessionPositionChanged(
    const base::Optional<media_session::MediaPosition>& position) {
  if (hide_controls_timer_->IsRunning())
    return;

  position_ = position;

  if (!position.has_value()) {
    if (progress_->GetVisible()) {
      progress_->SetVisible(false);
      Layout();
    }
    return;
  }

  progress_->UpdateProgress(*position);

  if (!progress_->GetVisible()) {
    progress_->SetVisible(true);
    Layout();
  }
}

void LockScreenMediaControlsView::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  if (hide_controls_timer_->IsRunning())
    return;

  // Convert the bitmap to kN32_SkColorType if necessary.
  SkBitmap converted_bitmap;
  if (bitmap.colorType() == kN32_SkColorType) {
    converted_bitmap = bitmap;
  } else {
    SkImageInfo info = bitmap.info().makeColorType(kN32_SkColorType);
    if (converted_bitmap.tryAllocPixels(info)) {
      bitmap.readPixels(info, converted_bitmap.getPixels(),
                        converted_bitmap.rowBytes(), 0, 0);
    }
  }

  switch (type) {
    case media_session::mojom::MediaSessionImageType::kArtwork: {
      base::Optional<gfx::ImageSkia> session_artwork =
          gfx::ImageSkia::CreateFrom1xBitmap(converted_bitmap);
      SetArtwork(session_artwork);
      break;
    }
    case media_session::mojom::MediaSessionImageType::kSourceIcon: {
      gfx::ImageSkia session_icon =
          gfx::ImageSkia::CreateFrom1xBitmap(converted_bitmap);
      if (session_icon.isNull()) {
        session_icon = gfx::CreateVectorIcon(message_center::kProductIcon,
                                             kIconSize, gfx::kChromeIconGrey);
      }
      header_row_->SetAppIcon(session_icon);
    }
  }
}

void LockScreenMediaControlsView::OnImplicitAnimationsCompleted() {
  Dismiss();
}

void LockScreenMediaControlsView::ButtonPressed(views::Button* sender,
                                                const ui::Event& event) {
  if (!base::Contains(enabled_actions_,
                      media_message_center::GetActionFromButtonTag(*sender)) ||
      !media_session_id_.has_value()) {
    return;
  }

  media_session::PerformMediaSessionAction(
      media_message_center::GetActionFromButtonTag(*sender),
      media_controller_remote_);
}

void LockScreenMediaControlsView::OnGestureEvent(ui::GestureEvent* event) {
  gfx::Point point_in_screen = event->location();
  ConvertPointToScreen(this, &point_in_screen);

  switch (event->type()) {
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      if (is_in_drag_)
        break;

      initial_drag_point_ = point_in_screen;
      is_in_drag_ = true;
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      last_fling_velocity_ = event->details().scroll_y();
      UpdateDrag(point_in_screen);
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_END: {
      if (!is_in_drag_)
        break;

      EndDrag();
      event->SetHandled();
      break;
    }
    default:
      break;
  }
}

void LockScreenMediaControlsView::FlushForTesting() {
  media_controller_remote_.FlushForTesting();
}

void LockScreenMediaControlsView::CreateMediaButton(
    int size,
    MediaSessionAction action,
    const base::string16& accessible_name) {
  auto button = views::CreateVectorImageButton(this);
  button->set_tag(static_cast<int>(action));
  button->SetPreferredSize(kMediaButtonSize);
  button->SetAccessibleName(accessible_name);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  views::SetImageFromVectorIcon(button.get(),
                                GetVectorIconForMediaAction(action), size,
                                kMediaButtonColor);

  button_row_->AddChildView(std::move(button));
}

void LockScreenMediaControlsView::UpdateActionButtonsVisibility() {
  std::set<MediaSessionAction> ignored_actions = {
      media_message_center::GetPlayPauseIgnoredAction(
          media_message_center::GetActionFromButtonTag(*play_pause_button_))};

  std::set<MediaSessionAction> visible_actions =
      media_message_center::GetTopVisibleActions(enabled_actions_,
                                                 ignored_actions, kMaxActions);

  for (auto* view : button_row_->children()) {
    views::Button* action_button = views::Button::AsButton(view);
    bool should_show = base::Contains(
        visible_actions,
        media_message_center::GetActionFromButtonTag(*action_button));

    action_button->SetVisible(should_show);
  }

  PreferredSizeChanged();
}

void LockScreenMediaControlsView::SetIsPlaying(bool playing) {
  MediaSessionAction action =
      playing ? MediaSessionAction::kPause : MediaSessionAction::kPlay;

  play_pause_button_->SetToggled(!playing);
  play_pause_button_->set_tag(static_cast<int>(action));

  UpdateActionButtonsVisibility();
}

void LockScreenMediaControlsView::SeekTo(double seek_progress) {
  DCHECK(position_.has_value());

  media_controller_remote_->SeekTo(seek_progress * position_->duration());
}

void LockScreenMediaControlsView::Dismiss() {
  media_controller_remote_->Stop();
  hide_media_controls_.Run();
}

void LockScreenMediaControlsView::SetArtwork(
    base::Optional<gfx::ImageSkia> img) {
  if (!img.has_value()) {
    session_artwork_->SetImage(nullptr);
    return;
  }

  session_artwork_->SetImageSize(ScaleSizeToFitView(img->size(), kArtworkSize));
  session_artwork_->SetImage(*img);
}

void LockScreenMediaControlsView::UpdateDrag(
    const gfx::Point& location_in_screen) {
  is_in_drag_ = true;
  int drag_delta = location_in_screen.y() - initial_drag_point_.y();

  // Don't let the user drag |contents_view_| below the view area.
  if (contents_view_->bounds().bottom() + drag_delta >=
      GetLocalBounds().bottom()) {
    return;
  }

  gfx::Transform transform;
  transform.Translate(0, drag_delta);
  contents_view_->layer()->SetTransform(transform);
  UpdateOpacity();
}

void LockScreenMediaControlsView::EndDrag() {
  is_in_drag_ = false;

  // If the user releases the drag with velocity over the threshold or drags
  // |contents_view_| past the height threshold, dismiss the controls.
  if (last_fling_velocity_ <= kDragVelocityThreshold ||
      (contents_view_->GetBoundsInScreen().y() <= kHeightDismissalThreshold &&
       last_fling_velocity_ < 0)) {
    RunHideControlsAnimation();
    return;
  }

  RunResetControlsAnimation();
}

void LockScreenMediaControlsView::UpdateOpacity() {
  float progress =
      static_cast<float>(contents_view_->GetBoundsInScreen().bottom()) /
      GetBoundsInScreen().bottom();
  contents_view_->layer()->SetOpacity(progress);
}

void LockScreenMediaControlsView::RunHideControlsAnimation() {
  ui::ScopedLayerAnimationSettings animation(
      contents_view_->layer()->GetAnimator());
  animation.AddObserver(this);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animation.SetTransitionDuration(kAnimationDuration);

  gfx::Transform transform;
  transform.Translate(0, -GetBoundsInScreen().bottom());
  contents_view_->layer()->SetTransform(transform);
  contents_view_->layer()->SetOpacity(0);
}

void LockScreenMediaControlsView::RunResetControlsAnimation() {
  ui::ScopedLayerAnimationSettings animation(
      contents_view_->layer()->GetAnimator());
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animation.SetTransitionDuration(kAnimationDuration);

  contents_view_->layer()->SetTransform(gfx::Transform());
  contents_view_->layer()->SetOpacity(1);
}

}  // namespace ash
