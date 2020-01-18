// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_on_gpu.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"

namespace viz {

OverlayProcessorOnGpu::OverlayProcessorOnGpu(
    gpu::SharedImageManager* shared_image_manager)
    : shared_image_representation_factory_(
          std::make_unique<gpu::SharedImageRepresentationFactory>(
              shared_image_manager,
              nullptr)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

OverlayProcessorOnGpu::~OverlayProcessorOnGpu() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void OverlayProcessorOnGpu::ScheduleOverlays(
    CandidateList&& overlay_candidates) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(weiliangc): Use shared image to schedule overlays.
}
}  // namespace viz
