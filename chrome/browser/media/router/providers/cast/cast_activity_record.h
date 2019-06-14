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
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "components/cast_channel/cast_message_handler.h"

namespace url {
class Origin;
}

namespace media_router {

class CastActivityManagerBase;
class CastActivityRecord;
class CastInternalMessage;
class CastSession;
class CastSessionClient;
class CastSessionClientFactoryForTest;
class CastSessionTracker;
class DataDecoder;
class MediaSinkServiceBase;
class MediaRoute;

class CastActivityRecordFactoryForTest {
 public:
  virtual std::unique_ptr<ActivityRecord> MakeCastActivityRecord(
      const MediaRoute& route,
      const std::string& app_id) = 0;
};

class CastActivityRecord : public ActivityRecord {
 public:
  // Creates a new record owned by |owner|.
  CastActivityRecord(const MediaRoute& route,
                     const std::string& app_id,
                     MediaSinkServiceBase* media_sink_service,
                     cast_channel::CastMessageHandler* message_handler,
                     CastSessionTracker* session_tracker,
                     DataDecoder* data_decoder,
                     CastActivityManagerBase* owner);
  ~CastActivityRecord() override;

  // ActivityRecord implementation
  cast_channel::Result SendAppMessageToReceiver(
      const CastInternalMessage& cast_message) override;
  base::Optional<int> SendMediaRequestToReceiver(
      const CastInternalMessage& cast_message) override;
  void SendSetVolumeRequestToReceiver(
      const CastInternalMessage& cast_message,
      cast_channel::ResultCallback callback) override;
  void SendStopSessionMessageToReceiver(
      const base::Optional<std::string>& client_id,
      const std::string& hash_token,
      mojom::MediaRouteProvider::TerminateRouteCallback callback) override;
  void HandleLeaveSession(const std::string& client_id) override;
  mojom::RoutePresentationConnectionPtr AddClient(const CastMediaSource& source,
                                                  const url::Origin& origin,
                                                  int tab_id) override;
  void RemoveClient(const std::string& client_id) override;
  void SetOrUpdateSession(const CastSession& session,
                          const MediaSinkInternal& sink,
                          const std::string& hash_token) override;
  void SendMessageToClient(
      const std::string& client_id,
      blink::mojom::PresentationConnectionMessagePtr message) override;
  void SendMediaStatusToClients(const base::Value& media_status,
                                base::Optional<int> request_id) override;
  void ClosePresentationConnections(
      blink::mojom::PresentationConnectionCloseReason close_reason) override;
  void TerminatePresentationConnections() override;
  void OnAppMessage(const cast_channel::CastMessage& message) override;
  void OnInternalMessage(const cast_channel::InternalMessage& message) override;

  static void SetClientFactoryForTest(
      CastSessionClientFactoryForTest* factory) {
    client_factory_for_test_ = factory;
  }

 private:
  friend class CastSessionClientImpl;
  friend class CastActivityManager;
  friend class CastActivityRecordTest;

  int GetCastChannelId();

  CastSessionClient* GetClient(const std::string& client_id) {
    auto it = connected_clients_.find(client_id);
    return it == connected_clients_.end() ? nullptr : it->second.get();
  }

  static CastSessionClientFactoryForTest* client_factory_for_test_;

  MediaSinkServiceBase* const media_sink_service_;
  CastActivityManagerBase* const activity_manager_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_ACTIVITY_RECORD_H_
