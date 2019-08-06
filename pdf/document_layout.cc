// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_layout.h"

#include "base/logging.h"

namespace chrome_pdf {

const draw_utils::PageInsetSizes DocumentLayout::kSingleViewInsets{
    /*left=*/5, /*top=*/3, /*right=*/5, /*bottom=*/7};

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

std::vector<pp::Rect> DocumentLayout::GetTwoUpViewLayout(
    const std::vector<pp::Rect>& page_rects) {
  std::vector<pp::Rect> formatted_rects(page_rects.size());
  // Reset the current layout's height for new two-up view layout.
  size_.set_height(0);

  for (size_t i = 0; i < page_rects.size(); ++i) {
    draw_utils::PageInsetSizes page_insets =
        draw_utils::GetPageInsetsForTwoUpView(
            i, page_rects.size(), kSingleViewInsets, kHorizontalSeparator);
    const pp::Size& page_size = page_rects[i].size();

    if (i % 2 == 0) {
      formatted_rects[i] = draw_utils::GetLeftRectForTwoUpView(
          page_size, {size_.width(), size_.height()}, page_insets);
    } else {
      formatted_rects[i] = draw_utils::GetRightRectForTwoUpView(
          page_size, {size_.width(), size_.height()}, page_insets);
      EnlargeHeight(
          std::max(page_size.height(), page_rects[i - 1].size().height()));
    }
  }

  if (page_rects.size() % 2 == 1) {
    EnlargeHeight(page_rects.back().size().height());
  }

  return formatted_rects;
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
