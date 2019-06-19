// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/remote_media_stream_track_adapter.h"

#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"
#include "media/base/limits.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_source.h"
#include "third_party/blink/public/platform/modules/webrtc/peer_connection_remote_audio_source.h"
#include "third_party/blink/public/platform/modules/webrtc/track_observer.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"

namespace content {

RemoteVideoTrackAdapter::RemoteVideoTrackAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::VideoTrackInterface* webrtc_track)
    : RemoteMediaStreamTrackAdapter(main_thread, webrtc_track) {
  std::unique_ptr<blink::TrackObserver> observer(
      new blink::TrackObserver(main_thread, observed_track().get()));
  // Here, we use base::Unretained() to avoid a circular reference.
  web_initialize_ = base::Bind(
      &RemoteVideoTrackAdapter::InitializeWebVideoTrack, base::Unretained(this),
      base::Passed(&observer), observed_track()->enabled());
}

RemoteVideoTrackAdapter::~RemoteVideoTrackAdapter() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (initialized()) {
    static_cast<MediaStreamRemoteVideoSource*>(
        web_track()->Source().GetPlatformSource())
        ->OnSourceTerminated();
  }
}

void RemoteVideoTrackAdapter::InitializeWebVideoTrack(
    std::unique_ptr<blink::TrackObserver> observer,
    bool enabled) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  std::unique_ptr<MediaStreamRemoteVideoSource> video_source_ptr =
      std::make_unique<MediaStreamRemoteVideoSource>(std::move(observer));
  MediaStreamRemoteVideoSource* video_source = video_source_ptr.get();
  InitializeWebTrack(blink::WebMediaStreamSource::kTypeVideo);
  web_track()->Source().SetPlatformSource(std::move(video_source_ptr));

  blink::WebMediaStreamSource::Capabilities capabilities;
  capabilities.device_id = blink::WebString::FromUTF8(id());
  web_track()->Source().SetCapabilities(capabilities);

  web_track()->SetPlatformTrack(std::make_unique<blink::MediaStreamVideoTrack>(
      video_source, blink::MediaStreamVideoSource::ConstraintsCallback(),
      enabled));
}

RemoteAudioTrackAdapter::RemoteAudioTrackAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::AudioTrackInterface* webrtc_track)
    : RemoteMediaStreamTrackAdapter(main_thread, webrtc_track),
#if DCHECK_IS_ON()
      unregistered_(false),
#endif
      state_(observed_track()->state()) {
  // TODO(tommi): Use blink::TrackObserver instead.
  observed_track()->RegisterObserver(this);
  // Here, we use base::Unretained() to avoid a circular reference.
  web_initialize_ =
      base::Bind(&RemoteAudioTrackAdapter::InitializeWebAudioTrack,
                 base::Unretained(this), main_thread);
}

RemoteAudioTrackAdapter::~RemoteAudioTrackAdapter() {
#if DCHECK_IS_ON()
  DCHECK(unregistered_);
#endif
}

void RemoteAudioTrackAdapter::Unregister() {
#if DCHECK_IS_ON()
  DCHECK(!unregistered_);
  unregistered_ = true;
#endif
  observed_track()->UnregisterObserver(this);
}

void RemoteAudioTrackAdapter::InitializeWebAudioTrack(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread) {
  InitializeWebTrack(blink::WebMediaStreamSource::kTypeAudio);

  blink::MediaStreamAudioSource* const source =
      new blink::PeerConnectionRemoteAudioSource(observed_track().get(),
                                                 main_thread);
  web_track()->Source().SetPlatformSource(
      base::WrapUnique(source));  // Takes ownership.

  blink::WebMediaStreamSource::Capabilities capabilities;
  capabilities.device_id = blink::WebString::FromUTF8(id());
  capabilities.echo_cancellation = std::vector<bool>({false});
  capabilities.auto_gain_control = std::vector<bool>({false});
  capabilities.noise_suppression = std::vector<bool>({false});
  capabilities.sample_size = {
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16),  // min
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16)   // max
  };
  web_track()->Source().SetCapabilities(capabilities);

  source->ConnectToTrack(*(web_track()));
}

void RemoteAudioTrackAdapter::OnChanged() {
  main_thread_->PostTask(
      FROM_HERE, base::BindOnce(&RemoteAudioTrackAdapter::OnChangedOnMainThread,
                                this, observed_track()->state()));
}

void RemoteAudioTrackAdapter::OnChangedOnMainThread(
    webrtc::MediaStreamTrackInterface::TrackState state) {
  DCHECK(main_thread_->BelongsToCurrentThread());

  if (state == state_ || !initialized())
    return;

  state_ = state;

  switch (state) {
    case webrtc::MediaStreamTrackInterface::kLive:
      web_track()->Source().SetReadyState(
          blink::WebMediaStreamSource::kReadyStateLive);
      break;
    case webrtc::MediaStreamTrackInterface::kEnded:
      web_track()->Source().SetReadyState(
          blink::WebMediaStreamSource::kReadyStateEnded);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace content
