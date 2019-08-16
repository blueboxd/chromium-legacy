// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frame_metadata.h"

#include "build/build_config.h"
#include "cc/trees/render_frame_metadata.h"

namespace content {

#if defined(OS_ANDROID)
DevToolsFrameMetadata::DevToolsFrameMetadata(
    const cc::RenderFrameMetadata& metadata)
    : device_scale_factor(metadata.device_scale_factor),
      page_scale_factor(metadata.page_scale_factor),
      root_scroll_offset(
          metadata.root_scroll_offset.value_or(gfx::Vector2dF())),
      top_controls_height(metadata.top_controls_height),
      top_controls_shown_ratio(metadata.top_controls_shown_ratio),
      scrollable_viewport_size(metadata.scrollable_viewport_size) {}
#else
// On non-Android, this class should never be created.
DevToolsFrameMetadata::DevToolsFrameMetadata(
    const cc::RenderFrameMetadata& metadata) {
  NOTREACHED();
}
#endif  // defined(OS_ANDROID)

}  // namespace content
