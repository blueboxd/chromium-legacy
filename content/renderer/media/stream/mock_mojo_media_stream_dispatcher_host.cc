// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/mock_mojo_media_stream_dispatcher_host.h"

#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

namespace content {

MockMojoMediaStreamDispatcherHost::MockMojoMediaStreamDispatcherHost()
    : binding_(this) {}

MockMojoMediaStreamDispatcherHost::~MockMojoMediaStreamDispatcherHost() {}

blink::mojom::MediaStreamDispatcherHostPtr
MockMojoMediaStreamDispatcherHost::CreateInterfacePtrAndBind() {
  blink::mojom::MediaStreamDispatcherHostPtr dispatcher_host;
  binding_.Bind(mojo::MakeRequest(&dispatcher_host));
  return dispatcher_host;
}

void MockMojoMediaStreamDispatcherHost::GenerateStream(
    int32_t request_id,
    const blink::StreamControls& controls,
    bool user_gesture,
    GenerateStreamCallback callback) {
  request_id_ = request_id;
  audio_devices_.clear();
  video_devices_.clear();
  ++request_stream_counter_;

  if (controls.audio.requested) {
    blink::MediaStreamDevice audio_device;
    audio_device.id = controls.audio.device_id + session_id_.ToString();
    audio_device.name = "microphone";
    audio_device.type = controls.audio.stream_type;
    audio_device.set_session_id(session_id_);
    audio_device.matched_output_device_id =
        "associated_output_device_id" + session_id_.ToString();
    audio_devices_.push_back(audio_device);
  }

  if (controls.video.requested) {
    blink::MediaStreamDevice video_device;
    video_device.id = controls.video.device_id + session_id_.ToString();
    video_device.name = "usb video camera";
    video_device.type = controls.video.stream_type;
    video_device.video_facing = media::MEDIA_VIDEO_FACING_USER;
    video_device.set_session_id(session_id_);
    video_devices_.push_back(video_device);
  }

  if (do_not_run_cb_) {
    generate_stream_cb_ = std::move(callback);
  } else {
    std::move(callback).Run(blink::mojom::MediaStreamRequestResult::OK,
                            "dummy" + base::NumberToString(request_id_),
                            audio_devices_, video_devices_);
  }
}

void MockMojoMediaStreamDispatcherHost::CancelRequest(int32_t request_id) {
  EXPECT_EQ(request_id, request_id_);
}

void MockMojoMediaStreamDispatcherHost::StopStreamDevice(
    const std::string& device_id,
    const base::Optional<base::UnguessableToken>& session_id) {
  base::UnguessableToken actual_session_id =
      session_id.value_or(base::UnguessableToken());
  for (const blink::MediaStreamDevice& device : audio_devices_) {
    if (device.id == device_id && device.session_id() == actual_session_id) {
      ++stop_audio_device_counter_;
      return;
    }
  }
  for (const blink::MediaStreamDevice& device : video_devices_) {
    if (device.id == device_id && device.session_id() == actual_session_id) {
      ++stop_video_device_counter_;
      return;
    }
  }
  NOTREACHED();
}

void MockMojoMediaStreamDispatcherHost::OpenDevice(
    int32_t request_id,
    const std::string& device_id,
    blink::mojom::MediaStreamType type,
    OpenDeviceCallback callback) {
  blink::MediaStreamDevice device;
  device.id = device_id;
  device.type = type;
  device.set_session_id(session_id_);
  std::move(callback).Run(true /* success */,
                          "dummy" + base::NumberToString(request_id), device);
}

}  // namespace content
