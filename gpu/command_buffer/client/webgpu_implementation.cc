// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/webgpu_implementation.h"

#include <algorithm>
#include <vector>

#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/dawn_client_memory_transfer_service.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"

#define GPU_CLIENT_SINGLE_THREAD_CHECK()

namespace gpu {
namespace webgpu {

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_implementation_impl_autogen.h"

WebGPUImplementation::WebGPUImplementation(
    WebGPUCmdHelper* helper,
    TransferBufferInterface* transfer_buffer,
    GpuControl* gpu_control)
    : ImplementationBase(helper, transfer_buffer, gpu_control),
      helper_(helper),
      c2s_buffer_(helper, transfer_buffer) {
}

WebGPUImplementation::~WebGPUImplementation() {
  // Wait for all commands to finish or we may free shared memory while
  // commands are still in flight.
  Flush();
  helper_->Finish();

#if BUILDFLAG(USE_DAWN)
  // Now that commands are finished, free the wire client.
  wire_client_.reset();

  // All client-side Dawn objects are now destroyed.
  // Shared memory allocations for buffers that were still mapped at the time
  // of destruction can now be safely freed.
  memory_transfer_service_->FreeHandlesPendingToken(helper_->InsertToken());
  helper_->Finish();
#endif
}

gpu::ContextResult WebGPUImplementation::Initialize(
    const SharedMemoryLimits& limits) {
  TRACE_EVENT0("gpu", "WebGPUImplementation::Initialize");
  auto result = ImplementationBase::Initialize(limits);
  if (result != gpu::ContextResult::kSuccess) {
    return result;
  }

  c2s_buffer_default_size_ = limits.start_transfer_buffer_size;
  DCHECK_GT(c2s_buffer_default_size_, 0u);

#if BUILDFLAG(USE_DAWN)
  memory_transfer_service_.reset(
      new DawnClientMemoryTransferService(mapped_memory_.get()));

  dawn_wire::WireClientDescriptor descriptor = {};
  descriptor.serializer = this;
  descriptor.memoryTransferService = memory_transfer_service_.get();

  wire_client_.reset(new dawn_wire::WireClient(descriptor));

  procs_ = wire_client_->GetProcs();
#endif

  return gpu::ContextResult::kSuccess;
}

// ContextSupport implementation.
void WebGPUImplementation::SetAggressivelyFreeResources(
    bool aggressively_free_resources) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::Swap(uint32_t flags,
                                SwapCompletedCallback complete_callback,
                                PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::SwapWithBounds(
    const std::vector<gfx::Rect>& rects,
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::PartialSwapBuffers(
    const gfx::Rect& sub_buffer,
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::CommitOverlayPlanes(
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTREACHED();
}
void WebGPUImplementation::ScheduleOverlayPlane(
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    unsigned overlay_texture_id,
    const gfx::Rect& display_bounds,
    const gfx::RectF& uv_rect,
    bool enable_blend,
    unsigned gpu_fence_id) {
  NOTREACHED();
}
uint64_t WebGPUImplementation::ShareGroupTracingGUID() const {
  NOTIMPLEMENTED();
  return 0;
}
void WebGPUImplementation::SetErrorMessageCallback(
    base::RepeatingCallback<void(const char*, int32_t)> callback) {
  NOTIMPLEMENTED();
}
bool WebGPUImplementation::ThreadSafeShallowLockDiscardableTexture(
    uint32_t texture_id) {
  NOTREACHED();
  return false;
}
void WebGPUImplementation::CompleteLockDiscardableTexureOnContextThread(
    uint32_t texture_id) {
  NOTREACHED();
}
bool WebGPUImplementation::ThreadsafeDiscardableTextureIsDeletedForTracing(
    uint32_t texture_id) {
  NOTREACHED();
  return false;
}
void* WebGPUImplementation::MapTransferCacheEntry(uint32_t serialized_size) {
  NOTREACHED();
  return nullptr;
}
void WebGPUImplementation::UnmapAndCreateTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  NOTREACHED();
}
bool WebGPUImplementation::ThreadsafeLockTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  NOTREACHED();
  return false;
}
void WebGPUImplementation::UnlockTransferCacheEntries(
    const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
  NOTREACHED();
}
void WebGPUImplementation::DeleteTransferCacheEntry(uint32_t type,
                                                    uint32_t id) {
  NOTREACHED();
}
unsigned int WebGPUImplementation::GetTransferBufferFreeSize() const {
  NOTREACHED();
  return 0;
}
bool WebGPUImplementation::IsJpegDecodeAccelerationSupported() const {
  NOTREACHED();
  return false;
}
bool WebGPUImplementation::IsWebPDecodeAccelerationSupported() const {
  NOTREACHED();
  return false;
}
bool WebGPUImplementation::CanDecodeWithHardwareAcceleration(
    const cc::ImageHeaderMetadata* image_metadata) const {
  NOTREACHED();
  return false;
}

// InterfaceBase implementation.
void WebGPUImplementation::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  ImplementationBase::GenSyncToken(sync_token);
}
void WebGPUImplementation::GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) {
  ImplementationBase::GenUnverifiedSyncToken(sync_token);
}
void WebGPUImplementation::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                    GLsizei count) {
  ImplementationBase::VerifySyncTokens(sync_tokens, count);
}
void WebGPUImplementation::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {
  ImplementationBase::WaitSyncToken(sync_token);
}

// ImplementationBase implementation.
void WebGPUImplementation::IssueShallowFlush() {
  NOTIMPLEMENTED();
}

void WebGPUImplementation::SetGLError(GLenum error,
                                      const char* function_name,
                                      const char* msg) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] Client Synthesized Error: "
                     << gles2::GLES2Util::GetStringError(error) << ": "
                     << function_name << ": " << msg);
  NOTIMPLEMENTED();
}

// GpuControlClient implementation.
void WebGPUImplementation::OnGpuControlLostContext() {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlLostContextMaybeReentrant() {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlErrorMessage(const char* message,
                                                    int32_t id) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlSwapBuffersCompleted(
    const SwapBuffersCompleteParams& params) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnSwapBufferPresented(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlReturnData(
    base::span<const uint8_t> data) {
#if BUILDFLAG(USE_DAWN)

  static uint32_t return_trace_id = 0;
  TRACE_EVENT_FLOW_END0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                        "DawnReturnCommands", return_trace_id++);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
               "WebGPUImplementation::OnGpuControlReturnData", "bytes",
               data.size());

  if (data.size() <= sizeof(cmds::DawnReturnDataHeader)) {
    // TODO(jiawei.shao@intel.com): Lose the context.
    NOTREACHED();
  }
  const cmds::DawnReturnDataHeader& dawnReturnDataHeader =
      *reinterpret_cast<const cmds::DawnReturnDataHeader*>(data.data());

  const uint8_t* dawnReturnDataBody =
      data.data() + sizeof(cmds::DawnReturnDataHeader);
  size_t dawnReturnDataSize = data.size() - sizeof(cmds::DawnReturnDataHeader);

  switch (dawnReturnDataHeader.return_data_type) {
    case DawnReturnDataType::kDawnCommands:
      if (!wire_client_->HandleCommands(
              reinterpret_cast<const char*>(dawnReturnDataBody),
              dawnReturnDataSize)) {
        // TODO(enga): Lose the context.
        NOTREACHED();
      }
      break;
    case DawnReturnDataType::kRequestedDawnAdapterProperties: {
      const cmds::DawnReturnAdapterInfo* returned_adapter_info =
          reinterpret_cast<const cmds::DawnReturnAdapterInfo*>(
              dawnReturnDataBody);

      GLuint request_adapter_serial =
          returned_adapter_info->adapter_ids.request_adapter_serial;
      auto request_callback_iter =
          request_adapter_callback_map_.find(request_adapter_serial);
      if (request_callback_iter == request_adapter_callback_map_.end()) {
        // TODO(jiawei.shao@intel.com): Lose the context.
        NOTREACHED();
        break;
      }
      auto& request_callback = request_callback_iter->second;
      GLuint adapter_service_id =
          returned_adapter_info->adapter_ids.adapter_service_id;
      WGPUDeviceProperties adapter_properties = {};
      const volatile char* deserialized_buffer =
          reinterpret_cast<const volatile char*>(
              returned_adapter_info->deserialized_buffer);
      dawn_wire::DeserializeWGPUDeviceProperties(&adapter_properties,
                                                 deserialized_buffer);
      std::move(request_callback).Run(adapter_service_id, adapter_properties);
      request_adapter_callback_map_.erase(request_callback_iter);
    } break;
    default:
      // TODO(jiawei.shao@intel.com): Lose the context.
      NOTREACHED();
      break;
  }
#endif
}

void* WebGPUImplementation::GetCmdSpace(size_t size) {
  // The buffer size must be initialized before any commands are serialized.
  if (c2s_buffer_default_size_ == 0u) {
    NOTREACHED();
    return nullptr;
  }

  base::CheckedNumeric<uint32_t> checked_next_offset(c2s_put_offset_);
  checked_next_offset += size;

  uint32_t next_offset;
  bool next_offset_valid = checked_next_offset.AssignIfValid(&next_offset);

  // If the buffer does not have enough space, or if the buffer is not
  // initialized, flush and reset the command stream.
  if (!next_offset_valid || next_offset > c2s_buffer_.size() ||
      !c2s_buffer_.valid()) {
    Flush();

    uint32_t max_allocation = transfer_buffer_->GetMaxSize();
    // TODO(crbug.com/951558): Handle command chunking or ensure commands aren't
    // this large.
    CHECK_LE(size, max_allocation);

    uint32_t allocation_size =
        std::max(c2s_buffer_default_size_, static_cast<uint32_t>(size));
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "WebGPUImplementation::GetCmdSpace", "bytes", allocation_size);
    c2s_buffer_.Reset(allocation_size);
    c2s_put_offset_ = 0;
    next_offset = size;

    // TODO(crbug.com/951558): Handle OOM.
    CHECK(c2s_buffer_.valid());
    CHECK_LE(size, c2s_buffer_.size());
  }

  DCHECK(c2s_buffer_.valid());
  uint8_t* ptr = static_cast<uint8_t*>(c2s_buffer_.address());
  ptr += c2s_put_offset_;

  c2s_put_offset_ = next_offset;
  return ptr;
}

bool WebGPUImplementation::Flush() {
  if (c2s_buffer_.valid()) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "WebGPUImplementation::Flush", "bytes", c2s_put_offset_);

    TRACE_EVENT_FLOW_BEGIN0(
        TRACE_DISABLED_BY_DEFAULT("gpu.dawn"), "DawnCommands",
        (static_cast<uint64_t>(c2s_buffer_.shm_id()) << 32) +
            c2s_buffer_.offset());

    c2s_buffer_.Shrink(c2s_put_offset_);
    helper_->DawnCommands(c2s_buffer_.shm_id(), c2s_buffer_.offset(),
                          c2s_put_offset_);
    c2s_put_offset_ = 0;
    c2s_buffer_.Release();
  }
#if BUILDFLAG(USE_DAWN)
  memory_transfer_service_->FreeHandlesPendingToken(helper_->InsertToken());
#endif
  return true;
}

const DawnProcTable& WebGPUImplementation::GetProcs() const {
#if !BUILDFLAG(USE_DAWN)
  NOTREACHED();
#endif
  return procs_;
}

void WebGPUImplementation::FlushCommands() {
  Flush();
  helper_->Flush();
}

WGPUDevice WebGPUImplementation::GetDefaultDevice() {
#if BUILDFLAG(USE_DAWN)
  return wire_client_->GetDevice();
#else
  NOTREACHED();
  return {};
#endif
}

ReservedTexture WebGPUImplementation::ReserveTexture(WGPUDevice device) {
#if BUILDFLAG(USE_DAWN)
  dawn_wire::ReservedTexture reservation = wire_client_->ReserveTexture(device);
  return {reservation.texture, reservation.id, reservation.generation};
#else
  NOTREACHED();
  return {};
#endif
}

uint32_t WebGPUImplementation::NextRequestAdapterSerial() {
  return ++request_adapter_serial_;
}

bool WebGPUImplementation::RequestAdapterAsync(
    PowerPreference power_preference,
    base::OnceCallback<void(uint32_t, const WGPUDeviceProperties&)>
        request_adapter_callback) {
  uint32_t request_adapter_serial = NextRequestAdapterSerial();

  // Avoid the overflow of request_adapter_serial and old slot being reused.
  if (request_adapter_callback_map_.find(request_adapter_serial) !=
      request_adapter_callback_map_.end()) {
    return false;
  }

  helper_->RequestAdapter(request_adapter_serial,
                          static_cast<uint32_t>(power_preference));
  helper_->Flush();

  request_adapter_callback_map_[request_adapter_serial] =
      std::move(request_adapter_callback);

  return true;
}

bool WebGPUImplementation::RequestDevice(
    uint32_t requested_adapter_id,
    const WGPUDeviceProperties* requested_device_properties) {
#if BUILDFLAG(USE_DAWN)
  if (!requested_device_properties) {
    helper_->RequestDevice(requested_adapter_id, 0, 0, 0);
    return true;
  }

  size_t serialized_device_properties_size =
      dawn_wire::SerializedWGPUDevicePropertiesSize(
          requested_device_properties);
  DCHECK_NE(0u, serialized_device_properties_size);

  // Both transfer_buffer and c2s_buffer_ are created with transfer_buffer_,
  // so we need to make c2s_buffer_ clean before transferring
  // requested_device_properties with transfer_buffer.
  Flush();
  ScopedTransferBufferPtr transfer_buffer(serialized_device_properties_size,
                                          helper_, transfer_buffer_);
  dawn_wire::SerializeWGPUDeviceProperties(
      requested_device_properties,
      reinterpret_cast<char*>(transfer_buffer.address()));
  helper_->RequestDevice(requested_adapter_id, transfer_buffer.shm_id(),
                         transfer_buffer.offset(),
                         serialized_device_properties_size);
  transfer_buffer.Release();
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

}  // namespace webgpu
}  // namespace gpu
