// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"

namespace blink {

MockMojoMediaStreamDispatcherHost::MockMojoMediaStreamDispatcherHost()
    : binding_(this) {}

MockMojoMediaStreamDispatcherHost::~MockMojoMediaStreamDispatcherHost() {}

mojom::blink::MediaStreamDispatcherHostPtr
MockMojoMediaStreamDispatcherHost::CreateInterfacePtrAndBind() {
  mojom::blink::MediaStreamDispatcherHostPtr dispatcher_host;
  binding_.Bind(mojo::MakeRequest(&dispatcher_host));
  return dispatcher_host;
}

void MockMojoMediaStreamDispatcherHost::GenerateStream(
    int32_t request_id,
    mojom::blink::StreamControlsPtr controls,
    bool user_gesture,
    GenerateStreamCallback callback) {
  request_id_ = request_id;
  audio_devices_.clear();
  video_devices_.clear();
  ++request_stream_counter_;

  if (controls->audio->requested) {
    mojom::blink::MediaStreamDevicePtr audio_device =
        mojom::blink::MediaStreamDevice::New();
    audio_device->id =
        controls->audio->device_id + session_id_.ToString().c_str();
    audio_device->name = "microphone";
    audio_device->type = controls->audio->stream_type;
    audio_device->session_id = session_id_;
    audio_device->matched_output_device_id =
        WTF::String("associated_output_device_id") +
        session_id_.ToString().c_str();
    audio_devices_.push_back(std::move(audio_device));
  }

  if (controls->video->requested) {
    mojom::blink::MediaStreamDevicePtr video_device =
        mojom::blink::MediaStreamDevice::New();
    video_device->id =
        controls->video->device_id + session_id_.ToString().c_str();
    video_device->name = "usb video camera";
    video_device->type = controls->video->stream_type;
    video_device->video_facing = media::mojom::blink::VideoFacingMode::USER;
    video_device->session_id = session_id_;
    video_devices_.push_back(std::move(video_device));
  }

  if (do_not_run_cb_) {
    generate_stream_cb_ = std::move(callback);
  } else {
    std::move(callback).Run(
        mojom::blink::MediaStreamRequestResult::OK,
        WTF::String("dummy") + WTF::String::Number(request_id_),
        std::move(audio_devices_), std::move(video_devices_));
  }
}

void MockMojoMediaStreamDispatcherHost::CancelRequest(int32_t request_id) {
  EXPECT_EQ(request_id, request_id_);
}

void MockMojoMediaStreamDispatcherHost::StopStreamDevice(
    const WTF::String& device_id,
    const base::Optional<base::UnguessableToken>& session_id) {
  for (const mojom::blink::MediaStreamDevicePtr& device : audio_devices_) {
    if (device->id == device_id && device->session_id == session_id) {
      ++stop_audio_device_counter_;
      return;
    }
  }
  for (const mojom::blink::MediaStreamDevicePtr& device : video_devices_) {
    if (device->id == device_id && device->session_id == session_id) {
      ++stop_video_device_counter_;
      return;
    }
  }
  NOTREACHED();
}

void MockMojoMediaStreamDispatcherHost::OpenDevice(
    int32_t request_id,
    const WTF::String& device_id,
    mojom::blink::MediaStreamType type,
    OpenDeviceCallback callback) {
  mojom::blink::MediaStreamDevicePtr device =
      mojom::blink::MediaStreamDevice::New();
  device->id = device_id;
  device->type = type;
  device->session_id = session_id_;
  std::move(callback).Run(true /* success */,
                          "dummy" + WTF::String::Number(request_id),
                          std::move(device));
}

}  // namespace blink
