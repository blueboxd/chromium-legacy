// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mirroring_activity_record.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "chrome/common/media_router/route_request_result.h"
#include "components/cast_channel/cast_message_util.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/enum_table.h"
#include "components/mirroring/mojom/session_parameters.mojom-forward.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_address.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

using blink::mojom::PresentationConnectionMessagePtr;
using cast_channel::Result;
using media_router::mojom::MediaRouteProvider;
using media_router::mojom::MediaRouter;
using mirroring::mojom::SessionError;
using mirroring::mojom::SessionParameters;
using mirroring::mojom::SessionType;

namespace media_router {

namespace {

constexpr char kHistogramSessionLaunch[] =
    "MediaRouter.CastStreaming.Session.Launch";
constexpr char kHistogramSessionLength[] =
    "MediaRouter.CastStreaming.Session.Length";
constexpr char kHistogramStartFailureNative[] =
    "MediaRouter.CastStreaming.Start.Failure.Native";
constexpr char kHistogramStartSuccess[] =
    "MediaRouter.CastStreaming.Start.Success";

using MirroringType = MirroringActivityRecord::MirroringType;

const std::string GetMirroringNamespace(const base::Value& message) {
  const base::Value* const type_value =
      message.FindKeyOfType("type", base::Value::Type::STRING);

  if (type_value &&
      type_value->GetString() ==
          cast_util::EnumToString<cast_channel::CastMessageType,
                                  cast_channel::CastMessageType::kRpc>()) {
    return mirroring::mojom::kRemotingNamespace;
  } else {
    return mirroring::mojom::kWebRtcNamespace;
  }
}

MirroringType GetMirroringType(const MediaRoute& route, int tab_id) {
  if (!route.is_local())
    return MirroringType::kNonLocal;
  return tab_id == -1 ? MirroringType::kDesktop : MirroringType::kTab;
}

}  // namespace

MirroringActivityRecord::MirroringActivityRecord(
    const MediaRoute& route,
    const std::string& app_id,
    cast_channel::CastMessageHandler* message_handler,
    CastSessionTracker* session_tracker,
    int target_tab_id,
    const CastSinkExtraData& cast_data,
    mojom::MediaRouter* media_router,
    OnStopCallback callback)
    : ActivityRecord(route, app_id, message_handler, session_tracker),
      channel_id_(cast_data.cast_channel_id),
      // TODO(jrw): MirroringType::kOffscreenTab should be a possible value here
      // once the Presentation API 1UA mode is supported.
      mirroring_type_(GetMirroringType(route, target_tab_id)),
      on_stop_(std::move(callback)) {
  // TODO(jrw): Detect and report errors.

  mirroring_tab_id_ = target_tab_id;

  // Get a reference to the mirroring service host.
  switch (mirroring_type_) {
    case MirroringType::kDesktop: {
      auto stream_id = route.media_source().DesktopStreamId();
      DCHECK(stream_id);
      media_router->GetMirroringServiceHostForDesktop(
          /* tab_id */ -1, *stream_id, host_.BindNewPipeAndPassReceiver());
      break;
    }
    case MirroringType::kTab:
      media_router->GetMirroringServiceHostForTab(
          target_tab_id, host_.BindNewPipeAndPassReceiver());
      break;
    case MirroringType::kNonLocal:
      // Non-local activity doesn't need to handle messages, so return without
      // setting up Mojo bindings.
      return;
    default:
      NOTREACHED();
  }

  // Derive session type from capabilities.
  const bool has_audio = (cast_data.capabilities &
                          static_cast<uint8_t>(cast_channel::AUDIO_OUT)) != 0;
  const bool has_video = (cast_data.capabilities &
                          static_cast<uint8_t>(cast_channel::VIDEO_OUT)) != 0;
  DCHECK(has_audio || has_video);
  const SessionType session_type =
      has_audio && has_video
          ? SessionType::AUDIO_AND_VIDEO
          : has_audio ? SessionType::AUDIO_ONLY : SessionType::VIDEO_ONLY;

  // Arrange to start mirroring once the session is set.
  on_session_set_ = base::BindOnce(
      &MirroringActivityRecord::StartMirroring, base::Unretained(this),
      SessionParameters::New(session_type, cast_data.ip_endpoint.address(),
                             cast_data.model_name),
      channel_to_service_.BindNewPipeAndPassReceiver());
}

MirroringActivityRecord::~MirroringActivityRecord() {
  if (did_start_mirroring_timestamp_) {
    base::UmaHistogramLongTimes(
        kHistogramSessionLength,
        base::Time::Now() - *did_start_mirroring_timestamp_);
  }
}

void MirroringActivityRecord::OnError(SessionError error) {
  if (will_start_mirroring_timestamp_) {
    // An error was encountered while attempting to start mirroring.
    base::UmaHistogramEnumeration(kHistogramStartFailureNative, error);
    will_start_mirroring_timestamp_.reset();
  }
  // Metrics for general errors are captured by the mirroring service in
  // MediaRouter.MirroringService.SessionError.
  StopMirroring();
}

void MirroringActivityRecord::DidStart() {
  if (!will_start_mirroring_timestamp_) {
    // DidStart() was called unexpectedly.
    return;
  }
  did_start_mirroring_timestamp_ = base::Time::Now();
  base::UmaHistogramTimes(
      kHistogramSessionLaunch,
      *did_start_mirroring_timestamp_ - *will_start_mirroring_timestamp_);
  base::UmaHistogramEnumeration(kHistogramStartSuccess, mirroring_type_);
  will_start_mirroring_timestamp_.reset();
}

void MirroringActivityRecord::DidStop() {
  StopMirroring();
}

void MirroringActivityRecord::Send(mirroring::mojom::CastMessagePtr message) {
  DCHECK(message);
  DVLOG(2) << "Relaying message to receiver: " << message->json_format_data;

  GetDataDecoder().ParseJson(
      message->json_format_data,
      base::BindOnce(&MirroringActivityRecord::HandleParseJsonResult,
                     weak_ptr_factory_.GetWeakPtr(), route().media_route_id()));
}

void MirroringActivityRecord::OnAppMessage(
    const cast::channel::CastMessage& message) {
  if (!route_.is_local())
    return;
  if (message.namespace_() != mirroring::mojom::kWebRtcNamespace &&
      message.namespace_() != mirroring::mojom::kRemotingNamespace) {
    // Ignore message with wrong namespace.
    DVLOG(2) << "Ignoring message with namespace " << message.namespace_();
    return;
  }
  DVLOG(2) << "Relaying app message from receiver: " << message.DebugString();
  DCHECK(message.has_payload_utf8());
  DCHECK_EQ(message.protocol_version(),
            cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  mirroring::mojom::CastMessagePtr ptr = mirroring::mojom::CastMessage::New();
  ptr->message_namespace = message.namespace_();
  ptr->json_format_data = message.payload_utf8();
  // TODO(jrw): Do something with message.source_id() and
  // message.destination_id()?
  channel_to_service_->Send(std::move(ptr));
}

void MirroringActivityRecord::OnInternalMessage(
    const cast_channel::InternalMessage& message) {
  if (!route_.is_local())
    return;
  DVLOG(2) << "Relaying internal message from receiver: " << message.message;
  mirroring::mojom::CastMessagePtr ptr = mirroring::mojom::CastMessage::New();
  ptr->message_namespace = message.message_namespace;

  // TODO(jrw): This line re-serializes a JSON string that was parsed by the
  // caller of this method.  Yuck!  This is probably a necessary evil as long as
  // the extension needs to communicate with the mirroring service.
  CHECK(base::JSONWriter::Write(message.message, &ptr->json_format_data));

  channel_to_service_->Send(std::move(ptr));
}

void MirroringActivityRecord::CreateMediaController(
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {}

void MirroringActivityRecord::HandleParseJsonResult(
    const std::string& route_id,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    // TODO(crbug.com/905002): Record UMA metric for parse result.
    DLOG(ERROR) << "Failed to parse Cast client message for " << route_id
                << ": " << *result.error;
    return;
  }

  CastSession* session = GetSession();
  DCHECK(session);

  const std::string message_namespace = GetMirroringNamespace(*result.value);

  // TODO(jrw): Can some of this logic be shared with
  // CastActivityRecord::SendAppMessageToReceiver?
  cast::channel::CastMessage cast_message = cast_channel::CreateCastMessage(
      message_namespace, std::move(*result.value),
      message_handler_->sender_id(), session->transport_id());
  message_handler_->SendCastMessage(channel_id_, cast_message);
}

void MirroringActivityRecord::StartMirroring(
    mirroring::mojom::SessionParametersPtr session_params,
    mojo::PendingReceiver<CastMessageChannel> channel_to_service) {
  will_start_mirroring_timestamp_ = base::Time::Now();

  // Bind Mojo receivers for the interfaces this object implements.
  mojo::PendingRemote<mirroring::mojom::SessionObserver> observer_remote;
  observer_receiver_.Bind(observer_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<mirroring::mojom::CastMessageChannel> channel_remote;
  channel_receiver_.Bind(channel_remote.InitWithNewPipeAndPassReceiver());

  host_->Start(std::move(session_params), std::move(observer_remote),
               std::move(channel_remote), std::move(channel_to_service));
}

void MirroringActivityRecord::StopMirroring() {
  // Running the callback will cause this object to be deleted.
  if (on_stop_)
    std::move(on_stop_).Run();
}

}  // namespace media_router
