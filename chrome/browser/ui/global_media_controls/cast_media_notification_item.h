// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojom/media_status.mojom.h"
#include "components/media_message_center/media_notification_item.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/cpp/media_metadata.h"

namespace media_message_center {
class MediaNotificationController;
}  // namespace media_message_center

// Represents the media notification shown in the Global Media Controls dialog
// for a Cast session. It is responsible for showing/hiding a
// MediaNotificationView.
class CastMediaNotificationItem
    : public media_message_center::MediaNotificationItem,
      public media_router::mojom::MediaStatusObserver {
 public:
  CastMediaNotificationItem(
      const media_router::MediaRoute& route,
      media_message_center::MediaNotificationController*
          notification_controller,
      std::unique_ptr<CastMediaSessionController> session_controller);
  CastMediaNotificationItem(const CastMediaNotificationItem&) = delete;
  CastMediaNotificationItem& operator=(const CastMediaNotificationItem&) =
      delete;
  ~CastMediaNotificationItem() override;

  // media_message_center::MediaNotificationItem:
  void SetView(media_message_center::MediaNotificationView* view) override;
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) override;
  void Dismiss() override;

  // media_router::mojom::MediaStatusObserver:
  void OnMediaStatusUpdated(
      media_router::mojom::MediaStatusPtr status) override;

  // Returns a pending remote bound to |this|. This should not be called more
  // than once per instance.
  mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
  GetObserverPendingRemote();

  base::WeakPtr<CastMediaNotificationItem> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void UpdateView();

  media_message_center::MediaNotificationController* const
      notification_controller_;
  media_message_center::MediaNotificationView* view_ = nullptr;

  std::unique_ptr<CastMediaSessionController> session_controller_;
  const media_router::MediaRoute::Id media_route_id_;
  media_session::MediaMetadata metadata_;
  std::vector<media_session::mojom::MediaSessionAction> actions_;
  media_session::mojom::MediaSessionInfoPtr session_info_;
  mojo::Receiver<media_router::mojom::MediaStatusObserver> observer_receiver_{
      this};
  base::WeakPtrFactory<CastMediaNotificationItem> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_
