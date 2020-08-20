// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/graphics.h"

#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "base/check_op.h"
#include "pdf/ppapi_migration/callback.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "pdf/ppapi_migration/image.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chrome_pdf {

Graphics::Graphics(const gfx::Size& size) : size_(size) {}

PepperGraphics::PepperGraphics(const pp::InstanceHandle& instance,
                               const gfx::Size& size)
    : Graphics(size),
      pepper_graphics_(instance,
                       PPSizeFromSize(size),
                       /*is_always_opaque=*/true) {}

PepperGraphics::~PepperGraphics() = default;

bool PepperGraphics::Flush(ResultCallback callback) {
  pp::CompletionCallback pp_callback =
      PPCompletionCallbackFromResultCallback(std::move(callback));
  int32_t result = pepper_graphics_.Flush(pp_callback);
  if (result == PP_OK_COMPLETIONPENDING) {
    return true;
  }

  // Should only happen if pp::Graphics2D::Flush() is called while a callback is
  // still pending, which should never happen if PaintManager is managing all
  // flushes.
  DCHECK_EQ(PP_OK, result);
  pp_callback.Run(result);
  return false;
}

void PepperGraphics::PaintImage(const Image& image, const gfx::Rect& src_rect) {
  pepper_graphics_.PaintImageData(image.pepper_image(), pp::Point(),
                                  PPRectFromRect(src_rect));
}

void PepperGraphics::Scroll(const gfx::Rect& clip,
                            const gfx::Vector2d& amount) {
  pepper_graphics_.Scroll(PPRectFromRect(clip), PPPointFromVector(amount));
}

void PepperGraphics::SetScale(float scale) {
  bool result = pepper_graphics_.SetScale(scale);
  DCHECK(result);
}

void PepperGraphics::SetLayerTransform(float scale,
                                       const gfx::Point& origin,
                                       const gfx::Vector2d& translate) {
  bool result = pepper_graphics_.SetLayerTransform(
      scale, PPPointFromPoint(origin), PPPointFromVector(translate));
  DCHECK(result);
}

}  // namespace chrome_pdf
