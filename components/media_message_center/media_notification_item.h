// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ITEM_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ITEM_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace media_message_center {

class MediaNotificationController;
class MediaNotificationView;

// MediaNotificationItem manages hiding/showing a media notification and
// updating the metadata for a single media session.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationItem
    : public media_session::mojom::MediaControllerObserver,
      public media_session::mojom::MediaControllerImageObserver {
 public:
  // The name of the histogram used when recording user actions.
  static const char kUserActionHistogramName[];

  // The name of the histogram used when recording the source.
  static const char kSourceHistogramName[];

  // The source of the media session. This is used in metrics so new values must
  // only be added to the end.
  enum class Source {
    kUnknown,
    kWeb,
    kAssistant,
    kArc,
    kMaxValue = kArc,
  };

  MediaNotificationItem(MediaNotificationController* notification_controller,
                        const std::string& request_id,
                        const std::string& source_name,
                        media_session::mojom::MediaControllerPtr controller,
                        media_session::mojom::MediaSessionInfoPtr session_info);
  ~MediaNotificationItem() override;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

  // media_session::mojom::MediaControllerImageObserver:
  void MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType type,
      const SkBitmap& bitmap) override;

  // Called by MediaNotificationView when created or destroyed.
  void SetView(MediaNotificationView* view);

  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action);

  base::WeakPtr<MediaNotificationItem> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void FlushForTesting();

  void SetMediaControllerForTesting(
      media_session::mojom::MediaControllerPtr controller) {
    media_controller_ptr_ = std::move(controller);
  }

 private:
  void MaybeHideOrShowNotification();

  void HideNotification();

  MediaNotificationController* controller_;

  // Weak reference to the view of the currently shown media notification.
  MediaNotificationView* view_ = nullptr;

  // The |request_id_| is the request id of the media session and is guaranteed
  // to be globally unique.
  const std::string request_id_;

  // The source of the media session (e.g. arc, web).
  const Source source_;

  media_session::mojom::MediaControllerPtr media_controller_ptr_;

  media_session::mojom::MediaSessionInfoPtr session_info_;

  media_session::MediaMetadata session_metadata_;

  std::set<media_session::mojom::MediaSessionAction> session_actions_;

  base::Optional<gfx::ImageSkia> session_artwork_;

  // True if the metadata needs to be updated on |view_|. Used to prevent
  // updating |view_|'s metadata twice on a single change.
  bool view_needs_metadata_update_ = false;

  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      observer_receiver_{this};

  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      artwork_observer_receiver_{this};

  base::WeakPtrFactory<MediaNotificationItem> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationItem);
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ITEM_H_
