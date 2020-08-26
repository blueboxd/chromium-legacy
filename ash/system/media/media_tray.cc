// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_tray.h"

#include "ash/public/cpp/media_notification_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "base/strings/string_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

MediaTray::MediaTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  DCHECK(MediaNotificationProvider::Get());
  MediaNotificationProvider::Get()->AddObserver(this);

  auto icon = std::make_unique<views::ImageView>();
  icon->set_tooltip_text(l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT));
  icon->SetImage(gfx::CreateVectorIcon(
      kGlobalMediaControlsIcon,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState())));

  tray_container()->SetMargin(kMediaTrayPadding, 0);
  icon_ = tray_container()->AddChildView(std::move(icon));
}

MediaTray::~MediaTray() {
  if (MediaNotificationProvider::Get())
    MediaNotificationProvider::Get()->RemoveObserver(this);
}

void MediaTray::OnNotificationListChanged() {
  UpdateDisplayState();
}

void MediaTray::OnNotificationListViewSizeChanged() {}

base::string16 MediaTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT);
}

void MediaTray::UpdateAfterLoginStatusChange() {
  UpdateDisplayState();
  PreferredSizeChanged();
}

void MediaTray::HandleLocaleChange() {
  icon_->set_tooltip_text(l10n_util::GetStringUTF16(
      IDS_ASH_GLOBAL_MEDIA_CONTROLS_BUTTON_TOOLTIP_TEXT));
}

void MediaTray::UpdateDisplayState() {
  if (!MediaNotificationProvider::Get())
    return;

  SetVisiblePreferred(
      MediaNotificationProvider::Get()->HasActiveNotifications() ||
      MediaNotificationProvider::Get()->HasFrozenNotifications());
}

}  // namespace ash
