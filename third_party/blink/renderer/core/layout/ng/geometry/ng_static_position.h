// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGStaticPosition_h
#define NGStaticPosition_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

struct NGPhysicalStaticPosition;

// Represents the static-position of an OOF-positioned descendant, in the
// logical coordinate space.
//
// |offset| is the position of the descandant's |inline_edge|, and |block_edge|.
struct CORE_EXPORT NGLogicalStaticPosition {
  enum InlineEdge { kInlineStart, kInlineEnd };
  enum BlockEdge { kBlockStart, kBlockEnd };

  inline NGPhysicalStaticPosition
  ConvertToPhysical(WritingMode, TextDirection, const PhysicalSize& size) const;

  LogicalOffset offset;
  InlineEdge inline_edge;
  BlockEdge block_edge;
};

// Similar to |NGLogicalStaticPosition| but in the physical coordinate space.
struct CORE_EXPORT NGPhysicalStaticPosition {
  enum HorizontalEdge { kLeft, kRight };
  enum VerticalEdge { kTop, kBottom };

  PhysicalOffset offset;
  HorizontalEdge horizontal_edge;
  VerticalEdge vertical_edge;

  // Left/Right/TopPosition functions map static position to inset of
  // left/right/top edge wrt container space.
  // The function arguments are required to solve the equation:
  // contaner_size = left + margin_left + width + margin_right + right
  LayoutUnit LeftInset(LayoutUnit container_size,
                       LayoutUnit width,
                       LayoutUnit margin_left,
                       LayoutUnit margin_right) const;
  LayoutUnit RightInset(LayoutUnit container_size,
                        LayoutUnit width,
                        LayoutUnit margin_left,
                        LayoutUnit margin_right) const;
  LayoutUnit TopInset(LayoutUnit container_size,
                      LayoutUnit height,
                      LayoutUnit margin_top,
                      LayoutUnit margin_bottom) const;
  LayoutUnit BottomInset(LayoutUnit container_size,
                         LayoutUnit height,
                         LayoutUnit margin_top,
                         LayoutUnit margin_bottom) const;

  LayoutUnit Left() const {
    DCHECK(HasLeft());
    return offset.left;
  }

  LayoutUnit Right() const {
    DCHECK(!HasLeft());
    return offset.left;
  }

  LayoutUnit Top() const {
    DCHECK(HasTop());
    return offset.top;
  }

  LayoutUnit Bottom() const {
    DCHECK(!HasTop());
    return offset.top;
  }

  bool HasLeft() const { return horizontal_edge == kLeft; }
  bool HasTop() const { return vertical_edge == kTop; }

  NGLogicalStaticPosition ConvertToLogical(WritingMode writing_mode,
                                           TextDirection direction,
                                           const PhysicalSize& size) const {
    LogicalOffset logical_offset =
        offset.ConvertToLogical(writing_mode, direction, /* outer_size */ size,
                                /* inner_size */ PhysicalSize());

    NGLogicalStaticPosition::InlineEdge inline_edge;
    NGLogicalStaticPosition::BlockEdge block_edge;

    switch (writing_mode) {
      case WritingMode::kHorizontalTb:
        inline_edge = ((horizontal_edge == kLeft) == IsLtr(direction))
                          ? NGLogicalStaticPosition::InlineEdge::kInlineStart
                          : NGLogicalStaticPosition::InlineEdge::kInlineEnd;
        block_edge = (vertical_edge == kTop)
                         ? NGLogicalStaticPosition::BlockEdge::kBlockStart
                         : NGLogicalStaticPosition::BlockEdge::kBlockEnd;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
        inline_edge = ((vertical_edge == kTop) == IsLtr(direction))
                          ? NGLogicalStaticPosition::InlineEdge::kInlineStart
                          : NGLogicalStaticPosition::InlineEdge::kInlineEnd;
        block_edge = (horizontal_edge == kRight)
                         ? NGLogicalStaticPosition::BlockEdge::kBlockStart
                         : NGLogicalStaticPosition::BlockEdge::kBlockEnd;
        break;
      case WritingMode::kVerticalLr:
        inline_edge = ((vertical_edge == kTop) == IsLtr(direction))
                          ? NGLogicalStaticPosition::InlineEdge::kInlineStart
                          : NGLogicalStaticPosition::InlineEdge::kInlineEnd;
        block_edge = (horizontal_edge == kLeft)
                         ? NGLogicalStaticPosition::BlockEdge::kBlockStart
                         : NGLogicalStaticPosition::BlockEdge::kBlockEnd;
        break;
      case WritingMode::kSidewaysLr:
        inline_edge = ((vertical_edge == kBottom) == IsLtr(direction))
                          ? NGLogicalStaticPosition::InlineEdge::kInlineStart
                          : NGLogicalStaticPosition::InlineEdge::kInlineEnd;
        block_edge = (horizontal_edge == kLeft)
                         ? NGLogicalStaticPosition::BlockEdge::kBlockStart
                         : NGLogicalStaticPosition::BlockEdge::kBlockEnd;
        break;
    }

    return {logical_offset, inline_edge, block_edge};
  }
};

inline NGPhysicalStaticPosition NGLogicalStaticPosition::ConvertToPhysical(
    WritingMode writing_mode,
    TextDirection direction,
    const PhysicalSize& size) const {
  PhysicalOffset physical_offset =
      offset.ConvertToPhysical(writing_mode, direction, /* outer_size */ size,
                               /* inner_size */ PhysicalSize());

  NGPhysicalStaticPosition::HorizontalEdge horizontal_edge;
  NGPhysicalStaticPosition::VerticalEdge vertical_edge;

  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      horizontal_edge = ((inline_edge == kInlineStart) == IsLtr(direction))
                            ? NGPhysicalStaticPosition::HorizontalEdge::kLeft
                            : NGPhysicalStaticPosition::HorizontalEdge::kRight;
      vertical_edge = (block_edge == kBlockStart)
                          ? NGPhysicalStaticPosition::VerticalEdge::kTop
                          : NGPhysicalStaticPosition::VerticalEdge::kBottom;
      break;
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      horizontal_edge = (block_edge == kBlockEnd)
                            ? NGPhysicalStaticPosition::HorizontalEdge::kLeft
                            : NGPhysicalStaticPosition::HorizontalEdge::kRight;
      vertical_edge = ((inline_edge == kInlineStart) == IsLtr(direction))
                          ? NGPhysicalStaticPosition::VerticalEdge::kTop
                          : NGPhysicalStaticPosition::VerticalEdge::kBottom;
      break;
    case WritingMode::kVerticalLr:
      horizontal_edge = (block_edge == kBlockStart)
                            ? NGPhysicalStaticPosition::HorizontalEdge::kLeft
                            : NGPhysicalStaticPosition::HorizontalEdge::kRight;
      vertical_edge = ((inline_edge == kInlineStart) == IsLtr(direction))
                          ? NGPhysicalStaticPosition::VerticalEdge::kTop
                          : NGPhysicalStaticPosition::VerticalEdge::kBottom;
      break;
    case WritingMode::kSidewaysLr:
      horizontal_edge = (block_edge == kBlockStart)
                            ? NGPhysicalStaticPosition::HorizontalEdge::kLeft
                            : NGPhysicalStaticPosition::HorizontalEdge::kRight;
      vertical_edge = ((inline_edge == kInlineEnd) == IsLtr(direction))
                          ? NGPhysicalStaticPosition::VerticalEdge::kTop
                          : NGPhysicalStaticPosition::VerticalEdge::kBottom;
      break;
  }

  return {physical_offset, horizontal_edge, vertical_edge};
}

}  // namespace blink

#endif  // NGStaticPosition_h
