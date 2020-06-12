// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_ABLATION_EXPERIMENT_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_ABLATION_EXPERIMENT_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class GpuChannelManager;
class SharedContextState;
class SharedImageFactory;
class SharedImageRepresentationFactory;

extern const base::Feature kGPUMemoryAblationFeature;

// When enabled, this experiment allocates additional memory alongside each
// normal allocation. This will allow a study of the correlation between
// memory usage and performance metrics.
//
// Each increase reported to OnMemoryAllocated will allocate a chunk of memory.
// Each decrease reported will release a previously allocated chunk.
//
// GpuMemoryAblationExperiment acts as the MemoryTracker for all of its own
// allocations. This prevents a cycle of memory allocations:
//    - GpuChannelManager::GpuPeakMemoryMonitor::OnMemoryAllocatedChange
//    - GpuMemoryAblationExperiment::OnMemoryAllocated
//    - MemoryTracker::TrackMemoryAllocatedChange
//    - GpuChannelManager::GpuPeakMemoryMonitor::OnMemoryAllocatedChange
//    - etc.
//
// Instead this will track the memory it allocated, which can be retrieved via
// GetPeakMemory.
class GPU_IPC_SERVICE_EXPORT GpuMemoryAblationExperiment
    : public MemoryTracker {
 public:
  explicit GpuMemoryAblationExperiment(GpuChannelManager* channel_manager);
  ~GpuMemoryAblationExperiment() override;

  // Allocates a chunk of memory in response to increases. Reported decreases
  // will release previously allocated chunks. The amount of memory allocated
  // is returned in bytes.
  void OnMemoryAllocated(uint64_t old_size, uint64_t new_size);

  uint64_t GetPeakMemory(uint32_t sequence_num) const;
  void StartSequence(uint32_t sequence_num);
  void StopSequence(uint32_t sequence_num);

 private:
  // Tracks the time spent doing the allocations/deallocations in order to
  // determine if the change in metrics was solely due to the ablation.
  //
  // The memory allocated for ablation is not reported directly to
  // GpuChannelManager::GpuPeakMemoryMonitor, as GpuMemoryAblationExperiment
  // acts as the MemoryTracker for its own allocations. This tracks the peak
  // allocation so that it can be reported.
  struct SequenceTracker {
   public:
    SequenceTracker() = default;
    ~SequenceTracker() = default;

    base::TimeDelta allocs_;
    base::TimeDelta deallocs_;
    uint64_t peak_memory_ = 0u;
  };

  void AllocateGpuMemory();
  void DeleteGpuMemory();

  // Setups the Gpu resources needed to allocate Gpu RAM. These are influenced
  // by SharedImageStub. Which is not used directly as there is no external
  // host to pair a GpuChannel with.
  void InitGpu(GpuChannelManager* channel_manager);

  // This must be called before any actions on |factory_|. If this method fails
  // then subsequent work on the |factory_| will fail. Also influenced by
  // SharedImageStub.
  bool MakeContextCurrent();

  // MemoryTracker:
  void TrackMemoryAllocatedChange(int64_t delta) override;
  uint64_t GetSize() const override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;
  uint64_t ContextGroupTracingId() const override;

  // Whether or not the entire experiment is enabled.
  bool enabled_;
  bool init_ = false;
  // If |true| then a Gpu ablation was requested, and initialization succeeded.
  bool gpu_enabled_ = false;

  // Size of image to allocate, determined by experiment parameters.
  gfx::Size size_;

  // The Mailboxes allocated for each image.
  std::vector<Mailbox> mailboxes_;

  // Tracks the time spent doing the allocations/deallocations, along with the
  // peak memory allocated. Thus allowing to determine if the change in only
  // metrics was solely due to the ablation.
  base::flat_map<uint32_t, SequenceTracker> sequences_;

  // The memory allocated for ablation is not reported directly to
  // GpuChannelManager::GpuPeakMemoryMonitor, as this class acts as the
  // MemoryTracker for its own allocations. Tracks the current amount of
  //  memory allocated as a part of the ablation.
  uint64_t gpu_allocation_size_ = 0;

  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<SharedImageFactory> factory_;
  std::unique_ptr<SharedImageRepresentationFactory> rep_factory_;
  GpuChannelManager* channel_manager_;
  base::WeakPtrFactory<GpuMemoryAblationExperiment> weak_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_ABLATION_EXPERIMENT_H_
