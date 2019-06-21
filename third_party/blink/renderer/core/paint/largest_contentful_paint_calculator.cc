// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"

namespace blink {

LargestContentfulPaintCalculator::LargestContentfulPaintCalculator(
    WindowPerformance* window_performance)
    : window_performance_(window_performance) {}

void LargestContentfulPaintCalculator::OnLargestImageUpdated(
    const ImageRecord* largest_image) {
  largest_image_.reset();
  if (largest_image) {
    largest_image_ = std::make_unique<ImageRecord>();
    largest_image_->first_size = largest_image->first_size;
    largest_image_->paint_time = largest_image->paint_time;
  }

  if (LargestImageSize() > LargestTextSize()) {
    // The new largest image is the largest content, so report it as the LCP.
    OnLargestContentfulPaintUpdated(LargestContentType::kImage);
  } else if (largest_text_ && last_type_ == LargestContentType::kImage) {
    // The text is at least as large as the new image. Because the last reported
    // content type was image, this means that the largest image is now smaller
    // and the largest text now needs to be reported as the LCP.
    OnLargestContentfulPaintUpdated(LargestContentType::kText);
  }
}

void LargestContentfulPaintCalculator::OnLargestTextUpdated(
    const TextRecord* largest_text) {
  largest_text_.reset();
  if (largest_text) {
    largest_text_ = std::make_unique<TextRecord>(
        kInvalidDOMNodeId, largest_text->first_size, FloatRect());
    largest_text_->paint_time = largest_text->paint_time;
  }

  if (LargestTextSize() > LargestImageSize()) {
    // The new largest text is the largest content, so report it as the LCP.
    OnLargestContentfulPaintUpdated(LargestContentType::kText);
  } else if (largest_image_ && last_type_ == LargestContentType::kText) {
    // The image is at least as large as the new text. Because the last reported
    // content type was text, this means that the largest text is now smaller
    // and the largest image now needs to be reported as the LCP.
    OnLargestContentfulPaintUpdated(LargestContentType::kImage);
  }
}

void LargestContentfulPaintCalculator::OnLargestContentfulPaintUpdated(
    LargestContentType type) {
  DCHECK(window_performance_);
  DCHECK(type != LargestContentType::kUnknown);
  last_type_ = type;
  // TODO(crbug.com/965505): finish implementation, including adding more
  // attribution and doing proper cross-origin checks for images.
  if (type == LargestContentType::kImage) {
    window_performance_->OnLargestContentfulPaintUpdated(
        largest_image_->paint_time, largest_image_->first_size);
  } else {
    window_performance_->OnLargestContentfulPaintUpdated(
        largest_text_->paint_time, largest_text_->first_size);
  }
}

void LargestContentfulPaintCalculator::Trace(Visitor* visitor) {
  visitor->Trace(window_performance_);
}

}  // namespace blink
