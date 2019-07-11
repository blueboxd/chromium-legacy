// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_TRACK_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_TRACK_ADAPTER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class MediaStreamVideoTrack;
}

namespace content {

class PeerConnectionDependencyFactory;
class WebRtcVideoTrackSource;

// MediaStreamVideoWebRtcSink is an adapter between a
// blink::MediaStreamVideoTrack object and a webrtc VideoTrack that is
// currently sent on a PeerConnection.
// The responsibility of the class is to create and own a representation of a
// webrtc VideoTrack that can be added and removed from a RTCPeerConnection. An
// instance of MediaStreamVideoWebRtcSink is created when a VideoTrack is added
// to an RTCPeerConnection object.
// Instances of this class is owned by the WebRtcMediaStreamAdapter object that
// created it.
class CONTENT_EXPORT MediaStreamVideoWebRtcSink
    : public blink::MediaStreamVideoSink {
 public:
  MediaStreamVideoWebRtcSink(
      const blink::WebMediaStreamTrack& track,
      PeerConnectionDependencyFactory* factory,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~MediaStreamVideoWebRtcSink() override;

  webrtc::VideoTrackInterface* webrtc_video_track() {
    return video_track_.get();
  }

  absl::optional<bool> SourceNeedsDenoisingForTesting() const;

 protected:
  // Implementation of MediaStreamSink.
  void OnEnabledChanged(bool enabled) override;
  void OnContentHintChanged(
      blink::WebMediaStreamTrack::ContentHintType content_hint) override;

 private:
  // Helper to request a refresh frame from the source. Called via the callback
  // passed to WebRtcVideoSourceAdapter.
  void RequestRefreshFrame();

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // |video_source_| and |video_source_proxy_| are held as
  // references to outlive |video_track_| since the interfaces between them
  // don't use reference counting.
  scoped_refptr<WebRtcVideoTrackSource> video_source_;
  scoped_refptr<webrtc::VideoTrackSourceInterface> video_source_proxy_;
  scoped_refptr<webrtc::VideoTrackInterface> video_track_;

  class WebRtcVideoSourceAdapter;
  scoped_refptr<WebRtcVideoSourceAdapter> source_adapter_;

  // Provides WebRtcVideoSourceAdapter a weak reference to
  // MediaStreamVideoWebRtcSink in order to allow it to request refresh frames.
  // See comments in media_stream_video_webrtc_sink.cc.
  base::WeakPtrFactory<MediaStreamVideoWebRtcSink> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoWebRtcSink);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_TRACK_ADAPTER_H_
