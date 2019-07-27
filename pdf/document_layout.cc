// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_layout.h"

#include "base/logging.h"
#include "pdf/draw_utils/coordinates.h"

namespace chrome_pdf {

DocumentLayout::DocumentLayout() = default;

DocumentLayout::DocumentLayout(const DocumentLayout& other) = default;
DocumentLayout& DocumentLayout::operator=(const DocumentLayout& other) =
    default;

DocumentLayout::~DocumentLayout() = default;

void DocumentLayout::RotatePagesClockwise() {
  default_page_orientation_ = (default_page_orientation_ + 1) % 4;
}

void DocumentLayout::RotatePagesCounterclockwise() {
  default_page_orientation_ = (default_page_orientation_ - 1) % 4;
}

void DocumentLayout::EnlargeHeight(int height) {
  DCHECK_GE(height, 0);
  size_.Enlarge(0, height);
}

void DocumentLayout::AppendPageRect(const pp::Size& page_rect) {
  // TODO(kmoon): Inline draw_utils::ExpandDocumentSize().
  draw_utils::ExpandDocumentSize(page_rect, &size_);
}

}  // namespace chrome_pdf
