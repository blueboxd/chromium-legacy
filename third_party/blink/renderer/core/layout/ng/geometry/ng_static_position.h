// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_STATIC_POSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_STATIC_POSITION_H_

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
  enum InlineEdge { kInlineStart, kInlineCenter, kInlineEnd };
  enum BlockEdge { kBlockStart, kBlockCenter, kBlockEnd };

  inline NGPhysicalStaticPosition
  ConvertToPhysical(WritingMode, TextDirection, const PhysicalSize& size) const;

  LogicalOffset offset;
  InlineEdge inline_edge;
  BlockEdge block_edge;
};

// Similar to |NGLogicalStaticPosition| but in the physical coordinate space.
struct CORE_EXPORT NGPhysicalStaticPosition {
  enum HorizontalEdge { kLeft, kHorizontalCenter, kRight };
  enum VerticalEdge { kTop, kVerticalCenter, kBottom };

  PhysicalOffset offset;
  HorizontalEdge horizontal_edge;
  VerticalEdge vertical_edge;

  NGLogicalStaticPosition ConvertToLogical(WritingMode writing_mode,
                                           TextDirection direction,
                                           const PhysicalSize& size) const {
    LogicalOffset logical_offset =
        offset.ConvertToLogical(writing_mode, direction, /* outer_size */ size,
                                /* inner_size */ PhysicalSize());

    using InlineEdge = NGLogicalStaticPosition::InlineEdge;
    using BlockEdge = NGLogicalStaticPosition::BlockEdge;

    InlineEdge inline_edge;
    BlockEdge block_edge;

    switch (writing_mode) {
      case WritingMode::kHorizontalTb:
        inline_edge = ((horizontal_edge == kLeft) == IsLtr(direction))
                          ? InlineEdge::kInlineStart
                          : InlineEdge::kInlineEnd;
        block_edge = (vertical_edge == kTop) ? BlockEdge::kBlockStart
                                             : BlockEdge::kBlockEnd;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
        inline_edge = ((vertical_edge == kTop) == IsLtr(direction))
                          ? InlineEdge::kInlineStart
                          : InlineEdge::kInlineEnd;
        block_edge = (horizontal_edge == kRight) ? BlockEdge::kBlockStart
                                                 : BlockEdge::kBlockEnd;
        break;
      case WritingMode::kVerticalLr:
        inline_edge = ((vertical_edge == kTop) == IsLtr(direction))
                          ? InlineEdge::kInlineStart
                          : InlineEdge::kInlineEnd;
        block_edge = (horizontal_edge == kLeft) ? BlockEdge::kBlockStart
                                                : BlockEdge::kBlockEnd;
        break;
      case WritingMode::kSidewaysLr:
        inline_edge = ((vertical_edge == kBottom) == IsLtr(direction))
                          ? InlineEdge::kInlineStart
                          : InlineEdge::kInlineEnd;
        block_edge = (horizontal_edge == kLeft) ? BlockEdge::kBlockStart
                                                : BlockEdge::kBlockEnd;
        break;
    }

    // Adjust for uncommon "center" static-positions.
    switch (writing_mode) {
      case WritingMode::kHorizontalTb:
        inline_edge = (horizontal_edge == kHorizontalCenter)
                          ? InlineEdge::kInlineCenter
                          : inline_edge;
        block_edge = (vertical_edge == kVerticalCenter)
                         ? BlockEdge::kBlockCenter
                         : block_edge;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
      case WritingMode::kVerticalLr:
      case WritingMode::kSidewaysLr:
        inline_edge = (vertical_edge == kVerticalCenter)
                          ? InlineEdge::kInlineCenter
                          : inline_edge;
        block_edge = (horizontal_edge == kHorizontalCenter)
                         ? BlockEdge::kBlockCenter
                         : block_edge;
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

  using HorizontalEdge = NGPhysicalStaticPosition::HorizontalEdge;
  using VerticalEdge = NGPhysicalStaticPosition::VerticalEdge;

  HorizontalEdge horizontal_edge;
  VerticalEdge vertical_edge;

  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      horizontal_edge = ((inline_edge == kInlineStart) == IsLtr(direction))
                            ? HorizontalEdge::kLeft
                            : HorizontalEdge::kRight;
      vertical_edge = (block_edge == kBlockStart) ? VerticalEdge::kTop
                                                  : VerticalEdge::kBottom;
      break;
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      horizontal_edge = (block_edge == kBlockEnd) ? HorizontalEdge::kLeft
                                                  : HorizontalEdge::kRight;
      vertical_edge = ((inline_edge == kInlineStart) == IsLtr(direction))
                          ? VerticalEdge::kTop
                          : VerticalEdge::kBottom;
      break;
    case WritingMode::kVerticalLr:
      horizontal_edge = (block_edge == kBlockStart) ? HorizontalEdge::kLeft
                                                    : HorizontalEdge::kRight;
      vertical_edge = ((inline_edge == kInlineStart) == IsLtr(direction))
                          ? VerticalEdge::kTop
                          : VerticalEdge::kBottom;
      break;
    case WritingMode::kSidewaysLr:
      horizontal_edge = (block_edge == kBlockStart) ? HorizontalEdge::kLeft
                                                    : HorizontalEdge::kRight;
      vertical_edge = ((inline_edge == kInlineEnd) == IsLtr(direction))
                          ? VerticalEdge::kTop
                          : VerticalEdge::kBottom;
      break;
  }

  // Adjust for uncommon "center" static-positions.
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      horizontal_edge = (inline_edge == kInlineCenter)
                            ? HorizontalEdge::kHorizontalCenter
                            : horizontal_edge;
      vertical_edge = (block_edge == kBlockCenter)
                          ? VerticalEdge::kVerticalCenter
                          : vertical_edge;
      break;
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysLr:
      horizontal_edge = (block_edge == kBlockCenter)
                            ? HorizontalEdge::kHorizontalCenter
                            : horizontal_edge;
      vertical_edge = (inline_edge == kInlineCenter)
                          ? VerticalEdge::kVerticalCenter
                          : vertical_edge;
      break;
  }

  return {physical_offset, horizontal_edge, vertical_edge};
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GEOMETRY_NG_STATIC_POSITION_H_
