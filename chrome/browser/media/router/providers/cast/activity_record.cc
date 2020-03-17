// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/activity_record.h"

#include "base/logging.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"

namespace media_router {

ActivityRecord::ActivityRecord(
    const MediaRoute& route,
    const std::string& app_id,
    cast_channel::CastMessageHandler* message_handler,
    CastSessionTracker* session_tracker)
    : route_(route),
      app_id_(app_id),
      message_handler_(message_handler),
      session_tracker_(session_tracker) {}

ActivityRecord::~ActivityRecord() = default;

mojom::RoutePresentationConnectionPtr ActivityRecord::AddClient(
    const CastMediaSource& source,
    const url::Origin& origin,
    int tab_id) {
  const std::string& client_id = source.client_id();
  DCHECK(!base::Contains(connected_clients_, client_id));
  std::unique_ptr<CastSessionClient> client =
      client_factory_for_test_
          ? client_factory_for_test_->MakeClientForTest(client_id, origin,
                                                        tab_id)
          : std::make_unique<CastSessionClientImpl>(
                client_id, origin, tab_id, source.auto_join_policy(), this);
  auto presentation_connection = client->Init();
  connected_clients_.emplace(client_id, std::move(client));

  // Route is now local due to connected client.
  route_.set_local(true);
  return presentation_connection;
}

void ActivityRecord::RemoveClient(const std::string& client_id) {
  // Don't erase by key here as the |client_id| may be referring to the
  // client being deleted.
  auto it = connected_clients_.find(client_id);
  if (it != connected_clients_.end())
    connected_clients_.erase(it);
}

CastSession* ActivityRecord::GetSession() const {
  DCHECK(session_id_);
  CastSession* session = session_tracker_->GetSessionById(*session_id_);
  if (!session) {
    // TODO(crbug.com/905002): Add UMA metrics for this and other error
    // conditions.
    LOG(ERROR) << "Session not found: " << session_id_.value_or("<missing>");
  }
  return session;
}

void ActivityRecord::SetOrUpdateSession(const CastSession& session,
                                        const MediaSinkInternal& sink,
                                        const std::string& hash_token) {
  DVLOG(2) << "SetOrUpdateSession old session_id = "
           << session_id_.value_or("<missing>")
           << ", new session_id = " << session.session_id();
  route_.set_description(session.GetRouteDescription());
  sink_ = sink;
  if (session_id_) {
    DCHECK_EQ(*session_id_, session.session_id());
  } else {
    session_id_ = session.session_id();
    if (on_session_set_)
      std::move(on_session_set_).Run();
  }
}

void ActivityRecord::SendStopSessionMessageToClients(
    const std::string& hash_token) {
  for (const auto& client : connected_clients_) {
    client.second->SendMessageToClient(
        CreateReceiverActionStopMessage(client.first, sink_, hash_token));
  }
}

void ActivityRecord::SendMessageToClient(
    const std::string& client_id,
    blink::mojom::PresentationConnectionMessagePtr message) {
  auto it = connected_clients_.find(client_id);
  if (it == connected_clients_.end()) {
    DLOG(ERROR) << "Attempting to send message to nonexistent client: "
                << client_id;
    return;
  }
  it->second->SendMessageToClient(std::move(message));
}

void ActivityRecord::SendMediaStatusToClients(const base::Value& media_status,
                                              base::Optional<int> request_id) {
  for (auto& client : connected_clients_)
    client.second->SendMediaStatusToClient(media_status, request_id);
}

void ActivityRecord::ClosePresentationConnections(
    blink::mojom::PresentationConnectionCloseReason close_reason) {
  for (auto& client : connected_clients_)
    client.second->CloseConnection(close_reason);
}

void ActivityRecord::TerminatePresentationConnections() {
  for (auto& client : connected_clients_)
    client.second->TerminateConnection();
}

base::Optional<int> ActivityRecord::SendMediaRequestToReceiver(
    const CastInternalMessage& cast_message) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

cast_channel::Result ActivityRecord::SendAppMessageToReceiver(
    const CastInternalMessage& cast_message) {
  NOTIMPLEMENTED();
  return cast_channel::Result::kFailed;
}

void ActivityRecord::SendSetVolumeRequestToReceiver(
    const CastInternalMessage& cast_message,
    cast_channel::ResultCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(cast_channel::Result::kFailed);
}

void ActivityRecord::StopSessionOnReceiver(
    const std::string& client_id,
    cast_channel::ResultCallback callback) {
  if (!session_id_)
    std::move(callback).Run(cast_channel::Result::kFailed);

  message_handler_->StopSession(cast_channel_id(), *session_id_, client_id,
                                std::move(callback));
}

void ActivityRecord::CloseConnectionOnReceiver(const std::string& client_id) {
  CastSession* session = GetSession();
  if (!session)
    return;
  message_handler_->CloseConnection(cast_channel_id(), client_id,
                                    session->transport_id());
}

void ActivityRecord::HandleLeaveSession(const std::string& client_id) {
  auto client_it = connected_clients_.find(client_id);
  CHECK(client_it != connected_clients_.end());
  auto& client = *client_it->second;
  std::vector<std::string> leaving_client_ids;
  for (const auto& pair : connected_clients_) {
    if (pair.second->MatchesAutoJoinPolicy(client.origin(), client.tab_id()))
      leaving_client_ids.push_back(pair.first);
  }

  for (const auto& client_id : leaving_client_ids) {
    auto leaving_client_it = connected_clients_.find(client_id);
    CHECK(leaving_client_it != connected_clients_.end());
    leaving_client_it->second->CloseConnection(
        blink::mojom::PresentationConnectionCloseReason::CLOSED);
    connected_clients_.erase(leaving_client_it);
  }
}

CastSessionClientFactoryForTest* ActivityRecord::client_factory_for_test_ =
    nullptr;

}  // namespace media_router
