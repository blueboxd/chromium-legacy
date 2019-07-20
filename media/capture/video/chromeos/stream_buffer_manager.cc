// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/stream_buffer_manager.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/pixel_format_utils.h"
#include "media/capture/video/chromeos/request_builder.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

StreamBufferManager::StreamBufferManager(
    CameraDeviceContext* device_context,
    bool video_capture_use_gmb,
    std::unique_ptr<CameraBufferFactory> camera_buffer_factory)
    : device_context_(device_context),
      video_capture_use_gmb_(video_capture_use_gmb),
      camera_buffer_factory_(std::move(camera_buffer_factory)),
      weak_ptr_factory_(this) {
  if (video_capture_use_gmb_) {
    gmb_support_ = std::make_unique<gpu::GpuMemoryBufferSupport>();
  }
}

StreamBufferManager::~StreamBufferManager() {
  DestroyCurrentStreamsAndBuffers();
}

void StreamBufferManager::ReserveBuffer(StreamType stream_type) {
  if (video_capture_use_gmb_) {
    ReserveBufferFromPool(stream_type);
  } else {
    ReserveBufferFromFactory(stream_type);
  }
}

gfx::GpuMemoryBuffer* StreamBufferManager::GetGpuMemoryBufferById(
    StreamType stream_type,
    uint64_t buffer_ipc_id) {
  auto& stream_context = stream_context_[stream_type];
  int key = GetBufferKey(buffer_ipc_id);
  auto it = stream_context->buffers.find(key);
  if (it == stream_context->buffers.end()) {
    LOG(ERROR) << "Invalid buffer: " << key << " for stream: " << stream_type;
    return nullptr;
  }
  return it->second.gmb.get();
}

base::Optional<StreamBufferManager::Buffer>
StreamBufferManager::AcquireBufferForClientById(StreamType stream_type,
                                                uint64_t buffer_ipc_id) {
  DCHECK(stream_context_.count(stream_type));
  auto& stream_context = stream_context_[stream_type];
  auto it = stream_context->buffers.find(GetBufferKey(buffer_ipc_id));
  if (it == stream_context->buffers.end()) {
    LOG(ERROR) << "Invalid buffer: " << buffer_ipc_id
               << " for stream: " << stream_type;
    return base::nullopt;
  }
  auto buffer_pair = std::move(it->second);
  stream_context->buffers.erase(it);
  return std::move(buffer_pair.vcd_buffer);
}

VideoCaptureFormat StreamBufferManager::GetStreamCaptureFormat(
    StreamType stream_type) {
  return stream_context_[stream_type]->capture_format;
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
    auto stream_context = std::make_unique<StreamContext>();
    stream_context->capture_format = capture_format;
    stream_context->stream = std::move(stream);

    const ChromiumPixelFormat stream_format =
        camera_buffer_factory_->ResolveStreamBufferFormat(
            stream_context->stream->format);
    // Internally we keep track of the VideoPixelFormat that's actually
    // supported by the camera instead of the one requested by the client.
    stream_context->capture_format.pixel_format = stream_format.video_format;

    switch (stream_type) {
      case StreamType::kPreviewOutput:
      case StreamType::kYUVInput:
      case StreamType::kYUVOutput:
        stream_context->buffer_dimension = gfx::Size(
            stream_context->stream->width, stream_context->stream->height);
        break;
      case StreamType::kJpegOutput: {
        auto jpeg_size = GetMetadataEntryAsSpan<int32_t>(
            static_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_JPEG_MAX_SIZE);
        CHECK_EQ(jpeg_size.size(), 1u);
        stream_context->buffer_dimension = gfx::Size(jpeg_size[0], 1);
        break;
      }
      default: {
        NOTREACHED();
      }
    }
    stream_context_[stream_type] = std::move(stream_context);

    // For input stream, there is no need to allocate buffers.
    if (IsInputStream(stream_type)) {
      continue;
    }

    // Allocate buffers.
    for (size_t j = 0; j < stream_context_[stream_type]->stream->max_buffers;
         ++j) {
      ReserveBuffer(stream_type);
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

base::Optional<BufferInfo> StreamBufferManager::RequestBufferForCaptureRequest(
    StreamType stream_type,
    base::Optional<uint64_t> buffer_ipc_id) {
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
  if (buffer_ipc_id.has_value()) {
    // Currently, only kYUVInput has an associated output buffer which is
    // kYUVOutput.
    if (stream_type != StreamType::kYUVInput) {
      return {};
    }
    int key = GetBufferKey(*buffer_ipc_id);
    const auto& stream_context = stream_context_[StreamType::kYUVOutput];
    auto it = stream_context->buffers.find(key);
    CHECK(it != stream_context->buffers.end());
    buffer_info.ipc_id = *buffer_ipc_id;
    buffer_info.dimension = stream_context->buffer_dimension;
    buffer_info.gpu_memory_buffer_handle = it->second.gmb->CloneHandle();
  } else {
    const auto& stream_context = stream_context_[stream_type];
    CHECK(!stream_context->free_buffers.empty());
    int key = stream_context->free_buffers.front();
    auto it = stream_context->buffers.find(key);
    CHECK(it != stream_context->buffers.end());
    stream_context->free_buffers.pop();
    buffer_info.ipc_id = GetBufferIpcId(stream_type, key);
    buffer_info.dimension = stream_context->buffer_dimension;
    buffer_info.gpu_memory_buffer_handle = it->second.gmb->CloneHandle();
  }
  buffer_info.drm_format = drm_format;
  buffer_info.hal_pixel_format = stream_context_[stream_type]->stream->format;
  return buffer_info;
}

void StreamBufferManager::ReleaseBufferFromCaptureResult(
    StreamType stream_type,
    uint64_t buffer_ipc_id) {
  if (IsInputStream(stream_type)) {
    return;
  }
  stream_context_[stream_type]->free_buffers.push(GetBufferKey(buffer_ipc_id));
}

gfx::Size StreamBufferManager::GetBufferDimension(StreamType stream_type) {
  DCHECK(stream_context_.count(stream_type));
  return stream_context_[stream_type]->buffer_dimension;
}

bool StreamBufferManager::IsReprocessSupported() {
  return stream_context_.find(StreamType::kYUVOutput) != stream_context_.end();
}

// static
uint64_t StreamBufferManager::GetBufferIpcId(StreamType stream_type, int key) {
  uint64_t id = 0;
  id |= static_cast<uint64_t>(stream_type) << 32;
  DCHECK_GE(key, 0);
  id |= static_cast<uint32_t>(key);
  return id;
}

// static
int StreamBufferManager::GetBufferKey(uint64_t buffer_ipc_id) {
  return buffer_ipc_id & 0xFFFFFFFF;
}

void StreamBufferManager::ReserveBufferFromFactory(StreamType stream_type) {
  auto& stream_context = stream_context_[stream_type];
  base::Optional<gfx::BufferFormat> gfx_format =
      PixFormatVideoToGfx(stream_context->capture_format.pixel_format);
  if (!gfx_format) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer,
        FROM_HERE, "Unsupported video pixel format");
    return;
  }
  auto gmb = camera_buffer_factory_->CreateGpuMemoryBuffer(
      stream_context->buffer_dimension, *gfx_format);
  if (!gmb) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer,
        FROM_HERE, "Failed to allocate GPU memory buffer");
    return;
  }
  if (!gmb->Map()) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer,
        FROM_HERE, "Failed to map GPU memory buffer");
    return;
  }
  // All the GpuMemoryBuffers are allocated from the factory in bulk when the
  // streams are configured.  Here we simply use the sequence of the allocated
  // buffer as the buffer id.
  int key = stream_context->buffers.size() + 1;
  stream_context->free_buffers.push(key);
  stream_context->buffers.insert(
      std::make_pair(key, BufferPair(std::move(gmb), base::nullopt)));
}

void StreamBufferManager::ReserveBufferFromPool(StreamType stream_type) {
  auto& stream_context = stream_context_[stream_type];
  base::Optional<gfx::BufferFormat> gfx_format =
      PixFormatVideoToGfx(stream_context->capture_format.pixel_format);
  if (!gfx_format) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer,
        FROM_HERE, "Unsupported video pixel format");
    return;
  }
  Buffer vcd_buffer;
  if (!device_context_->ReserveVideoCaptureBufferFromPool(
          stream_context->buffer_dimension,
          stream_context->capture_format.pixel_format, &vcd_buffer)) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer,
        FROM_HERE, "Failed to reserve video capture buffer");
    return;
  }
  auto gmb = gmb_support_->CreateGpuMemoryBufferImplFromHandle(
      vcd_buffer.handle_provider->GetGpuMemoryBufferHandle(),
      stream_context->buffer_dimension, *gfx_format,
      CameraBufferFactory::GetBufferUsage(*gfx_format), base::NullCallback());
  stream_context->free_buffers.push(vcd_buffer.id);
  stream_context->buffers.insert(std::make_pair(
      vcd_buffer.id, BufferPair(std::move(gmb), std::move(vcd_buffer))));
}

void StreamBufferManager::DestroyCurrentStreamsAndBuffers() {
  for (const auto& iter : stream_context_) {
    if (iter.second) {
      if (!video_capture_use_gmb_) {
        // The GMB is mapped by default only when it's allocated locally.
        for (auto& buf : iter.second->buffers) {
          auto& buf_pair = buf.second;
          if (buf_pair.gmb) {
            buf_pair.gmb->Unmap();
          }
        }
        iter.second->buffers.clear();
      }
    }
  }
  stream_context_.clear();
}

StreamBufferManager::BufferPair::BufferPair(
    std::unique_ptr<gfx::GpuMemoryBuffer> input_gmb,
    base::Optional<Buffer> input_vcd_buffer)
    : gmb(std::move(input_gmb)), vcd_buffer(std::move(input_vcd_buffer)) {}

StreamBufferManager::BufferPair::BufferPair(
    StreamBufferManager::BufferPair&& other) {
  gmb = std::move(other.gmb);
  vcd_buffer = std::move(other.vcd_buffer);
}

StreamBufferManager::BufferPair::~BufferPair() = default;

StreamBufferManager::StreamContext::StreamContext() = default;

StreamBufferManager::StreamContext::~StreamContext() = default;

}  // namespace media
