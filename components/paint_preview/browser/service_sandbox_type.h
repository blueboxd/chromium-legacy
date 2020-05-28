// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_SERVICE_SANDBOX_TYPE_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_SERVICE_SANDBOX_TYPE_H_

#include "content/public/browser/sandbox_type.h"
#include "content/public/browser/service_process_host.h"

// This file maps service classes to sandbox types.  Services which
// require a non-utility sandbox can be added here.  See
// ServiceProcessHost::Launch() for how these templates are consumed.

// paint_preview::mojom::PaintPreviewCompositorCollection
namespace paint_preview {
namespace mojom {
class PaintPreviewCompositorCollection;
}
}  // namespace paint_preview

template <>
inline content::SandboxType content::GetServiceSandboxType<
    paint_preview::mojom::PaintPreviewCompositorCollection>() {
  // TODO(crbug/1074323): Investigate using a different SandboxType.
  return content::SandboxType::kPrintCompositor;
}

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_SERVICE_SANDBOX_TYPE_H_
