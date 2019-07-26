// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_device_observer.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_dispatcher_eventhandler.h"
#include "url/origin.h"

namespace content {

namespace {

bool RemoveStreamDeviceFromArray(const blink::MediaStreamDevice& device,
                                 blink::MediaStreamDevices* devices) {
  for (auto device_it = devices->begin(); device_it != devices->end();
       ++device_it) {
    if (device_it->IsSameDevice(device)) {
      devices->erase(device_it);
      return true;
    }
  }
  return false;
}

}  // namespace

struct MediaStreamDeviceObserver::Stream {
  Stream() {}
  ~Stream() {}
  base::WeakPtr<blink::MediaStreamDispatcherEventHandler> handler;
  blink::MediaStreamDevices audio_devices;
  blink::MediaStreamDevices video_devices;
};

// static
const base::UnguessableToken& MediaStreamDeviceObserver::NoSessionId() {
  static const base::NoDestructor<base::UnguessableToken> fake_session_id(
      base::UnguessableToken::Deserialize(0xFFFFFFFFFFFFFFFFU,
                                          0xFFFFFFFFFFFFFFFFU));
  return *fake_session_id;
}

MediaStreamDeviceObserver::MediaStreamDeviceObserver(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame), binding_(this) {
  registry_.AddInterface(base::BindRepeating(
      &MediaStreamDeviceObserver::BindMediaStreamDeviceObserverRequest,
      base::Unretained(this)));
}

MediaStreamDeviceObserver::~MediaStreamDeviceObserver() {}

blink::MediaStreamDevices
MediaStreamDeviceObserver::GetNonScreenCaptureDevices() {
  blink::MediaStreamDevices video_devices;
  for (const auto& stream_it : label_stream_map_) {
    for (const auto& video_device : stream_it.second.video_devices) {
      if (!blink::IsScreenCaptureMediaType(video_device.type))
        video_devices.push_back(video_device);
    }
  }
  return video_devices;
}

void MediaStreamDeviceObserver::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void MediaStreamDeviceObserver::OnDestruct() {
  // Do not self-destruct. UserMediaClientImpl owns |this|.
}

void MediaStreamDeviceObserver::OnDeviceStopped(
    const std::string& label,
    const blink::MediaStreamDevice& device) {
  DVLOG(1) << __func__ << " label=" << label << " device_id=" << device.id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end()) {
    // This can happen if a user stops a device from JS at the same
    // time as the underlying media device is unplugged from the system.
    return;
  }
  Stream* stream = &it->second;
  if (blink::IsAudioInputMediaType(device.type))
    RemoveStreamDeviceFromArray(device, &stream->audio_devices);
  else
    RemoveStreamDeviceFromArray(device, &stream->video_devices);

  if (stream->handler.get())
    stream->handler->OnDeviceStopped(device);

  // |it| could have already been invalidated in the function call above. So we
  // need to check if |label| is still in |label_stream_map_| again.
  // Note: this is a quick fix to the crash caused by erasing the invalidated
  // iterator from |label_stream_map_| (https://crbug.com/616884). Future work
  // needs to be done to resolve this re-entrancy issue.
  it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;
  stream = &it->second;
  if (stream->audio_devices.empty() && stream->video_devices.empty())
    label_stream_map_.erase(it);
}

void MediaStreamDeviceObserver::OnDeviceChanged(
    const std::string& label,
    const blink::MediaStreamDevice& old_device,
    const blink::MediaStreamDevice& new_device) {
  DVLOG(1) << __func__ << " old_device_id=" << old_device.id
           << " new_device_id=" << new_device.id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end()) {
    // This can happen if a user stops a device from JS at the same
    // time as the underlying media device is unplugged from the system.
    return;
  }

  Stream* stream = &it->second;
  if (stream->handler.get())
    stream->handler->OnDeviceChanged(old_device, new_device);

  // Update device list only for device changing. Removing device will be
  // handled in its own callback.
  if (old_device.type != blink::mojom::MediaStreamType::NO_SERVICE &&
      new_device.type != blink::mojom::MediaStreamType::NO_SERVICE) {
    if (RemoveStreamDeviceFromArray(old_device, &stream->audio_devices) ||
        RemoveStreamDeviceFromArray(old_device, &stream->video_devices)) {
      if (blink::IsAudioInputMediaType(new_device.type))
        stream->audio_devices.push_back(new_device);
      else
        stream->video_devices.push_back(new_device);
    }
  }
}

void MediaStreamDeviceObserver::BindMediaStreamDeviceObserverRequest(
    blink::mojom::MediaStreamDeviceObserverRequest request) {
  binding_.Bind(std::move(request));
}

void MediaStreamDeviceObserver::AddStream(
    const std::string& label,
    const blink::MediaStreamDevices& audio_devices,
    const blink::MediaStreamDevices& video_devices,
    const base::WeakPtr<blink::MediaStreamDispatcherEventHandler>&
        event_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Stream stream;
  stream.handler = event_handler;
  stream.audio_devices = audio_devices;
  stream.video_devices = video_devices;

  label_stream_map_[label] = stream;
}

void MediaStreamDeviceObserver::AddStream(
    const std::string& label,
    const blink::MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Stream stream;
  if (blink::IsAudioInputMediaType(device.type))
    stream.audio_devices.push_back(device);
  else if (blink::IsVideoInputMediaType(device.type))
    stream.video_devices.push_back(device);
  else
    NOTREACHED();

  label_stream_map_[label] = stream;
}

bool MediaStreamDeviceObserver::RemoveStream(const std::string& label) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return false;

  label_stream_map_.erase(it);
  return true;
}

void MediaStreamDeviceObserver::RemoveStreamDevice(
    const blink::MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Remove |device| from all streams in |label_stream_map_|.
  bool device_found = false;
  auto stream_it = label_stream_map_.begin();
  while (stream_it != label_stream_map_.end()) {
    blink::MediaStreamDevices& audio_devices = stream_it->second.audio_devices;
    blink::MediaStreamDevices& video_devices = stream_it->second.video_devices;

    if (RemoveStreamDeviceFromArray(device, &audio_devices) ||
        RemoveStreamDeviceFromArray(device, &video_devices)) {
      device_found = true;
      if (audio_devices.empty() && video_devices.empty()) {
        label_stream_map_.erase(stream_it++);
        continue;
      }
    }
    ++stream_it;
  }
  DCHECK(device_found);
}

base::UnguessableToken MediaStreamDeviceObserver::audio_session_id(
    const std::string& label) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() || it->second.audio_devices.empty())
    return NoSessionId();

  return it->second.audio_devices[0].session_id();
}

base::UnguessableToken MediaStreamDeviceObserver::video_session_id(
    const std::string& label) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() || it->second.video_devices.empty())
    return NoSessionId();

  return it->second.video_devices[0].session_id();
}

}  // namespace content
