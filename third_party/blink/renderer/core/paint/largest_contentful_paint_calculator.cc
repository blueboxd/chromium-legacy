// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"

#include "third_party/blink/renderer/core/paint/image_element_timing.h"

namespace blink {

LargestContentfulPaintCalculator::LargestContentfulPaintCalculator(
    WindowPerformance* window_performance)
    : window_performance_(window_performance) {}

void LargestContentfulPaintCalculator::OnLargestImageUpdated(
    const ImageRecord* largest_image) {
  largest_image_.reset();
  if (largest_image) {
    largest_image_ = std::make_unique<ImageRecord>();
    largest_image_->node_id = largest_image->node_id;
    largest_image_->first_size = largest_image->first_size;
    largest_image_->paint_time = largest_image->paint_time;
    largest_image_->cached_image = largest_image->cached_image;
    largest_image_->load_time = largest_image->load_time;
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
    base::WeakPtr<TextRecord> largest_text) {
  largest_text_.reset();
  if (largest_text) {
    largest_text_ = std::make_unique<TextRecord>(
        largest_text->node_id, largest_text->first_size, FloatRect());
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
  if (type == LargestContentType::kImage) {
    const ImageResourceContent* cached_image = largest_image_->cached_image;
    Node* image_node = DOMNodeIds::NodeForId(largest_image_->node_id);

    // |cached_image| is a weak pointer, so it may be null. This can only happen
    // if the image has been removed, which means that the largest image is not
    // up-to-date. This can happen when this method call came from
    // OnLargestTextUpdated(). It is safe to ignore the image in this case: the
    // correct largest content should be identified on the next call to
    // OnLargestImageUpdated().
    // For similar reasons, |image_node| may be null and it is safe to ignore
    // the |largest_image_| content in this case as well.
    if (!cached_image || !image_node)
      return;

    const KURL& url = cached_image->Url();
    auto* document = window_performance_->GetExecutionContext();
    if (!url.ProtocolIsData() &&
        (!document || !Performance::PassesTimingAllowCheck(
                          cached_image->GetResponse(),
                          *document->GetSecurityOrigin(), document))) {
      // Reset the paint time of this image. It cannot be exposed to the
      // webexposed API.
      largest_image_->paint_time = base::TimeTicks();
    }
    const String& image_url =
        url.ProtocolIsData()
            ? url.GetString().Left(ImageElementTiming::kInlineImageMaxChars)
            : url.GetString();
    // Do not expose element attribution from shadow trees.
    Element* image_element =
        image_node->IsInShadowTree() ? nullptr : To<Element>(image_node);
    const AtomicString& image_id =
        image_element ? image_element->GetIdAttribute() : AtomicString();
    window_performance_->OnLargestContentfulPaintUpdated(
        largest_image_->paint_time, largest_image_->first_size,
        largest_image_->load_time, image_id, image_url, image_element);
  } else {
    Node* text_node = DOMNodeIds::NodeForId(largest_text_->node_id);
    // |text_node| could be null and |largest_text_| should be ignored in this
    // case.
    if (!text_node)
      return;

    // Do not expose element attribution from shadow trees.
    Element* text_element =
        text_node->IsInShadowTree() ? nullptr : To<Element>(text_node);
    const AtomicString& text_id =
        text_element ? text_element->GetIdAttribute() : AtomicString();
    window_performance_->OnLargestContentfulPaintUpdated(
        largest_text_->paint_time, largest_text_->first_size, base::TimeTicks(),
        text_id, g_empty_string, text_element);
  }
}

void LargestContentfulPaintCalculator::Trace(Visitor* visitor) {
  visitor->Trace(window_performance_);
}

}  // namespace blink
