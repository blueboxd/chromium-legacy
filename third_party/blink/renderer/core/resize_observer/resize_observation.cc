// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_box_options.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"

namespace blink {

ResizeObservation::ResizeObservation(Element* target,
                                     ResizeObserver* observer,
                                     ResizeObserverBoxOptions observed_box)
    : target_(target),
      observer_(observer),
      observation_size_(0, 0),
      element_size_changed_(true),
      observed_box_(observed_box) {
  DCHECK(target_);
  observer_->ElementSizeChanged();
}

bool ResizeObservation::ObservationSizeOutOfSync() {
  if (!element_size_changed_ || observation_size_ == ComputeTargetSize())
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

  // Don't clear the dirty bit while locked. This allows us to make sure to
  // compare sizes when becoming unlocked.
  if (UNLIKELY(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*target_)))
    return;

  element_size_changed_ = false;
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

      switch (observed_box_) {
        case ResizeObserverBoxOptions::BorderBox:
          return ToLayoutBox(layout_object)->BorderBoxRect().Size();
        case ResizeObserverBoxOptions::ContentBox:
          return ToLayoutBox(layout_object)->ContentSize();
        default:
          NOTREACHED();
      }
    }
  }
  return LayoutSize();
}

void ResizeObservation::ElementSizeChanged() {
  element_size_changed_ = true;
  observer_->ElementSizeChanged();
}

void ResizeObservation::Trace(Visitor* visitor) {
  visitor->Trace(target_);
  visitor->Trace(observer_);
}

}  // namespace blink
