// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_COMPOSITOR_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_COMPOSITOR_UTILS_H_

#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace paint_preview {

// Creates a utility process via the service manager that is sandboxed and
// running an instance of the PaintPreviewCompositorCollectionImpl. This can be
// used to create compositor instances that composite Paint Previews into
// bitmaps. The service is killed when the remote goes out of scope.
mojo::Remote<mojom::PaintPreviewCompositorCollection>
CreateCompositorCollection();

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_COMPOSITOR_UTILS_H_
