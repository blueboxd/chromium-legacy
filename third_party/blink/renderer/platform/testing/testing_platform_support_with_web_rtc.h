// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TESTING_PLATFORM_SUPPORT_WITH_WEB_RTC_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TESTING_PLATFORM_SUPPORT_WITH_WEB_RTC_H_

#include <memory>
#include <string>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"

namespace blink {

// TODO(https://crbug.com/908461): This is currently implemented as NO-OPs or to
// create dummy objects whose methods return default values. Consider renaming
// the class, changing it to be GMOCK friendly or deleting it.
class MockWebRTCPeerConnectionHandler : public WebRTCPeerConnectionHandler {
 public:
  MockWebRTCPeerConnectionHandler();
  ~MockWebRTCPeerConnectionHandler() override;

  bool Initialize(const webrtc::PeerConnectionInterface::RTCConfiguration&,
                  const WebMediaConstraints&) override;

  WebVector<std::unique_ptr<WebRTCRtpTransceiver>> CreateOffer(
      const WebRTCSessionDescriptionRequest&,
      const WebMediaConstraints&) override;
  WebVector<std::unique_ptr<WebRTCRtpTransceiver>> CreateOffer(
      const WebRTCSessionDescriptionRequest&,
      const WebRTCOfferOptions&) override;
  void CreateAnswer(const WebRTCSessionDescriptionRequest&,
                    const WebMediaConstraints&) override;
  void CreateAnswer(const WebRTCSessionDescriptionRequest&,
                    const WebRTCAnswerOptions&) override;
  void SetLocalDescription(const WebRTCVoidRequest&,
                           const WebRTCSessionDescription&) override;
  void SetRemoteDescription(const WebRTCVoidRequest&,
                            const WebRTCSessionDescription&) override;
  WebRTCSessionDescription LocalDescription() override;
  WebRTCSessionDescription RemoteDescription() override;
  WebRTCSessionDescription CurrentLocalDescription() override;
  WebRTCSessionDescription CurrentRemoteDescription() override;
  WebRTCSessionDescription PendingLocalDescription() override;
  WebRTCSessionDescription PendingRemoteDescription() override;
  const webrtc::PeerConnectionInterface::RTCConfiguration& GetConfiguration()
      const override;
  webrtc::RTCErrorType SetConfiguration(
      const webrtc::PeerConnectionInterface::RTCConfiguration&) override;
  void RestartIce() override;
  void GetStats(const WebRTCStatsRequest&) override;
  void GetStats(WebRTCStatsReportCallback,
                const WebVector<webrtc::NonStandardGroupId>&) override;
  webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>>
  AddTransceiverWithTrack(const WebMediaStreamTrack&,
                          const webrtc::RtpTransceiverInit&) override;
  webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>>
  AddTransceiverWithKind(std::string kind,
                         const webrtc::RtpTransceiverInit&) override;
  webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>> AddTrack(
      const WebMediaStreamTrack&,
      const WebVector<WebMediaStream>&) override;
  webrtc::RTCErrorOr<std::unique_ptr<WebRTCRtpTransceiver>> RemoveTrack(
      WebRTCRtpSender*) override;
  scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
      const WebString& label,
      const WebRTCDataChannelInit&) override;
  void Stop() override;
  webrtc::PeerConnectionInterface* NativePeerConnection() override;
  void RunSynchronousOnceClosureOnSignalingThread(
      base::OnceClosure closure,
      const char* trace_event_name) override;
  void RunSynchronousRepeatingClosureOnSignalingThread(
      const base::RepeatingClosure& closure,
      const char* trace_event_name) override;
  void TrackIceConnectionStateChange(
      WebRTCPeerConnectionHandler::IceConnectionStateVersion version,
      webrtc::PeerConnectionInterface::IceConnectionState state) override;

 private:
  class DummyWebRTCRtpTransceiver;

  Vector<std::unique_ptr<DummyWebRTCRtpTransceiver>> transceivers_;
};

class TestingPlatformSupportWithWebRTC : public TestingPlatformSupport {
 public:
  std::unique_ptr<WebRTCPeerConnectionHandler> CreateRTCPeerConnectionHandler(
      WebRTCPeerConnectionHandlerClient*,
      scoped_refptr<base::SingleThreadTaskRunner>) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TESTING_PLATFORM_SUPPORT_WITH_WEB_RTC_H_
