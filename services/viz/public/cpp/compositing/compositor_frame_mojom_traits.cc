// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/compositor_frame_mojom_traits.h"

#include "services/viz/public/cpp/crash_keys.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::CompositorFrameDataView, viz::CompositorFrame>::
    Read(viz::mojom::CompositorFrameDataView data, viz::CompositorFrame* out) {
  if (!data.ReadPasses(&out->render_pass_list))
    return false;

  if (out->render_pass_list.empty()) {
    viz::SetDeserializationCrashKeyString(
        "CompositorFrame::render_pass_list empty");
    return false;
  }

  if (out->render_pass_list.back()->output_rect.size().IsEmpty()) {
    viz::SetDeserializationCrashKeyString("CompositorFrame empty");
    return false;
  }

  if (!data.ReadMetadata(&out->metadata))
    return false;

  if (!data.ReadResources(&out->resource_list)) {
    viz::SetDeserializationCrashKeyString(
        "Failed read CompositorFrame::resource_list");
    return false;
  }

  return true;
}

}  // namespace mojo
