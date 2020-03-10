// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_RECORD_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_RECORD_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "chrome/browser/media/router/providers/cast/activity_record.h"
#include "chrome/browser/media/router/providers/cast/cast_media_controller.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "components/cast_channel/cast_message_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace url {
class Origin;
}

namespace media_router {

class CastActivityRecord;
class CastInternalMessage;
class CastSession;
class CastSessionTracker;
class MediaRoute;

class CastActivityRecordFactoryForTest {
 public:
  virtual std::unique_ptr<CastActivityRecord> MakeCastActivityRecord(
      const MediaRoute& route,
      const std::string& app_id) = 0;
};

class CastActivityRecord : public ActivityRecord {
 public:
  // Creates a new record owned by |owner|.
  CastActivityRecord(const MediaRoute& route,
                     const std::string& app_id,
                     cast_channel::CastMessageHandler* message_handler,
                     CastSessionTracker* session_tracker);
  ~CastActivityRecord() override;

  // ActivityRecord implementation
  void SetOrUpdateSession(const CastSession& session,
                          const MediaSinkInternal& sink,
                          const std::string& hash_token) override;
  void SendMediaStatusToClients(const base::Value& media_status,
                                base::Optional<int> request_id) override;
  void OnAppMessage(const cast::channel::CastMessage& message) override;
  void OnInternalMessage(const cast_channel::InternalMessage& message) override;
  void CreateMediaController(
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) override;
  base::Optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message) override;
  cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message) override;
  void SendSetVolumeRequestToReceiver(
      const CastInternalMessage& cast_message,
      cast_channel::ResultCallback callback) override;

  bool CanJoinSession(const CastMediaSource& cast_source, bool incognito) const;
  bool HasJoinableClient(AutoJoinPolicy policy,
                         const url::Origin& origin,
                         int tab_id) const;

 private:
  friend class CastSessionClientImpl;
  friend class CastActivityManager;
  friend class CastActivityRecordTest;

  std::unique_ptr<CastMediaController> media_controller_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_RECORD_H_
