// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/stream_buffer_manager.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/request_builder.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

StreamBufferManager::StreamBufferManager(
    CameraDeviceContext* device_context,
    std::unique_ptr<CameraBufferFactory> camera_buffer_factory)
    : device_context_(device_context),
      camera_buffer_factory_(std::move(camera_buffer_factory)),
      weak_ptr_factory_(this) {}

StreamBufferManager::~StreamBufferManager() {
  DestroyCurrentStreamsAndBuffers();
}

gfx::GpuMemoryBuffer* StreamBufferManager::GetBufferById(StreamType stream_type,
                                                         uint64_t buffer_id) {
  size_t buffer_index = GetBufferIndex(buffer_id);
  if (buffer_index >= stream_context_[stream_type]->buffers.size()) {
    LOG(ERROR) << "Invalid buffer index: " << buffer_index
               << " for stream: " << stream_type;
    return nullptr;
  }
  return stream_context_[stream_type]->buffers[buffer_index].get();
}

VideoCaptureFormat StreamBufferManager::GetStreamCaptureFormat(
    StreamType stream_type) {
  return stream_context_[stream_type]->capture_format;
}

void StreamBufferManager::DestroyCurrentStreamsAndBuffers() {
  for (const auto& iter : stream_context_) {
    if (iter.second) {
      for (const auto& buf : iter.second->buffers) {
        if (buf) {
          buf->Unmap();
        }
      }
      iter.second->buffers.clear();
    }
  }
  stream_context_.clear();
}

bool StreamBufferManager::HasFreeBuffers(
    const std::set<StreamType>& stream_types) {
  for (auto stream_type : stream_types) {
    if (IsInputStream(stream_type)) {
      continue;
    }
    if (stream_context_[stream_type]->free_buffers.empty()) {
      return false;
    }
  }
  return true;
}

bool StreamBufferManager::HasStreamsConfigured(
    std::initializer_list<StreamType> stream_types) {
  for (auto stream_type : stream_types) {
    if (stream_context_.find(stream_type) == stream_context_.end()) {
      return false;
    }
  }
  return true;
}

void StreamBufferManager::SetUpStreamsAndBuffers(
    VideoCaptureFormat capture_format,
    const cros::mojom::CameraMetadataPtr& static_metadata,
    std::vector<cros::mojom::Camera3StreamPtr> streams) {
  DestroyCurrentStreamsAndBuffers();

  for (auto& stream : streams) {
    DVLOG(2) << "Stream " << stream->id
             << " stream_type: " << stream->stream_type
             << " configured: usage=" << stream->usage
             << " max_buffers=" << stream->max_buffers;

    const size_t kMaximumAllowedBuffers = 15;
    if (stream->max_buffers > kMaximumAllowedBuffers) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerHalRequestedTooManyBuffers,
          FROM_HERE,
          std::string("Camera HAL requested ") +
              base::NumberToString(stream->max_buffers) +
              std::string(" buffers which exceeds the allowed maximum "
                          "number of buffers"));
      return;
    }

    // A better way to tell the stream type here would be to check on the usage
    // flags of the stream.
    StreamType stream_type = StreamIdToStreamType(stream->id);
    stream_context_[stream_type] = std::make_unique<StreamContext>();
    stream_context_[stream_type]->capture_format = capture_format;
    stream_context_[stream_type]->stream = std::move(stream);

    const ChromiumPixelFormat stream_format =
        camera_buffer_factory_->ResolveStreamBufferFormat(
            stream_context_[stream_type]->stream->format);
    stream_context_[stream_type]->capture_format.pixel_format =
        stream_format.video_format;

    // For input stream, there is no need to allocate buffers.
    if (IsInputStream(stream_type)) {
      continue;
    }

    // Allocate buffers.
    size_t num_buffers = stream_context_[stream_type]->stream->max_buffers;
    stream_context_[stream_type]->buffers.resize(num_buffers);
    int32_t buffer_width, buffer_height;
    switch (stream_type) {
      case StreamType::kPreviewOutput:
      case StreamType::kYUVOutput: {
        buffer_width = stream_context_[stream_type]->stream->width;
        buffer_height = stream_context_[stream_type]->stream->height;
        break;
      }
      case StreamType::kJpegOutput: {
        const cros::mojom::CameraMetadataEntryPtr* jpeg_max_size =
            GetMetadataEntry(
                static_metadata,
                cros::mojom::CameraMetadataTag::ANDROID_JPEG_MAX_SIZE);
        buffer_width =
            *reinterpret_cast<int32_t*>((*jpeg_max_size)->data.data());
        buffer_height = 1;
        break;
      }
      default: {
        NOTREACHED();
      }
    }
    for (size_t j = 0; j < num_buffers; ++j) {
      auto buffer = camera_buffer_factory_->CreateGpuMemoryBuffer(
          gfx::Size(buffer_width, buffer_height), stream_format.gfx_format);
      if (!buffer) {
        device_context_->SetErrorState(
            media::VideoCaptureError::
                kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer,
            FROM_HERE, "Failed to create GpuMemoryBuffer");
        return;
      }
      bool ret = buffer->Map();
      if (!ret) {
        device_context_->SetErrorState(
            media::VideoCaptureError::
                kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer,
            FROM_HERE, "Failed to map GpuMemoryBuffer");
        return;
      }
      stream_context_[stream_type]->buffers[j] = std::move(buffer);
      stream_context_[stream_type]->free_buffers.push(
          GetBufferIpcId(stream_type, j));
    }
    DVLOG(2) << "Allocated "
             << stream_context_[stream_type]->stream->max_buffers << " buffers";
  }
}

cros::mojom::Camera3StreamPtr StreamBufferManager::GetStreamConfiguration(
    StreamType stream_type) {
  if (!stream_context_.count(stream_type)) {
    return cros::mojom::Camera3Stream::New();
  }
  return stream_context_[stream_type]->stream.Clone();
}

base::Optional<BufferInfo> StreamBufferManager::RequestBuffer(
    StreamType stream_type,
    base::Optional<uint64_t> buffer_id) {
  VideoPixelFormat buffer_format =
      stream_context_[stream_type]->capture_format.pixel_format;
  uint32_t drm_format = PixFormatVideoToDrm(buffer_format);
  if (!drm_format) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerUnsupportedVideoPixelFormat,
        FROM_HERE,
        std::string("Unsupported video pixel format") +
            VideoPixelFormatToString(buffer_format));
    return {};
  }

  BufferInfo buffer_info;
  if (buffer_id.has_value()) {
    // Currently, only kYUVInput has an associated output buffer which is
    // kYUVOutput.
    if (stream_type != StreamType::kYUVInput) {
      return {};
    }
    buffer_info.id = *buffer_id;
    buffer_info.gpu_memory_buffer =
        stream_context_[StreamType::kYUVOutput]
            ->buffers[GetBufferIndex(buffer_info.id)]
            .get();
  } else {
    buffer_info.id = stream_context_[stream_type]->free_buffers.front();
    stream_context_[stream_type]->free_buffers.pop();
    buffer_info.gpu_memory_buffer =
        stream_context_[stream_type]
            ->buffers[GetBufferIndex(buffer_info.id)]
            .get();
  }
  buffer_info.hal_pixel_format = stream_context_[stream_type]->stream->format;
  buffer_info.drm_format = drm_format;
  return buffer_info;
}

void StreamBufferManager::ReleaseBuffer(StreamType stream_type,
                                        uint64_t buffer_id) {
  if (IsInputStream(stream_type)) {
    return;
  }
  stream_context_[stream_type]->free_buffers.push(buffer_id);
}

bool StreamBufferManager::IsReprocessSupported() {
  return stream_context_.find(StreamType::kYUVOutput) != stream_context_.end();
}

// static
uint64_t StreamBufferManager::GetBufferIpcId(StreamType stream_type,
                                             size_t index) {
  uint64_t id = 0;
  id |= static_cast<uint64_t>(stream_type) << 32;
  id |= index;
  return id;
}

// static
size_t StreamBufferManager::GetBufferIndex(uint64_t buffer_id) {
  return buffer_id & 0xFFFFFFFF;
}

StreamBufferManager::StreamContext::StreamContext() = default;

StreamBufferManager::StreamContext::~StreamContext() = default;

}  // namespace media
