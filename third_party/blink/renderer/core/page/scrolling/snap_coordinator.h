// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SNAP_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SNAP_COORDINATOR_H_

#include "base/macros.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class LayoutBox;

// Snap Coordinator keeps track of snap containers and all of their associated
// snap areas. It also contains the logic to generate the list of valid snap
// positions for a given snap container.
//
// Snap container:
//   An scroll container that has 'scroll-snap-type' value other
//   than 'none'.
// Snap area:
//   A snap container's descendant that contributes snap positions. An element
//   only contributes snap positions to its nearest ancestor (on the element’s
//   containing block chain) scroll container.
//
// For more information see spec: https://drafts.csswg.org/css-snappoints/
class CORE_EXPORT SnapCoordinator final
    : public GarbageCollected<SnapCoordinator> {
 public:
  explicit SnapCoordinator();
  ~SnapCoordinator();
  void Trace(blink::Visitor* visitor) {}

  void SnapContainerDidChange(LayoutBox&, bool is_removed);
  void SnapAreaDidChange(LayoutBox&, cc::ScrollSnapAlign);

  // Returns the SnapContainerData if the snap container has one.
  base::Optional<cc::SnapContainerData> GetSnapContainerData(
      const LayoutBox& snap_container) const;

  // Calculate the SnapAreaData for the specific snap area in its snap
  // container.
  cc::SnapAreaData CalculateSnapAreaData(const LayoutBox& snap_area,
                                         const LayoutBox& snap_container);

  // Called by LocalFrameView::PerformPostLayoutTasks(), so that the snap data
  // are updated whenever a layout happens.
  void UpdateAllSnapContainerData();

  // SnapAtCurrentPosition(), SnapForEndPosition(), SnapForDirection(), and
  // SnapForEndAndDirection() return true if snapping was performed, and false
  // otherwise. Note that this does not necessarily mean that any scrolling was
  // performed as a result e.g., if we are already at the snap point.
  //
  // SnapAtCurrentPosition() calls SnapForEndPosition() with the current
  // scroll position.
  bool SnapAtCurrentPosition(const LayoutBox& snap_container,
                             bool scrolled_x,
                             bool scrolled_y) const;
  bool SnapForEndPosition(const LayoutBox& snap_container,
                          const FloatPoint& end_position,
                          bool scrolled_x,
                          bool scrolled_y) const;
  bool SnapForDirection(const LayoutBox& snap_container,
                        const ScrollOffset& delta) const;
  bool SnapForEndAndDirection(const LayoutBox& snap_container,
                              const ScrollOffset& delta) const;

  base::Optional<FloatPoint> GetSnapPosition(
      const LayoutBox& snap_container,
      const cc::SnapSelectionStrategy& strategy) const;

#ifndef NDEBUG
  void ShowSnapAreaMap();
  void ShowSnapAreasFor(const LayoutBox*);
  void ShowSnapDataFor(const LayoutBox*);
#endif

 private:
  friend class SnapCoordinatorTest;

  // Returns true if a snap point was found.
  bool PerformSnapping(const LayoutBox& snap_container,
                       const cc::SnapSelectionStrategy& strategy) const;

  void UpdateSnapContainerData(LayoutBox&);

  HashMap<LayoutBox*, cc::SnapContainerData> snap_container_map_;
  DISALLOW_COPY_AND_ASSIGN(SnapCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SNAP_COORDINATOR_H_
