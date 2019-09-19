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

pp::Rect InsetRect(pp::Rect rect,
                   const draw_utils::PageInsetSizes& inset_sizes) {
  rect.Inset(inset_sizes.left, inset_sizes.top, inset_sizes.right,
             inset_sizes.bottom);
  return rect;
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

void DocumentLayout::ComputeSingleViewLayout(
    const std::vector<pp::Size>& page_sizes) {
  size_ = {GetWidestPageWidth(page_sizes), 0};

  page_layouts_.resize(page_sizes.size());
  for (size_t i = 0; i < page_sizes.size(); ++i) {
    if (i != 0) {
      // Add space for bottom separator.
      size_.Enlarge(0, kBottomSeparator);
    }

    const pp::Size& page_size = page_sizes[i];
    pp::Rect page_rect = draw_utils::GetRectForSingleView(page_size, size_);
    page_layouts_[i].outer_rect = page_rect;
    page_layouts_[i].inner_rect = InsetRect(page_rect, kSingleViewInsets);

    draw_utils::ExpandDocumentSize(page_size, &size_);
  }
}

void DocumentLayout::ComputeTwoUpViewLayout(
    const std::vector<pp::Size>& page_sizes) {
  size_ = {GetWidestPageWidth(page_sizes), 0};

  page_layouts_.resize(page_sizes.size());
  for (size_t i = 0; i < page_sizes.size(); ++i) {
    draw_utils::PageInsetSizes page_insets =
        draw_utils::GetPageInsetsForTwoUpView(
            i, page_sizes.size(), kSingleViewInsets, kHorizontalSeparator);
    const pp::Size& page_size = page_sizes[i];

    pp::Rect page_rect;
    if (i % 2 == 0) {
      page_rect = draw_utils::GetLeftRectForTwoUpView(
          page_size, {size_.width(), size_.height()});
    } else {
      page_rect = draw_utils::GetRightRectForTwoUpView(
          page_size, {size_.width(), size_.height()});
      size_.Enlarge(0,
                    std::max(page_size.height(), page_sizes[i - 1].height()));
    }
    page_layouts_[i].outer_rect = page_rect;
    page_layouts_[i].inner_rect = InsetRect(page_rect, page_insets);
  }

  if (page_sizes.size() % 2 == 1) {
    size_.Enlarge(0, page_sizes.back().height());
  }

  size_.set_width(2 * size_.width());
}

}  // namespace chrome_pdf
