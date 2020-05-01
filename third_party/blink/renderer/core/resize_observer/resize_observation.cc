// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_box_options.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

ResizeObservation::ResizeObservation(Element* target,
                                     ResizeObserver* observer,
                                     ResizeObserverBoxOptions observed_box)
    : target_(target),
      observer_(observer),
      observation_size_(0, 0),
      observed_box_(observed_box) {
  DCHECK(target_);
}

bool ResizeObservation::ObservationSizeOutOfSync() {
  if (observation_size_ == ComputeTargetSize())
    return false;

  // Skip resize observations on locked elements.
  if (UNLIKELY(target_ && DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(
                              *target_))) {
    return false;
  }

  return true;
}

void ResizeObservation::SetObservationSize(const LayoutSize& observation_size) {
  observation_size_ = observation_size;
}

size_t ResizeObservation::TargetDepth() {
  unsigned depth = 0;
  for (Element* parent = target_; parent; parent = parent->parentElement())
    ++depth;
  return depth;
}

LayoutSize ResizeObservation::ComputeTargetSize() const {
  if (target_) {
    if (LayoutObject* layout_object = target_->GetLayoutObject()) {
      // https://drafts.csswg.org/resize-observer/#calculate-box-size states
      // that the bounding box should be used for SVGGraphicsElements regardless
      // of the observed box.
      if (auto* svg_graphics_element =
              DynamicTo<SVGGraphicsElement>(target_.Get())) {
        return LayoutSize(svg_graphics_element->GetBBox().Size());
      }
      if (!layout_object->IsBox())
        return LayoutSize();

      if (LayoutBox* layout_box = ToLayoutBox(layout_object)) {
        const ComputedStyle& style = layout_object->StyleRef();
        switch (observed_box_) {
          case ResizeObserverBoxOptions::BorderBox:
            return LayoutSize(AdjustForAbsoluteZoom::AdjustLayoutUnit(
                                  layout_box->LogicalWidth(), style),
                              AdjustForAbsoluteZoom::AdjustLayoutUnit(
                                  layout_box->LogicalHeight(), style));
          case ResizeObserverBoxOptions::ContentBox:
            return LayoutSize(AdjustForAbsoluteZoom::AdjustLayoutUnit(
                                  layout_box->ContentLogicalWidth(), style),
                              AdjustForAbsoluteZoom::AdjustLayoutUnit(
                                  layout_box->ContentLogicalHeight(), style));
          case ResizeObserverBoxOptions::DevicePixelContentBox: {
            LayoutSize paint_offset =
                layout_object->FirstFragment().PaintOffset().ToLayoutSize();

            LayoutSize device_pixel_content_box_size(
                SnapSizeToPixel(layout_box->ContentLogicalWidth(),
                                style.IsHorizontalWritingMode()
                                    ? paint_offset.Width()
                                    : paint_offset.Height()),
                SnapSizeToPixel(layout_box->ContentLogicalHeight(),
                                style.IsHorizontalWritingMode()
                                    ? paint_offset.Height()
                                    : paint_offset.Width()));

            // Get Device Scale Factor for cases where use-zoom-for-dsf is
            // disabled. This is 1 if use-zoom-for-dsf is enabled.
            float device_scale_factor = layout_object->GetFrame()
                                            ->GetPage()
                                            ->DeviceScaleFactorDeprecated();
            device_pixel_content_box_size.Scale(device_scale_factor);

            return device_pixel_content_box_size;
          }
          default:
            NOTREACHED();
        }
      }
    }
  }
  return LayoutSize();
}

void ResizeObservation::Trace(Visitor* visitor) {
  visitor->Trace(target_);
  visitor->Trace(observer_);
}

}  // namespace blink
