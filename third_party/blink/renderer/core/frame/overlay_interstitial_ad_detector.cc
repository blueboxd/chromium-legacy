// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/overlay_interstitial_ad_detector.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {

namespace {

constexpr base::TimeDelta kFireInterval = base::TimeDelta::FromSeconds(1);
constexpr double kLargeAdSizeToViewportSizeThreshold = 0.1;

bool IsIframeAd(Element* element) {
  HTMLFrameOwnerElement* frame_owner_element =
      DynamicTo<HTMLFrameOwnerElement>(element);
  if (!frame_owner_element)
    return false;

  Frame* frame = frame_owner_element->ContentFrame();
  return frame && frame->IsAdSubframe();
}

bool IsImageAd(Element* element) {
  HTMLImageElement* image_element = DynamicTo<HTMLImageElement>(element);
  if (!image_element)
    return false;

  return image_element->IsAdRelated();
}

// An overlay interstitial element shouldn't move with scrolling and should be
// able to overlap with other contents. So, either:
// 1) one of its container ancestors (including itself) has fixed position.
// 2) <body> or <html> has style="overflow:hidden", and among its container
// ancestors (including itself), the 2nd to the top (where the top should always
// be the <body>) has absolute position.
bool IsImmobileAndCanOverlapWithOtherContent(Element* element) {
  const ComputedStyle* style = nullptr;
  LayoutView* layout_view = element->GetDocument().GetLayoutView();
  LayoutObject* object = element->GetLayoutObject();

  DCHECK_NE(object, layout_view);

  for (; object != layout_view; object = object->Container()) {
    DCHECK(object);
    style = object->Style();
  }

  DCHECK(style);

  // 'style' is now the ComputedStyle for the object whose position depends
  // on the document.
  if (style->HasViewportConstrainedPosition() ||
      style->HasStickyConstrainedPosition()) {
    return true;
  }

  if (style->GetPosition() == EPosition::kAbsolute)
    return !object->StyleRef().ScrollsOverflow();

  return false;
}

bool IsOverlayAdCandidate(Element* element) {
  return (IsIframeAd(element) || IsImageAd(element)) &&
         IsImmobileAndCanOverlapWithOtherContent(element);
}

}  // namespace

void OverlayInterstitialAdDetector::MaybeFireDetection(LocalFrame* main_frame) {
  DCHECK(main_frame);
  DCHECK(main_frame->IsMainFrame());
  if (done_detection_)
    return;

  DCHECK(main_frame->GetDocument());
  DCHECK(main_frame->ContentLayoutObject());

  base::Time current_time = base::Time::Now();

  if (!started_detection_ ||
      current_time - last_detection_time_ >= kFireInterval) {
    IntSize main_frame_size = main_frame->GetMainFrameViewportSize();

    if (started_detection_ &&
        main_frame_size != last_detection_main_frame_size_) {
      // Reset the candidate when the the viewport size has changed. Changing
      // the viewport size could influence the layout and may trick the detector
      // into believing that an element appeared and was dismissed, but what
      // could have happened is that the element no longer covers the center,
      // but still exists (e.g. a sticky ad at the top).
      candidate_id_ = kInvalidDOMNodeId;
    }

    HitTestLocation location(DoublePoint(main_frame_size.Width() / 2.0,
                                         main_frame_size.Height() / 2.0));
    HitTestResult result;
    main_frame->ContentLayoutObject()->HitTestNoLifecycleUpdate(location,
                                                                result);
    started_detection_ = true;

    last_detection_time_ = current_time;
    last_detection_main_frame_size_ = main_frame_size;

    Element* element = result.InnerElement();
    if (!element)
      return;

    DOMNodeId element_id = DOMNodeIds::IdForNode(element);

    bool is_new_element = (element_id != candidate_id_);

    if (is_new_element && candidate_id_ != kInvalidDOMNodeId) {
      // If the main frame scrolling offset hasn't changed since the candidate's
      // appearance, we consider it to be a overlay interstitial; otherwise, we
      // skip that candidate because it could be a parallax/scroller ad.
      if (main_frame->GetMainFrameScrollOffset().Y() ==
          candidate_start_main_frame_scroll_offset_) {
        OnPopupAdDetected(main_frame);
        return;
      }
      last_unqualified_element_id_ = candidate_id_;
      candidate_id_ = kInvalidDOMNodeId;
    }

    if (!is_new_element)
      return;

    if (element_id == last_unqualified_element_id_)
      return;

    if (!element->GetLayoutObject())
      return;

    // Skip considering the overlay for a pop-up candidate if we haven't seen or
    // have just seen the first meaningful paint. If we have just seen the first
    // meaningful paint, however, we would consider future overlays for pop-up
    // candidates.
    if (!main_content_has_loaded_) {
      if (FirstMeaningfulPaintDetector::From(*(main_frame->GetDocument()))
              .SeenFirstMeaningfulPaint()) {
        main_content_has_loaded_ = true;
      }
      last_unqualified_element_id_ = element_id;
      return;
    }

    IntRect overlay_rect =
        element->GetLayoutObject()->AbsoluteBoundingBoxRect();

    bool is_large =
        !overlay_rect.IsEmpty() &&
        (overlay_rect.Size().Area() >
         main_frame_size.Area() * kLargeAdSizeToViewportSizeThreshold);

    bool has_gesture = LocalFrame::HasTransientUserActivation(main_frame);

    if (!has_gesture && is_large && IsOverlayAdCandidate(element)) {
      // If main page is not scrollable, immediately determinine the overlay
      // to be a popup. There's is no need to check any state at the dismissal
      // time.
      if (!main_frame->GetDocument()
               ->GetLayoutView()
               ->HasScrollableOverflowY()) {
        OnPopupAdDetected(main_frame);
        return;
      }
      candidate_id_ = element_id;
      candidate_start_main_frame_scroll_offset_ =
          main_frame->GetMainFrameScrollOffset().Y();
    } else {
      last_unqualified_element_id_ = element_id;
    }
  }
}

void OverlayInterstitialAdDetector::OnPopupAdDetected(LocalFrame* main_frame) {
  UseCounter::Count(main_frame->GetDocument(), WebFeature::kOverlayPopupAd);
  done_detection_ = true;
}

}  // namespace blink
