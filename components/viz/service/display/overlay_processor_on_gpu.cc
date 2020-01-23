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

#if defined(OS_ANDROID)
void OverlayProcessorOnGpu::NotifyOverlayPromotions(
    base::flat_set<gpu::Mailbox> promotion_denied,
    base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions) {
  for (auto& denied : promotion_denied) {
    auto shared_image_overlay =
        shared_image_representation_factory_->ProduceOverlay(denied);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image_overlay)
      continue;

    shared_image_overlay->NotifyOverlayPromotion(false, gfx::Rect());
  }
  for (auto& possible : possible_promotions) {
    auto shared_image_overlay =
        shared_image_representation_factory_->ProduceOverlay(possible.first);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image_overlay)
      continue;

    shared_image_overlay->NotifyOverlayPromotion(true, possible.second);
  }
}
#endif
}  // namespace viz
