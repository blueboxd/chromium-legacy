// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_ready_rect.h"

#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/rect.h"

namespace chrome_pdf {

PaintReadyRect::PaintReadyRect(const pp::Rect& rect,
                               const pp::ImageData& image_data,
                               bool flush_now)
    : rect(rect), image_data(image_data), flush_now(flush_now) {}

PaintReadyRect::PaintReadyRect(const PaintReadyRect& other) = default;

PaintReadyRect& PaintReadyRect::operator=(const PaintReadyRect& other) =
    default;

}  // namespace chrome_pdf
