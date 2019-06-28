// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/renderer/media/webrtc/fake_rtc_rtp_transceiver.h"
#include "content/renderer/media/webrtc/webrtc_util.h"

namespace content {

blink::WebMediaStreamTrack CreateWebMediaStreamTrack(
    const std::string& id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  blink::WebMediaStreamSource web_source;
  web_source.Initialize(blink::WebString::FromUTF8(id),
                        blink::WebMediaStreamSource::kTypeAudio,
                        blink::WebString::FromUTF8("audio_track"), false);
  std::unique_ptr<blink::MediaStreamAudioSource> audio_source_ptr =
      std::make_unique<blink::MediaStreamAudioSource>(
          std::move(task_runner), true /* is_local_source */);
  blink::MediaStreamAudioSource* audio_source = audio_source_ptr.get();
  // Takes ownership of |audio_source_ptr|.
  web_source.SetPlatformSource(std::move(audio_source_ptr));

  blink::WebMediaStreamTrack web_track;
  web_track.Initialize(web_source.Id(), web_source);
  audio_source->ConnectToTrack(web_track);
  return web_track;
}

FakeRTCRtpSender::FakeRTCRtpSender(
    base::Optional<std::string> track_id,
    std::vector<std::string> stream_ids,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : track_id_(std::move(track_id)),
      stream_ids_(std::move(stream_ids)),
      task_runner_(task_runner) {}

FakeRTCRtpSender::FakeRTCRtpSender(const FakeRTCRtpSender&) = default;

FakeRTCRtpSender::~FakeRTCRtpSender() {}

FakeRTCRtpSender& FakeRTCRtpSender::operator=(const FakeRTCRtpSender&) =
    default;

std::unique_ptr<blink::WebRTCRtpSender> FakeRTCRtpSender::ShallowCopy() const {
  return std::make_unique<FakeRTCRtpSender>(*this);
}

uintptr_t FakeRTCRtpSender::Id() const {
  NOTIMPLEMENTED();
  return 0;
}

rtc::scoped_refptr<webrtc::DtlsTransportInterface>
FakeRTCRtpSender::DtlsTransport() {
  NOTIMPLEMENTED();
  return nullptr;
}

webrtc::DtlsTransportInformation FakeRTCRtpSender::DtlsTransportInformation() {
  NOTIMPLEMENTED();
  static webrtc::DtlsTransportInformation dummy(
      webrtc::DtlsTransportState::kNew);
  return dummy;
}

blink::WebMediaStreamTrack FakeRTCRtpSender::Track() const {
  return track_id_ ? CreateWebMediaStreamTrack(*track_id_, task_runner_)
                   : blink::WebMediaStreamTrack();  // null
}

blink::WebVector<blink::WebString> FakeRTCRtpSender::StreamIds() const {
  blink::WebVector<blink::WebString> web_stream_ids(stream_ids_.size());
  for (size_t i = 0; i < stream_ids_.size(); ++i) {
    web_stream_ids[i] = blink::WebString::FromUTF8(stream_ids_[i]);
  }
  return web_stream_ids;
}

void FakeRTCRtpSender::ReplaceTrack(blink::WebMediaStreamTrack with_track,
                                    blink::WebRTCVoidRequest request) {
  NOTIMPLEMENTED();
}

std::unique_ptr<blink::WebRTCDTMFSenderHandler>
FakeRTCRtpSender::GetDtmfSender() const {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<webrtc::RtpParameters> FakeRTCRtpSender::GetParameters() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeRTCRtpSender::SetParameters(
    blink::WebVector<webrtc::RtpEncodingParameters>,
    webrtc::DegradationPreference,
    blink::WebRTCVoidRequest) {
  NOTIMPLEMENTED();
}

void FakeRTCRtpSender::GetStats(
    blink::WebRTCStatsReportCallback,
    const blink::WebVector<webrtc::NonStandardGroupId>&) {
  NOTIMPLEMENTED();
}

void FakeRTCRtpSender::SetStreams(
    const blink::WebVector<blink::WebString>& stream_ids) {
  NOTIMPLEMENTED();
}

FakeRTCRtpReceiver::FakeRTCRtpReceiver(
    const std::string& track_id,
    std::vector<std::string> stream_ids,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : track_(CreateWebMediaStreamTrack(track_id, task_runner)),
      stream_ids_(std::move(stream_ids)) {}

FakeRTCRtpReceiver::FakeRTCRtpReceiver(const FakeRTCRtpReceiver&) = default;

FakeRTCRtpReceiver::~FakeRTCRtpReceiver() {}

FakeRTCRtpReceiver& FakeRTCRtpReceiver::operator=(const FakeRTCRtpReceiver&) =
    default;

std::unique_ptr<blink::WebRTCRtpReceiver> FakeRTCRtpReceiver::ShallowCopy()
    const {
  return std::make_unique<FakeRTCRtpReceiver>(*this);
}

uintptr_t FakeRTCRtpReceiver::Id() const {
  NOTIMPLEMENTED();
  return 0;
}

rtc::scoped_refptr<webrtc::DtlsTransportInterface>
FakeRTCRtpReceiver::DtlsTransport() {
  NOTIMPLEMENTED();
  return nullptr;
}

webrtc::DtlsTransportInformation
FakeRTCRtpReceiver::DtlsTransportInformation() {
  NOTIMPLEMENTED();
  static webrtc::DtlsTransportInformation dummy(
      webrtc::DtlsTransportState::kNew);
  return dummy;
}

const blink::WebMediaStreamTrack& FakeRTCRtpReceiver::Track() const {
  return track_;
}

blink::WebVector<blink::WebString> FakeRTCRtpReceiver::StreamIds() const {
  blink::WebVector<blink::WebString> web_stream_ids(stream_ids_.size());
  for (size_t i = 0; i < stream_ids_.size(); ++i) {
    web_stream_ids[i] = blink::WebString::FromUTF8(stream_ids_[i]);
  }
  return web_stream_ids;
}

blink::WebVector<std::unique_ptr<blink::WebRTCRtpSource>>
FakeRTCRtpReceiver::GetSources() {
  NOTIMPLEMENTED();
  return {};
}

void FakeRTCRtpReceiver::GetStats(
    blink::WebRTCStatsReportCallback,
    const blink::WebVector<webrtc::NonStandardGroupId>&) {
  NOTIMPLEMENTED();
}

std::unique_ptr<webrtc::RtpParameters> FakeRTCRtpReceiver::GetParameters()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeRTCRtpReceiver::SetJitterBufferMinimumDelay(
    base::Optional<double> delay_seconds) {
  NOTIMPLEMENTED();
}

FakeRTCRtpTransceiver::FakeRTCRtpTransceiver(
    base::Optional<std::string> mid,
    FakeRTCRtpSender sender,
    FakeRTCRtpReceiver receiver,
    bool stopped,
    webrtc::RtpTransceiverDirection direction,
    base::Optional<webrtc::RtpTransceiverDirection> current_direction)
    : mid_(std::move(mid)),
      sender_(std::move(sender)),
      receiver_(std::move(receiver)),
      stopped_(stopped),
      direction_(std::move(direction)),
      current_direction_(std::move(current_direction)) {}

FakeRTCRtpTransceiver::~FakeRTCRtpTransceiver() {}

blink::WebRTCRtpTransceiverImplementationType
FakeRTCRtpTransceiver::ImplementationType() const {
  return blink::WebRTCRtpTransceiverImplementationType::kFullTransceiver;
}

uintptr_t FakeRTCRtpTransceiver::Id() const {
  NOTIMPLEMENTED();
  return 0u;
}

blink::WebString FakeRTCRtpTransceiver::Mid() const {
  return mid_ ? blink::WebString::FromUTF8(*mid_) : blink::WebString();
}

std::unique_ptr<blink::WebRTCRtpSender> FakeRTCRtpTransceiver::Sender() const {
  return sender_.ShallowCopy();
}

std::unique_ptr<blink::WebRTCRtpReceiver> FakeRTCRtpTransceiver::Receiver()
    const {
  return receiver_.ShallowCopy();
}

bool FakeRTCRtpTransceiver::Stopped() const {
  return stopped_;
}

webrtc::RtpTransceiverDirection FakeRTCRtpTransceiver::Direction() const {
  return direction_;
}

void FakeRTCRtpTransceiver::SetDirection(
    webrtc::RtpTransceiverDirection direction) {
  NOTIMPLEMENTED();
}

base::Optional<webrtc::RtpTransceiverDirection>
FakeRTCRtpTransceiver::CurrentDirection() const {
  return current_direction_;
}

base::Optional<webrtc::RtpTransceiverDirection>
FakeRTCRtpTransceiver::FiredDirection() const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

}  // namespace content
