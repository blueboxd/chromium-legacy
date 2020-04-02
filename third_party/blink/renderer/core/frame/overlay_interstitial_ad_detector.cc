// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/overlay_interstitial_ad_detector.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {

namespace {

constexpr base::TimeDelta kFireInterval = base::TimeDelta::FromSeconds(1);

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

bool IsInterstitialAd(Element* element) {
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
  if (!last_detection_time_.has_value() ||
      current_time - last_detection_time_.value() >= kFireInterval) {
    IntSize main_frame_size =
        main_frame->View()->GetScrollableArea()->VisibleContentRect().Size();
    HitTestLocation location(DoublePoint(main_frame_size.Width() / 2.0,
                                         main_frame_size.Height() / 2.0));
    HitTestResult result;
    main_frame->ContentLayoutObject()->HitTestNoLifecycleUpdate(location,
                                                                result);
    Element* element = result.InnerElement();

    if (element && IsInterstitialAd(element)) {
      UseCounter::Count(main_frame->GetDocument(),
                        WebFeature::kOverlayInterstitialAd);
      done_detection_ = true;
    }
    last_detection_time_ = current_time;
  }
}

}  // namespace blink
