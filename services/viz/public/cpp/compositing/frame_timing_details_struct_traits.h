// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_TIMING_DETAILS_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_TIMING_DETAILS_STRUCT_TRAITS_H_

#include "components/viz/common/frame_timing_details.h"
#include "services/viz/public/interfaces/compositing/frame_timing_details.mojom-shared.h"

#include "ui/gfx/presentation_feedback.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::FrameTimingDetailsDataView,
                    viz::FrameTimingDetails> {
  static gfx::PresentationFeedback presentation_feedback(
      const viz::FrameTimingDetails& frame_timing_details) {
    return frame_timing_details.presentation_feedback;
  }

  static bool Read(viz::mojom::FrameTimingDetailsDataView data,
                   viz::FrameTimingDetails* out) {
    return data.ReadPresentationFeedback(&out->presentation_feedback);
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_TIMING_DETAILS_STRUCT_TRAITS_H_
