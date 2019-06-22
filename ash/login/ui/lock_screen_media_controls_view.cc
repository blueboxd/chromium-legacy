// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_controls_view.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/media/media_controller_impl.h"
#include "ash/shell.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/views/background.h"

namespace ash {

namespace {
// Total width of the media controls view.
const int kMediaControlsTotalWidthDp = 320;

// Total height of the media controls view.
const int kMediaControlsTotalHeightDp = 400;

// How long to wait (in milliseconds) for a new media session to begin.
constexpr base::TimeDelta kNextMediaDelay =
    base::TimeDelta::FromMilliseconds(2500);

constexpr const char kLockScreenMediaControlsViewName[] =
    "LockScreenMediaControlsView";
}  // namespace

LockScreenMediaControlsView::LockScreenMediaControlsView(
    service_manager::Connector* connector,
    LockContentsView* view)
    : view_(view),
      connector_(connector),
      hide_controls_timer_(new base::OneShotTimer()) {
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
  middle_spacing_ = std::make_unique<NonAccessibleView>();
  middle_spacing_->set_owned_by_client();

  // Media controls have not been dismissed initially.
  Shell::Get()->media_controller()->SetMediaControlsDismissed(false);

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
}

LockScreenMediaControlsView::~LockScreenMediaControlsView() = default;

// views::View:
gfx::Size LockScreenMediaControlsView::CalculatePreferredSize() const {
  return gfx::Size(kMediaControlsTotalWidthDp, kMediaControlsTotalHeightDp);
}

const char* LockScreenMediaControlsView::GetClassName() const {
  return kLockScreenMediaControlsViewName;
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

}  // namespace ash
