// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_METADATA_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_METADATA_H_

#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
class RenderFrameMetadata;
}

namespace content {

// A subset of the information in RenderFrameMetadata used in DevTools with
// Android WebView.
// TODO(crbug.com/985009): Replace with RenderFrameMetadata.
struct DevToolsFrameMetadata {
 public:
  explicit DevToolsFrameMetadata(const cc::RenderFrameMetadata& metadata);

  float device_scale_factor;
  float page_scale_factor;
  gfx::Vector2dF root_scroll_offset;
  float top_controls_height;
  float top_controls_shown_ratio;
  gfx::SizeF scrollable_viewport_size;
};

}  // namespace content

#endif  //  CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_METADATA_H_
