// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/peer_connection_tracker.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/renderer/media/webrtc/rtc_peer_connection_handler.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_rtc_answer_options.h"
#include "third_party/blink/public/platform/web_rtc_ice_candidate.h"
#include "third_party/blink/public/platform/web_rtc_offer_options.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_user_media_request.h"

using webrtc::StatsReport;
using webrtc::StatsReports;
using blink::WebRTCPeerConnectionHandlerClient;

namespace content {

// TODO(hta): This module should be redesigned to reduce string copies.

static const char* SerializeBoolean(bool value) {
  return value ? "true" : "false";
}

static std::string SerializeServers(
    const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers) {
  std::string result = "[";
  bool following = false;
  for (const auto& server : servers) {
    for (const auto& url : server.urls) {
      if (following)
        result += ", ";
      else
        following = true;

      result += url;
    }
  }
  result += "]";
  return result;
}

static std::string SerializeMediaConstraints(
    const blink::WebMediaConstraints& constraints) {
  return constraints.ToString().Utf8();
}

static std::string SerializeOfferOptions(
    const blink::WebRTCOfferOptions& options) {
  if (options.IsNull())
    return "null";

  std::ostringstream result;
  result << "offerToReceiveVideo: " << options.OfferToReceiveVideo()
         << ", offerToReceiveAudio: " << options.OfferToReceiveAudio()
         << ", voiceActivityDetection: "
         << SerializeBoolean(options.VoiceActivityDetection())
         << ", iceRestart: " << SerializeBoolean(options.IceRestart());
  return result.str();
}

static std::string SerializeAnswerOptions(
    const blink::WebRTCAnswerOptions& options) {
  if (options.IsNull())
    return "null";

  std::ostringstream result;
  result << ", voiceActivityDetection: "
         << SerializeBoolean(options.VoiceActivityDetection());
  return result.str();
}

static std::string SerializeMediaStreamIds(
    const blink::WebVector<blink::WebString>& stream_ids) {
  if (!stream_ids.size())
    return "[]";
  std::string result = "[";
  for (const auto& stream_id : stream_ids) {
    if (result.size() > 2u)
      result += ",";
    result += "'" + stream_id.Utf8() + "'";
  }
  result += "]";
  return result;
}

static const char* SerializeDirection(
    webrtc::RtpTransceiverDirection direction) {
  switch (direction) {
    case webrtc::RtpTransceiverDirection::kSendRecv:
      return "'sendrecv'";
    case webrtc::RtpTransceiverDirection::kSendOnly:
      return "'sendonly'";
    case webrtc::RtpTransceiverDirection::kRecvOnly:
      return "'recvonly'";
    case webrtc::RtpTransceiverDirection::kInactive:
      return "'inactive'";
  }
}

static const char* SerializeOptionalDirection(
    const base::Optional<webrtc::RtpTransceiverDirection>& direction) {
  return direction ? SerializeDirection(*direction) : "null";
}

static std::string SerializeSender(const std::string& indent,
                                   const blink::WebRTCRtpSender& sender) {
  std::string result = "{\n";
  // track:'id',
  result += indent + "  track:";
  if (sender.Track().IsNull()) {
    result += "null";
  } else {
    result += "'" + sender.Track().Source().Id().Utf8() + "'";
  }
  result += ",\n";
  // streams:['id,'id'],
  result += indent +
            "  streams:" + SerializeMediaStreamIds(sender.StreamIds()) + ",\n";
  result += indent + "}";
  return result;
}

static std::string SerializeReceiver(const std::string& indent,
                                     const blink::WebRTCRtpReceiver& receiver) {
  std::string result = "{\n";
  // track:'id',
  DCHECK(!receiver.Track().IsNull());
  result +=
      indent + "  track:'" + receiver.Track().Source().Id().Utf8() + "',\n";
  // streams:['id,'id'],
  result += indent +
            "  streams:" + SerializeMediaStreamIds(receiver.StreamIds()) +
            ",\n";
  result += indent + "}";
  return result;
}

static std::string SerializeTransceiver(
    const blink::WebRTCRtpTransceiver& transceiver) {
  if (transceiver.ImplementationType() ==
      blink::WebRTCRtpTransceiverImplementationType::kFullTransceiver) {
    std::string result = "{\n";
    // mid:'foo',
    if (transceiver.Mid().IsNull())
      result += "  mid:null,\n";
    else
      result += "  mid:'" + transceiver.Mid().Utf8() + "',\n";
    // sender:{...},
    result +=
        "  sender:" + SerializeSender("  ", *transceiver.Sender()) + ",\n";
    // receiver:{...},
    result += "  receiver:" + SerializeReceiver("  ", *transceiver.Receiver()) +
              ",\n";
    // stopped:false,
    result += "  stopped:" +
              std::string(SerializeBoolean(transceiver.Stopped())) + ",\n";
    // direction:'sendrecv',
    result += "  direction:" +
              std::string(SerializeDirection(transceiver.Direction())) + ",\n";
    // currentDirection:null,
    result += "  currentDirection:" +
              std::string(
                  SerializeOptionalDirection(transceiver.CurrentDirection())) +
              ",\n";
    result += "}";
    return result;
  }
  if (transceiver.ImplementationType() ==
      blink::WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly) {
    return SerializeSender("", *transceiver.Sender());
  }
  DCHECK(transceiver.ImplementationType() ==
         blink::WebRTCRtpTransceiverImplementationType::kPlanBReceiverOnly);
  return SerializeReceiver("", *transceiver.Receiver());
}

static const char* SerializeIceTransportType(
    webrtc::PeerConnectionInterface::IceTransportsType type) {
  const char* transport_type = "";
  switch (type) {
  case webrtc::PeerConnectionInterface::kNone:
    transport_type = "none";
    break;
  case webrtc::PeerConnectionInterface::kRelay:
    transport_type = "relay";
    break;
  case webrtc::PeerConnectionInterface::kAll:
    transport_type = "all";
    break;
  case webrtc::PeerConnectionInterface::kNoHost:
    transport_type = "noHost";
    break;
  default:
    NOTREACHED();
  }
  return transport_type;
}

static const char* SerializeBundlePolicy(
    webrtc::PeerConnectionInterface::BundlePolicy policy) {
  const char* policy_str = "";
  switch (policy) {
  case webrtc::PeerConnectionInterface::kBundlePolicyBalanced:
    policy_str = "balanced";
    break;
  case webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle:
    policy_str = "max-bundle";
    break;
  case webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat:
    policy_str = "max-compat";
    break;
  default:
    NOTREACHED();
  }
  return policy_str;
}

static const char* SerializeRtcpMuxPolicy(
    webrtc::PeerConnectionInterface::RtcpMuxPolicy policy) {
  const char* policy_str = "";
  switch (policy) {
  case webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate:
    policy_str = "negotiate";
    break;
  case webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire:
    policy_str = "require";
    break;
  default:
    NOTREACHED();
  }
  return policy_str;
}

static const char* SerializeSdpSemantics(webrtc::SdpSemantics sdp_semantics) {
  const char* sdp_semantics_str = "";
  switch (sdp_semantics) {
    case webrtc::SdpSemantics::kPlanB:
      sdp_semantics_str = "plan-b";
      break;
    case webrtc::SdpSemantics::kUnifiedPlan:
      sdp_semantics_str = "unified-plan";
      break;
    default:
      NOTREACHED();
  }
  return sdp_semantics_str;
}

static std::string SerializeConfiguration(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config) {
  std::ostringstream oss;
  // TODO(hbos): Add serialization of certificate.
  oss << "{ iceServers: " << SerializeServers(config.servers)
      << ", iceTransportPolicy: " << SerializeIceTransportType(config.type)
      << ", bundlePolicy: " << SerializeBundlePolicy(config.bundle_policy)
      << ", rtcpMuxPolicy: " << SerializeRtcpMuxPolicy(config.rtcp_mux_policy)
      << ", iceCandidatePoolSize: " << config.ice_candidate_pool_size
      << ", sdpSemantics: \"" << SerializeSdpSemantics(config.sdp_semantics)
      << "\" }";
  return oss.str();
}

// Note: All of these strings need to be kept in sync with
// peer_connection_update_table.js, in order to be displayed as friendly
// strings on chrome://webrtc-internals.

static const char* GetSignalingStateString(
    webrtc::PeerConnectionInterface::SignalingState state) {
  const char* result = "";
  switch (state) {
    case webrtc::PeerConnectionInterface::SignalingState::kStable:
      return "SignalingStateStable";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveLocalOffer:
      return "SignalingStateHaveLocalOffer";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveRemoteOffer:
      return "SignalingStateHaveRemoteOffer";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveLocalPrAnswer:
      return "SignalingStateHaveLocalPrAnswer";
    case webrtc::PeerConnectionInterface::SignalingState::kHaveRemotePrAnswer:
      return "SignalingStateHaveRemotePrAnswer";
    case webrtc::PeerConnectionInterface::SignalingState::kClosed:
      return "SignalingStateClosed";
    default:
      NOTREACHED();
      break;
  }
  return result;
}

static const char* GetIceConnectionStateString(
    webrtc::PeerConnectionInterface::IceConnectionState state) {
  switch (state) {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
      return "new";
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
      return "checking";
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
      return "connected";
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
      return "completed";
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
      return "failed";
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
      return "disconnected";
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
      return "closed";
    default:
      NOTREACHED();
      return "";
  }
}

static const char* GetConnectionStateString(
    webrtc::PeerConnectionInterface::PeerConnectionState state) {
  switch (state) {
    case webrtc::PeerConnectionInterface::PeerConnectionState::kNew:
      return "new";
    case webrtc::PeerConnectionInterface::PeerConnectionState::kConnecting:
      return "connecting";
    case webrtc::PeerConnectionInterface::PeerConnectionState::kConnected:
      return "connected";
    case webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected:
      return "disconnected";
    case webrtc::PeerConnectionInterface::PeerConnectionState::kFailed:
      return "failed";
    case webrtc::PeerConnectionInterface::PeerConnectionState::kClosed:
      return "closed";
    default:
      NOTREACHED();
      return "";
  }
}

static const char* GetIceGatheringStateString(
    webrtc::PeerConnectionInterface::IceGatheringState state) {
  switch (state) {
    case webrtc::PeerConnectionInterface::kIceGatheringNew:
      return "new";
    case webrtc::PeerConnectionInterface::kIceGatheringGathering:
      return "gathering";
    case webrtc::PeerConnectionInterface::kIceGatheringComplete:
      return "complete";
    default:
      NOTREACHED();
      return "";
  }
}

static const char* GetTransceiverUpdatedReasonString(
    PeerConnectionTracker::TransceiverUpdatedReason reason) {
  switch (reason) {
    case PeerConnectionTracker::TransceiverUpdatedReason::kAddTransceiver:
      return "addTransceiver";
    case PeerConnectionTracker::TransceiverUpdatedReason::kAddTrack:
      return "addTrack";
    case PeerConnectionTracker::TransceiverUpdatedReason::kRemoveTrack:
      return "removeTrack";
    case PeerConnectionTracker::TransceiverUpdatedReason::kSetLocalDescription:
      return "setLocalDescription";
    case PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription:
      return "setRemoteDescription";
  }
  NOTREACHED();
  return nullptr;
}

// Builds a DictionaryValue from the StatsReport.
// Note:
// The format must be consistent with what webrtc_internals.js expects.
// If you change it here, you must change webrtc_internals.js as well.
static std::unique_ptr<base::DictionaryValue> GetDictValueStats(
    const StatsReport& report) {
  if (report.values().empty())
    return nullptr;

  auto values = std::make_unique<base::ListValue>();

  for (const auto& v : report.values()) {
    const StatsReport::ValuePtr& value = v.second;
    values->AppendString(value->display_name());
    switch (value->type()) {
      case StatsReport::Value::kInt:
        values->AppendInteger(value->int_val());
        break;
      case StatsReport::Value::kFloat:
        values->AppendDouble(value->float_val());
        break;
      case StatsReport::Value::kString:
        values->AppendString(value->string_val());
        break;
      case StatsReport::Value::kStaticString:
        values->AppendString(value->static_string_val());
        break;
      case StatsReport::Value::kBool:
        values->AppendBoolean(value->bool_val());
        break;
      case StatsReport::Value::kInt64:  // int64_t isn't supported, so use
                                        // string.
      case StatsReport::Value::kId:
      default:
        values->AppendString(value->ToString());
        break;
    }
  }

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetDouble("timestamp", report.timestamp());
  dict->Set("values", std::move(values));

  return dict;
}

// Builds a DictionaryValue from the StatsReport.
// The caller takes the ownership of the returned value.
static std::unique_ptr<base::DictionaryValue> GetDictValue(
    const StatsReport& report) {
  std::unique_ptr<base::DictionaryValue> stats = GetDictValueStats(report);
  if (!stats)
    return nullptr;

  // Note:
  // The format must be consistent with what webrtc_internals.js expects.
  // If you change it here, you must change webrtc_internals.js as well.
  auto result = std::make_unique<base::DictionaryValue>();
  result->Set("stats", std::move(stats));
  result->SetString("id", report.id()->ToString());
  result->SetString("type", report.TypeToString());

  return result;
}

// chrome://webrtc-internals displays stats and stats graphs. The call path
// involves thread and process hops (IPC). This is the webrtc::StatsObserver
// that is used when webrtc-internals wants legacy stats. It starts in
// webrtc_internals.js performing requestLegacyStats and the result gets
// asynchronously delivered to webrtc_internals.js at addLegacyStats.
class InternalLegacyStatsObserver : public webrtc::StatsObserver {
 public:
  InternalLegacyStatsObserver(
      int lid,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread)
      : lid_(lid), main_thread_(std::move(main_thread)) {}

  void OnComplete(const StatsReports& reports) override {
    std::unique_ptr<base::ListValue> list(new base::ListValue());

    for (const auto* r : reports) {
      std::unique_ptr<base::DictionaryValue> report = GetDictValue(*r);
      if (report)
        list->Append(std::move(report));
    }

    if (!list->empty()) {
      main_thread_->PostTask(
          FROM_HERE,
          base::BindOnce(&InternalLegacyStatsObserver::OnCompleteImpl,
                         std::move(list), lid_));
    }
  }

 protected:
  ~InternalLegacyStatsObserver() override {
    // Will be destructed on libjingle's signaling thread.
    // The signaling thread is where libjingle's objects live and from where
    // libjingle makes callbacks.  This may or may not be the same thread as
    // the main thread.
  }

 private:
  // Static since |this| will most likely have been deleted by the time we
  // get here.
  static void OnCompleteImpl(std::unique_ptr<base::ListValue> list, int lid) {
    DCHECK(!list->empty());
    RenderThreadImpl::current()->Send(
        new PeerConnectionTrackerHost_AddLegacyStats(lid, *list.get()));
  }

  const int lid_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
};

// chrome://webrtc-internals displays stats and stats graphs. The call path
// involves thread and process hops (IPC). This is the ----webrtc::StatsObserver
// that is used when webrtc-internals wants standard stats. It starts in
// webrtc_internals.js performing requestStandardStats and the result gets
// asynchronously delivered to webrtc_internals.js at addStandardStats.
class InternalStandardStatsObserver : public webrtc::RTCStatsCollectorCallback {
 public:
  InternalStandardStatsObserver(
      int lid,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread)
      : lid_(lid), main_thread_(std::move(main_thread)) {}

  void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override {
    // We're on the signaling thread.
    DCHECK(!main_thread_->BelongsToCurrentThread());
    main_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InternalStandardStatsObserver::OnStatsDeliveredOnMainThread,
            scoped_refptr<InternalStandardStatsObserver>(this), report));
  }

 protected:
  ~InternalStandardStatsObserver() override {}

 private:
  void OnStatsDeliveredOnMainThread(
      rtc::scoped_refptr<const webrtc::RTCStatsReport> report) {
    auto list = ReportToList(report);
    RenderThreadImpl::current()->Send(
        new PeerConnectionTrackerHost_AddStandardStats(lid_, *list.get()));
  }

  std::unique_ptr<base::ListValue> ReportToList(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
    std::unique_ptr<base::ListValue> result_list(new base::ListValue());
    for (const auto& stats : *report) {
      // The format of "stats_subdictionary" is:
      // {timestamp:<milliseconds>, values: [<key-value pairs>]}
      auto stats_subdictionary = std::make_unique<base::DictionaryValue>();
      // Timestamp is reported in milliseconds.
      stats_subdictionary->SetDouble("timestamp",
                                     stats.timestamp_us() / 1000.0);
      // Values are reported as
      // "values": ["member1", value, "member2", value...]
      auto name_value_pairs = std::make_unique<base::ListValue>();
      for (const auto* member : stats.Members()) {
        if (!member->is_defined())
          continue;
        name_value_pairs->AppendString(member->name());
        name_value_pairs->Append(MemberToValue(*member));
      }
      stats_subdictionary->Set("values", std::move(name_value_pairs));

      // The format of "stats_dictionary" is:
      // {id:<string>, stats:<stats_subdictionary>, type:<string>}
      auto stats_dictionary = std::make_unique<base::DictionaryValue>();
      stats_dictionary->Set("stats", std::move(stats_subdictionary));
      stats_dictionary->SetString("id", stats.id());
      stats_dictionary->SetString("type", stats.type());
      result_list->Append(std::move(stats_dictionary));
    }
    return result_list;
  }

  std::unique_ptr<base::Value> MemberToValue(
      const webrtc::RTCStatsMemberInterface& member) {
    switch (member.type()) {
      // Types supported by base::Value are passed as the appropriate type.
      case webrtc::RTCStatsMemberInterface::Type::kBool:
        return std::make_unique<base::Value>(
            *member.cast_to<webrtc::RTCStatsMember<bool>>());
      case webrtc::RTCStatsMemberInterface::Type::kInt32:
        return std::make_unique<base::Value>(
            *member.cast_to<webrtc::RTCStatsMember<int32_t>>());
      case webrtc::RTCStatsMemberInterface::Type::kString:
        return std::make_unique<base::Value>(
            *member.cast_to<webrtc::RTCStatsMember<std::string>>());
      // Types not supported by base::Value are converted to string.
      case webrtc::RTCStatsMemberInterface::Type::kUint32:
      case webrtc::RTCStatsMemberInterface::Type::kInt64:
      case webrtc::RTCStatsMemberInterface::Type::kUint64:
      case webrtc::RTCStatsMemberInterface::Type::kDouble:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceBool:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceInt32:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceUint32:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceInt64:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceUint64:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceDouble:
      case webrtc::RTCStatsMemberInterface::Type::kSequenceString:
      default:
        return std::make_unique<base::Value>(member.ValueToString());
    }
  }

  const int lid_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
};

struct PeerConnectionTrackerLazyInstanceTraits
    : public base::internal::DestructorAtExitLazyInstanceTraits<
          PeerConnectionTracker> {
  static PeerConnectionTracker* New(void* instance) {
    return new (instance) PeerConnectionTracker(
        RenderThreadImpl::current()->main_thread_runner());
  }
};

base::LazyInstance<PeerConnectionTracker,
                   PeerConnectionTrackerLazyInstanceTraits>
    g_peer_connection_tracker = LAZY_INSTANCE_INITIALIZER;

// static
PeerConnectionTracker* PeerConnectionTracker::GetInstance() {
  return &g_peer_connection_tracker.Get();
}

PeerConnectionTracker::PeerConnectionTracker(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : next_local_id_(1),
      send_target_for_test_(nullptr),
      main_thread_task_runner_(std::move(main_thread_task_runner)) {}

PeerConnectionTracker::PeerConnectionTracker(
    mojom::PeerConnectionTrackerHostAssociatedPtr host,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : next_local_id_(1),
      send_target_for_test_(nullptr),
      peer_connection_tracker_host_ptr_(std::move(host)),
      main_thread_task_runner_(std::move(main_thread_task_runner)) {}

PeerConnectionTracker::~PeerConnectionTracker() {
}

bool PeerConnectionTracker::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PeerConnectionTracker, message)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_GetStandardStats,
                        OnGetStandardStats)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_GetLegacyStats, OnGetLegacyStats)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_OnSuspend, OnSuspend)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_StartEventLog, OnStartEventLog)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_StopEventLog, OnStopEventLog)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PeerConnectionTracker::OnGetStandardStats() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  const std::string empty_track_id;
  for (const auto& pair : peer_connection_local_id_map_) {
    scoped_refptr<InternalStandardStatsObserver> observer(
        new rtc::RefCountedObject<InternalStandardStatsObserver>(
            pair.second, main_thread_task_runner_));
    pair.first->GetStandardStatsForTracker(observer);
  }
}

void PeerConnectionTracker::OnGetLegacyStats() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  const std::string empty_track_id;
  for (const auto& pair : peer_connection_local_id_map_) {
    rtc::scoped_refptr<InternalLegacyStatsObserver> observer(
        new rtc::RefCountedObject<InternalLegacyStatsObserver>(
            pair.second, main_thread_task_runner_));

    pair.first->GetStats(
        observer, webrtc::PeerConnectionInterface::kStatsOutputLevelDebug,
        nullptr);
  }
}

RenderThread* PeerConnectionTracker::SendTarget() {
  if (send_target_for_test_) {
    return send_target_for_test_;
  }
  return RenderThreadImpl::current();
}

void PeerConnectionTracker::OnSuspend() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  for (auto it = peer_connection_local_id_map_.begin();
       it != peer_connection_local_id_map_.end(); ++it) {
    it->first->CloseClientPeerConnection();
  }
}

void PeerConnectionTracker::OnStartEventLog(int peer_connection_local_id,
                                            int output_period_ms) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  for (auto& it : peer_connection_local_id_map_) {
    if (it.second == peer_connection_local_id) {
      it.first->StartEventLog(output_period_ms);
      return;
    }
  }
}

void PeerConnectionTracker::OnStopEventLog(int peer_connection_local_id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  for (auto& it : peer_connection_local_id_map_) {
    if (it.second == peer_connection_local_id) {
      it.first->StopEventLog();
      return;
    }
  }
}

void PeerConnectionTracker::RegisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    const blink::WebMediaConstraints& constraints,
    const blink::WebLocalFrame* frame) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  DCHECK(pc_handler);
  DCHECK_EQ(GetLocalIDForHandler(pc_handler), -1);
  DVLOG(1) << "PeerConnectionTracker::RegisterPeerConnection()";
  auto info = mojom::PeerConnectionInfo::New();

  info->lid = GetNextLocalID();
  info->rtc_configuration = SerializeConfiguration(config);

  info->constraints = SerializeMediaConstraints(constraints);
  if (frame)
    info->url = frame->GetDocument().Url().GetString().Utf8();
  else
    info->url = "test:testing";

  int32_t lid = info->lid;
  GetPeerConnectionTrackerHost()->AddPeerConnection(std::move(info));

  peer_connection_local_id_map_.insert(std::make_pair(pc_handler, lid));
}

void PeerConnectionTracker::UnregisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  DVLOG(1) << "PeerConnectionTracker::UnregisterPeerConnection()";

  auto it = peer_connection_local_id_map_.find(pc_handler);

  if (it == peer_connection_local_id_map_.end()) {
    // The PeerConnection might not have been registered if its initilization
    // failed.
    return;
  }

  GetPeerConnectionTrackerHost()->RemovePeerConnection(it->second);

  peer_connection_local_id_map_.erase(it);
}

void PeerConnectionTracker::TrackCreateOffer(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebRTCOfferOptions& options) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "createOffer",
                           "options: {" + SerializeOfferOptions(options) + "}");
}

void PeerConnectionTracker::TrackCreateOffer(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebMediaConstraints& constraints) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "createOffer",
      "constraints: {" + SerializeMediaConstraints(constraints) + "}");
}

void PeerConnectionTracker::TrackCreateAnswer(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebRTCAnswerOptions& options) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "createAnswer", "options: {" + SerializeAnswerOptions(options) + "}");
}

void PeerConnectionTracker::TrackCreateAnswer(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebMediaConstraints& constraints) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "createAnswer",
      "constraints: {" + SerializeMediaConstraints(constraints) + "}");
}

void PeerConnectionTracker::TrackSetSessionDescription(
    RTCPeerConnectionHandler* pc_handler,
    const std::string& sdp, const std::string& type, Source source) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  std::string value = "type: " + type + ", sdp: " + sdp;
  SendPeerConnectionUpdate(
      id,
      source == SOURCE_LOCAL ? "setLocalDescription" : "setRemoteDescription",
      value);
}

void PeerConnectionTracker::TrackSetConfiguration(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::PeerConnectionInterface::RTCConfiguration& config) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;

  SendPeerConnectionUpdate(id, "setConfiguration",
                           SerializeConfiguration(config));
}

void PeerConnectionTracker::TrackAddIceCandidate(
    RTCPeerConnectionHandler* pc_handler,
    scoped_refptr<blink::WebRTCICECandidate> candidate,
    Source source,
    bool succeeded) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  std::string value = "sdpMid: " + candidate->SdpMid().Utf8() + ", " +
                      "sdpMLineIndex: " +
                      (candidate->SdpMLineIndex()
                           ? base::NumberToString(*candidate->SdpMLineIndex())
                           : "null") +
                      ", " + "candidate: " + candidate->Candidate().Utf8();

  // OnIceCandidate always succeeds as it's a callback from the browser.
  DCHECK(source != SOURCE_LOCAL || succeeded);

  const char* event =
      (source == SOURCE_LOCAL) ? "onIceCandidate"
                               : (succeeded ? "addIceCandidate"
                                            : "addIceCandidateFailed");

  SendPeerConnectionUpdate(id, event, value);
}

void PeerConnectionTracker::TrackAddTransceiver(
    RTCPeerConnectionHandler* pc_handler,
    PeerConnectionTracker::TransceiverUpdatedReason reason,
    const blink::WebRTCRtpTransceiver& transceiver,
    size_t transceiver_index) {
  TrackTransceiver("Added", pc_handler, reason, transceiver, transceiver_index);
}

void PeerConnectionTracker::TrackModifyTransceiver(
    RTCPeerConnectionHandler* pc_handler,
    PeerConnectionTracker::TransceiverUpdatedReason reason,
    const blink::WebRTCRtpTransceiver& transceiver,
    size_t transceiver_index) {
  TrackTransceiver("Modified", pc_handler, reason, transceiver,
                   transceiver_index);
}

void PeerConnectionTracker::TrackRemoveTransceiver(
    RTCPeerConnectionHandler* pc_handler,
    PeerConnectionTracker::TransceiverUpdatedReason reason,
    const blink::WebRTCRtpTransceiver& transceiver,
    size_t transceiver_index) {
  TrackTransceiver("Removed", pc_handler, reason, transceiver,
                   transceiver_index);
}

void PeerConnectionTracker::TrackTransceiver(
    const char* callback_type_ending,
    RTCPeerConnectionHandler* pc_handler,
    PeerConnectionTracker::TransceiverUpdatedReason reason,
    const blink::WebRTCRtpTransceiver& transceiver,
    size_t transceiver_index) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  std::string callback_type;
  if (transceiver.ImplementationType() ==
      blink::WebRTCRtpTransceiverImplementationType::kFullTransceiver) {
    callback_type = "transceiver";
  } else if (transceiver.ImplementationType() ==
             blink::WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly) {
    callback_type = "sender";
  } else {
    callback_type = "receiver";
  }
  callback_type += callback_type_ending;

  std::string result;
  result += "Caused by: ";
  result += GetTransceiverUpdatedReasonString(reason);
  result += "\n\n";
  if (transceiver.ImplementationType() ==
      blink::WebRTCRtpTransceiverImplementationType::kFullTransceiver) {
    result += "getTransceivers()";
  } else if (transceiver.ImplementationType() ==
             blink::WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly) {
    result += "getSenders()";
  } else {
    DCHECK_EQ(
        transceiver.ImplementationType(),
        blink::WebRTCRtpTransceiverImplementationType::kPlanBReceiverOnly);
    result += "getReceivers()";
  }
  result += "[" + base::NumberToString(transceiver_index) + "]:";
  result += SerializeTransceiver(transceiver);
  SendPeerConnectionUpdate(id, callback_type, result);
}

void PeerConnectionTracker::TrackCreateDataChannel(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::DataChannelInterface* data_channel,
    Source source) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  std::string value = "label: " + data_channel->label() + ", reliable: " +
                      SerializeBoolean(data_channel->reliable());
  SendPeerConnectionUpdate(
      id,
      source == SOURCE_LOCAL ? "createLocalDataChannel" : "onRemoteDataChannel",
      value);
}

void PeerConnectionTracker::TrackStop(RTCPeerConnectionHandler* pc_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "stop", std::string());
}

void PeerConnectionTracker::TrackSignalingStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::SignalingState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "signalingStateChange", GetSignalingStateString(state));
}

void PeerConnectionTracker::TrackLegacyIceConnectionStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::IceConnectionState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "legacyIceConnectionStateChange",
                           GetIceConnectionStateString(state));
}

void PeerConnectionTracker::TrackIceConnectionStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::IceConnectionState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "iceConnectionStateChange",
      GetIceConnectionStateString(state));
}

void PeerConnectionTracker::TrackConnectionStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::PeerConnectionState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "connectionStateChange",
                           GetConnectionStateString(state));
}

void PeerConnectionTracker::TrackIceGatheringStateChange(
    RTCPeerConnectionHandler* pc_handler,
    webrtc::PeerConnectionInterface::IceGatheringState state) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(
      id, "iceGatheringStateChange",
      GetIceGatheringStateString(state));
}

void PeerConnectionTracker::TrackSessionDescriptionCallback(
    RTCPeerConnectionHandler* pc_handler,
    Action action,
    const std::string& callback_type,
    const std::string& value) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  std::string update_type;
  switch (action) {
    case ACTION_SET_LOCAL_DESCRIPTION:
      update_type = "setLocalDescription";
      break;
    case ACTION_SET_REMOTE_DESCRIPTION:
      update_type = "setRemoteDescription";
      break;
    case ACTION_CREATE_OFFER:
      update_type = "createOffer";
      break;
    case ACTION_CREATE_ANSWER:
      update_type = "createAnswer";
      break;
    default:
      NOTREACHED();
      break;
  }
  update_type += callback_type;

  SendPeerConnectionUpdate(id, update_type.c_str(), value);
}

void PeerConnectionTracker::TrackSessionId(RTCPeerConnectionHandler* pc_handler,
                                           const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  DCHECK(pc_handler);
  DCHECK(!session_id.empty());
  const int local_id = GetLocalIDForHandler(pc_handler);
  if (local_id == -1) {
    return;
  }
  GetPeerConnectionTrackerHost().get()->OnPeerConnectionSessionIdSet(
      local_id, session_id);
}

void PeerConnectionTracker::TrackOnRenegotiationNeeded(
    RTCPeerConnectionHandler* pc_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  SendPeerConnectionUpdate(id, "onRenegotiationNeeded", std::string());
}

void PeerConnectionTracker::TrackGetUserMedia(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);

  GetPeerConnectionTrackerHost()->GetUserMedia(
      user_media_request.GetSecurityOrigin().ToString().Utf8(),
      user_media_request.Audio(), user_media_request.Video(),
      SerializeMediaConstraints(user_media_request.AudioConstraints()),
      SerializeMediaConstraints(user_media_request.VideoConstraints()));
}

void PeerConnectionTracker::TrackRtcEventLogWrite(
    RTCPeerConnectionHandler* pc_handler,
    const std::string& output) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  int id = GetLocalIDForHandler(pc_handler);
  if (id == -1)
    return;
  GetPeerConnectionTrackerHost()->WebRtcEventLogWrite(id, output);
}

int PeerConnectionTracker::GetNextLocalID() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  if (next_local_id_< 0)
    next_local_id_ = 1;
  return next_local_id_++;
}

int PeerConnectionTracker::GetLocalIDForHandler(
    RTCPeerConnectionHandler* handler) const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  const auto found = peer_connection_local_id_map_.find(handler);
  if (found == peer_connection_local_id_map_.end())
    return -1;
  DCHECK_NE(found->second, -1);
  return found->second;
}

void PeerConnectionTracker::SendPeerConnectionUpdate(
    int local_id,
    const std::string& callback_type,
    const std::string& value) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_);
  GetPeerConnectionTrackerHost().get()->UpdatePeerConnection(
      local_id, callback_type, value);
}

void PeerConnectionTracker::OverrideSendTargetForTesting(RenderThread* target) {
  send_target_for_test_ = target;
}

const mojom::PeerConnectionTrackerHostAssociatedPtr&
PeerConnectionTracker::GetPeerConnectionTrackerHost() {
  if (!peer_connection_tracker_host_ptr_) {
    RenderThreadImpl::current()->channel()->GetRemoteAssociatedInterface(
        &peer_connection_tracker_host_ptr_);
  }
  return peer_connection_tracker_host_ptr_;
}

}  // namespace content
