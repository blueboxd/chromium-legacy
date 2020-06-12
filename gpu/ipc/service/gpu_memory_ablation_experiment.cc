// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_ablation_experiment.h"

#include <algorithm>

#include "base/bind.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "components/viz/common/features.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {

// Main feature flag to control the entire experiment, encompassing bot CPU and
// GPU ablations.
const base::Feature kGPUMemoryAblationFeature{
    "GPUMemoryAblation", base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(jonross): Replace these feature flags with Field Trial Param lookup.
const base::Feature kGPUMemoryAblationGPUSmall{
    "GPUMemoryAblationGPUSmall", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kGPUMemoryAblationGPUMedium{
    "GPUMemoryAblationGPUMedium", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kGPUMemoryAblationGPULarge{
    "GPUMemoryAblationGPULarge", base::FEATURE_DISABLED_BY_DEFAULT};

// The size to use when allocating images. The sizes vary based on the chosen
// experiment.
constexpr gfx::Size kSmallSize(256, 256);
constexpr gfx::Size kMediumSize(256 * 4, 256 * 4);
constexpr gfx::Size kLargeSize(256 * 8, 256 * 8);

// Image allocation parameters.
constexpr viz::ResourceFormat kFormat = viz::ResourceFormat::RGBA_8888;
constexpr uint32_t kUsage = SHARED_IMAGE_USAGE_DISPLAY;

GpuMemoryAblationExperiment::GpuMemoryAblationExperiment(
    GpuChannelManager* channel_manager)
    : enabled_(base::FeatureList::IsEnabled(kGPUMemoryAblationFeature)),
      channel_manager_(channel_manager) {}

GpuMemoryAblationExperiment::~GpuMemoryAblationExperiment() = default;

void GpuMemoryAblationExperiment::OnMemoryAllocated(uint64_t old_size,
                                                    uint64_t new_size) {
  if (!enabled_)
    return;
  if (!init_) {
    InitGpu(channel_manager_);
  }
  // TODO(jonross): Investigate why there are 0 size allocations.
  if (new_size > old_size) {
    // TODO(jonross): Impl CPU ablation
    if (gpu_enabled_)
      AllocateGpuMemory();
  } else if (old_size > new_size) {
    // TODO(jonross): Impl CPU ablation
    if (gpu_enabled_ && !mailboxes_.empty()) {
      DeleteGpuMemory();
    }
  }
}

uint64_t GpuMemoryAblationExperiment::GetPeakMemory(
    uint32_t sequence_num) const {
  auto it = sequences_.find(sequence_num);
  if (it == sequences_.end())
    return 0u;

  return it->second.peak_memory_;
}

void GpuMemoryAblationExperiment::StartSequence(uint32_t sequence_num) {
  sequences_.emplace(sequence_num, SequenceTracker());
}

void GpuMemoryAblationExperiment::StopSequence(uint32_t sequence_num) {
  auto it = sequences_.find(sequence_num);
  if (it == sequences_.end())
    return;

  TRACE_EVENT_INSTANT2("gpu.memory", "Memory.GPU.PeakMemoryUsage.AblationTimes",
                       TRACE_EVENT_SCOPE_THREAD, "alloc",
                       it->second.allocs_.InMilliseconds(), "dealloc",
                       it->second.deallocs_.InMilliseconds());

  sequences_.erase(it);
}

void GpuMemoryAblationExperiment::AllocateGpuMemory() {
  // We can't successfully create an image without a context, so do not even
  // perform the initial allocations.
  if (!MakeContextCurrent())
    return;
  base::Time start = base::Time::Now();

  auto mailbox = Mailbox::GenerateForSharedImage();
  auto color_space = gfx::ColorSpace::CreateSRGB();

  if (!factory_->CreateSharedImage(mailbox, kFormat, size_, color_space,
                                   gpu::kNullSurfaceHandle, kUsage)) {
    return;
  }

  auto skia_rep = rep_factory_->ProduceSkia(mailbox, context_state_);
  if (!skia_rep)
    return;

  auto write_access = skia_rep->BeginScopedWriteAccess(
      /*begin_semaphores=*/{}, /*end_semaphores=*/{},
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!write_access)
    return;

  auto* canvas = write_access->surface()->getCanvas();
  canvas->clear(SK_ColorWHITE);

  mailboxes_.push_back(mailbox);

  base::TimeDelta delta = base::Time::Now() - start;
  for (auto& it : sequences_)
    it.second.allocs_ += delta;
}

void GpuMemoryAblationExperiment::DeleteGpuMemory() {
  if (mailboxes_.empty())
    return;
  base::Time start = base::Time::Now();

  auto mailbox = mailboxes_.front();
  // We can't successfully destroy the image if we cannot get the context,
  // however we still need to cleanup our internal state.
  if (MakeContextCurrent())
    factory_->DestroySharedImage(mailbox);

  mailboxes_.erase(mailboxes_.begin());

  base::TimeDelta delta = base::Time::Now() - start;
  for (auto& it : sequences_)
    it.second.deallocs_ += delta;
}

void GpuMemoryAblationExperiment::InitGpu(GpuChannelManager* channel_manager) {
  // GPU Info Collection Process can be created, with no graphical output
  // possible. Don't init there, as all future image operations will fail.
  if (gl::GetGLImplementation() == gl::kGLImplementationDisabled)
    return;

  if (base::FeatureList::IsEnabled(kGPUMemoryAblationGPUSmall)) {
    size_ = kSmallSize;
  } else if (base::FeatureList::IsEnabled(kGPUMemoryAblationGPUMedium)) {
    size_ = kMediumSize;
  } else if (base::FeatureList::IsEnabled(kGPUMemoryAblationGPULarge)) {
    size_ = kLargeSize;
  }

  ContextResult result;
  context_state_ = channel_manager->GetSharedContextState(&result);
  if (result != ContextResult::kSuccess || !MakeContextCurrent()) {
    context_state_ = nullptr;
    return;
  }

  gpu::GpuMemoryBufferFactory* gmb_factory =
      channel_manager->gpu_memory_buffer_factory();
  factory_ = std::make_unique<SharedImageFactory>(
      channel_manager->gpu_preferences(),
      channel_manager->gpu_driver_bug_workarounds(),
      channel_manager->gpu_feature_info(), context_state_.get(),
      channel_manager->mailbox_manager(),
      channel_manager->shared_image_manager(),
      gmb_factory ? gmb_factory->AsImageFactory() : nullptr, this,
      features::IsUsingSkiaRenderer());

  rep_factory_ = std::make_unique<SharedImageRepresentationFactory>(
      channel_manager->shared_image_manager(), this);
  gpu_enabled_ = true;
  init_ = true;
}

bool GpuMemoryAblationExperiment::MakeContextCurrent() {
  return context_state_->MakeCurrent(nullptr);
}

// MemoryTracker:
void GpuMemoryAblationExperiment::TrackMemoryAllocatedChange(int64_t delta) {
  DCHECK(delta >= 0 || gpu_allocation_size_ >= static_cast<uint64_t>(-delta));
  gpu_allocation_size_ += delta;
  for (auto& it : sequences_) {
    if (gpu_allocation_size_ > it.second.peak_memory_)
      it.second.peak_memory_ = gpu_allocation_size_;
  }
}

// Unused methods that form the basis of memory dumps
uint64_t GpuMemoryAblationExperiment::GetSize() const {
  return 0u;
}

uint64_t GpuMemoryAblationExperiment::ClientTracingId() const {
  return 0u;
}

int GpuMemoryAblationExperiment::ClientId() const {
  return 0;
}

uint64_t GpuMemoryAblationExperiment::ContextGroupTracingId() const {
  return 0u;
}

}  // namespace gpu