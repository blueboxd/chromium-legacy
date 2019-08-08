// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_layout.h"

#include "base/logging.h"

namespace chrome_pdf {

namespace {

int GetWidestPageWidth(const std::vector<pp::Size>& page_sizes) {
  int widest_page_width = 0;
  for (const auto& page_size : page_sizes) {
    widest_page_width = std::max(widest_page_width, page_size.width());
  }

  return widest_page_width;
}

}  // namespace

const draw_utils::PageInsetSizes DocumentLayout::kSingleViewInsets{
    /*left=*/5, /*top=*/3, /*right=*/5, /*bottom=*/7};

DocumentLayout::Options::Options() = default;

DocumentLayout::Options::Options(const Options& other) = default;
DocumentLayout::Options& DocumentLayout::Options::operator=(
    const Options& other) = default;

DocumentLayout::Options::~Options() = default;

void DocumentLayout::Options::RotatePagesClockwise() {
  default_page_orientation_ = RotateClockwise(default_page_orientation_);
}

void DocumentLayout::Options::RotatePagesCounterclockwise() {
  default_page_orientation_ = RotateCounterclockwise(default_page_orientation_);
}

DocumentLayout::DocumentLayout() = default;

DocumentLayout::~DocumentLayout() = default;

std::vector<pp::Rect> DocumentLayout::GetSingleViewLayout(
    const std::vector<pp::Size>& page_sizes) {
  set_size({GetWidestPageWidth(page_sizes), 0});

  std::vector<pp::Rect> formatted_rects(page_sizes.size());
  for (size_t i = 0; i < page_sizes.size(); ++i) {
    if (i != 0) {
      // Add space for bottom separator.
      EnlargeHeight(kBottomSeparator);
    }

    const pp::Size& page_size = page_sizes[i];
    formatted_rects[i] =
        draw_utils::GetRectForSingleView(page_size, size_, kSingleViewInsets);
    AppendPageRect(page_size);
  }

  return formatted_rects;
}

std::vector<pp::Rect> DocumentLayout::GetTwoUpViewLayout(
    const std::vector<pp::Size>& page_sizes) {
  set_size({GetWidestPageWidth(page_sizes), 0});

  std::vector<pp::Rect> formatted_rects(page_sizes.size());
  for (size_t i = 0; i < page_sizes.size(); ++i) {
    draw_utils::PageInsetSizes page_insets =
        draw_utils::GetPageInsetsForTwoUpView(
            i, page_sizes.size(), kSingleViewInsets, kHorizontalSeparator);
    const pp::Size& page_size = page_sizes[i];

    if (i % 2 == 0) {
      formatted_rects[i] = draw_utils::GetLeftRectForTwoUpView(
          page_size, {size_.width(), size_.height()}, page_insets);
    } else {
      formatted_rects[i] = draw_utils::GetRightRectForTwoUpView(
          page_size, {size_.width(), size_.height()}, page_insets);
      EnlargeHeight(std::max(page_size.height(), page_sizes[i - 1].height()));
    }
  }

  if (page_sizes.size() % 2 == 1) {
    EnlargeHeight(page_sizes.back().height());
  }

  size_.set_width(2 * size_.width());

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
