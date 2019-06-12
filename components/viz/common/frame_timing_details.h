// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_H_
#define COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_H_

#include "ui/gfx/presentation_feedback.h"

namespace viz {

struct FrameTimingDetails {
  gfx::PresentationFeedback presentation_feedback;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_H_
