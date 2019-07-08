// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"

#include <algorithm>
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

// Tables need special handling, unfortunately. The code in this file assumes
// that if an element has a height or width specified, that's what its final
// height/width will be. Tables don't follow this pattern though; they treat
// specified height/width as a second min-height or min-width.
bool IsTable(const ComputedStyle& style) {
  return style.Display() == EDisplay::kTable ||
         style.Display() == EDisplay::kInlineTable;
}

inline Length TableAwareHeight(const ComputedStyle& style) {
  if (IsTable(style))
    return Length::Auto();
  return style.Height();
}

inline Length TableAwareWidth(const ComputedStyle& style) {
  if (IsTable(style))
    return Length::Auto();
  return style.Width();
}

bool AbsoluteHorizontalNeedsEstimate(const ComputedStyle& style) {
  const Length& width = TableAwareWidth(style);
  return width.IsIntrinsic() || style.MinWidth().IsIntrinsic() ||
         style.MaxWidth().IsIntrinsic() ||
         (width.IsAuto() && (style.Left().IsAuto() || style.Right().IsAuto()));
}

bool AbsoluteVerticalNeedsEstimate(const ComputedStyle& style) {
  const Length& height = TableAwareHeight(style);
  return height.IsIntrinsic() || style.MinHeight().IsIntrinsic() ||
         style.MaxHeight().IsIntrinsic() ||
         (height.IsAuto() && (style.Top().IsAuto() || style.Bottom().IsAuto()));
}

// Dominant side:
// htb ltr => top left
// htb rtl => top right
// vlr ltr => top left
// vlr rtl => bottom left
// vrl ltr => top right
// vrl rtl => bottom right
bool IsLeftDominant(const WritingMode container_writing_mode,
                    const TextDirection container_direction) {
  return (container_writing_mode != WritingMode::kVerticalRl) &&
         !(container_writing_mode == WritingMode::kHorizontalTb &&
           container_direction == TextDirection::kRtl);
}

bool IsTopDominant(const WritingMode container_writing_mode,
                   const TextDirection container_direction) {
  return (container_writing_mode == WritingMode::kHorizontalTb) ||
         (container_direction != TextDirection::kRtl);
}

LayoutUnit ResolveMinWidth(const NGConstraintSpace& space,
                           const ComputedStyle& style,
                           const NGBoxStrut& border_padding,
                           const base::Optional<MinMaxSize>& child_minmax) {
  const Length& min_width = style.MinWidth();
  if (space.GetWritingMode() == WritingMode::kHorizontalTb) {
    LayoutUnit resolved_min_width =
        ResolveMinInlineLength(space, style, border_padding, child_minmax,
                               min_width, LengthResolvePhase::kLayout);
    if (!IsTable(style))
      return resolved_min_width;
    Length table_width = style.Width();
    if (table_width.IsAuto())
      return resolved_min_width;
    LayoutUnit resolved_width = ResolveMainInlineLength(
        space, style, border_padding, child_minmax, table_width);
    return std::max(resolved_min_width, resolved_width);
  }
  LayoutUnit computed_width =
      child_minmax.has_value() ? child_minmax->max_size : LayoutUnit();
  return ResolveMinBlockLength(space, style, border_padding, min_width,
                               computed_width, LengthResolvePhase::kLayout);
}

LayoutUnit ResolveMaxWidth(const NGConstraintSpace& space,
                           const ComputedStyle& style,
                           const NGBoxStrut& border_padding,
                           const base::Optional<MinMaxSize>& child_minmax,
                           const Length& width) {
  if (space.GetWritingMode() == WritingMode::kHorizontalTb) {
    return ResolveMaxInlineLength(space, style, border_padding, child_minmax,
                                  width, LengthResolvePhase::kLayout);
  }
  LayoutUnit computed_width =
      child_minmax.has_value() ? child_minmax->max_size : LayoutUnit();
  return ResolveMaxBlockLength(space, style, border_padding, width,
                               computed_width, LengthResolvePhase::kLayout);
}

LayoutUnit ResolveMainWidth(const NGConstraintSpace& space,
                            const ComputedStyle& style,
                            const NGBoxStrut& border_padding,
                            const base::Optional<MinMaxSize>& child_minmax,
                            const Length& width) {
  if (space.GetWritingMode() == WritingMode::kHorizontalTb) {
    return ResolveMainInlineLength(space, style, border_padding, child_minmax,
                                   width);
  }
  LayoutUnit computed_width =
      child_minmax.has_value() ? child_minmax->max_size : LayoutUnit();
  return ResolveMainBlockLength(space, style, border_padding, width,
                                computed_width, LengthResolvePhase::kLayout);
}

LayoutUnit ResolveMinHeight(const NGConstraintSpace& space,
                            const ComputedStyle& style,
                            const NGBoxStrut& border_padding,
                            const base::Optional<MinMaxSize>& child_minmax) {
  const Length& min_height = style.MinHeight();
  if (space.GetWritingMode() != WritingMode::kHorizontalTb) {
    LayoutUnit resolved_min_height =
        ResolveMinInlineLength(space, style, border_padding, child_minmax,
                               min_height, LengthResolvePhase::kLayout);
    if (!IsTable(style))
      return resolved_min_height;
    LayoutUnit resolved_height = ResolveMainInlineLength(
        space, style, border_padding, child_minmax, style.Height());
    return std::max(resolved_min_height, resolved_height);
  }
  LayoutUnit computed_height =
      child_minmax.has_value() ? child_minmax->max_size : LayoutUnit();
  return ResolveMinBlockLength(space, style, border_padding, min_height,
                               computed_height, LengthResolvePhase::kLayout);
}

LayoutUnit ResolveMaxHeight(const NGConstraintSpace& space,
                            const ComputedStyle& style,
                            const NGBoxStrut& border_padding,
                            const base::Optional<MinMaxSize>& child_minmax,
                            const Length& height) {
  if (space.GetWritingMode() != WritingMode::kHorizontalTb) {
    return ResolveMaxInlineLength(space, style, border_padding, child_minmax,
                                  height, LengthResolvePhase::kLayout);
  }
  LayoutUnit computed_height =
      child_minmax.has_value() ? child_minmax->max_size : LayoutUnit();
  return ResolveMaxBlockLength(space, style, border_padding, height,
                               computed_height, LengthResolvePhase::kLayout);
}

LayoutUnit ResolveMainHeight(const NGConstraintSpace& space,
                             const ComputedStyle& style,
                             const NGBoxStrut& border_padding,
                             const base::Optional<MinMaxSize>& child_minmax,
                             const Length& height) {
  if (space.GetWritingMode() != WritingMode::kHorizontalTb) {
    return ResolveMainInlineLength(space, style, border_padding, child_minmax,
                                   height);
  }
  LayoutUnit computed_height =
      child_minmax.has_value() ? child_minmax->max_size : LayoutUnit();
  return ResolveMainBlockLength(space, style, border_padding, height,
                                computed_height, LengthResolvePhase::kLayout);
}

inline LayoutUnit ResolveMargin(const NGConstraintSpace& space,
                                const Length& length) {
  DCHECK(!length.IsAuto());
  return MinimumValueForLength(
      length, space.PercentageResolutionInlineSizeForParentWritingMode());
}

// Available size can is maximum length Element can have without overflowing
// container bounds. The position of Element's edges will determine
// how much space there is available.
LayoutUnit ComputeAvailableWidth(
    LayoutUnit container_width,
    const base::Optional<LayoutUnit>& left,
    const base::Optional<LayoutUnit>& right,
    const base::Optional<LayoutUnit>& margin_left,
    const base::Optional<LayoutUnit>& margin_right,
    const NGPhysicalStaticPosition& static_position) {
  LayoutUnit available_width = container_width;
  DCHECK(!left || !right);
  if (!left && !right) {
    if (static_position.HasLeft())
      available_width -= static_position.Left();
    else
      available_width = static_position.Right();
  } else if (!right) {
    available_width -= *left;
  } else {  // !left
    available_width -= *right;
  }
  LayoutUnit margins = (margin_left ? *margin_left : LayoutUnit()) +
                       (margin_right ? *margin_right : LayoutUnit());
  return (available_width - margins).ClampNegativeToZero();
}

LayoutUnit ComputeAvailableHeight(
    LayoutUnit container_height,
    const base::Optional<LayoutUnit>& top,
    const base::Optional<LayoutUnit>& bottom,
    const base::Optional<LayoutUnit>& margin_top,
    const base::Optional<LayoutUnit>& margin_bottom,
    const NGPhysicalStaticPosition& static_position) {
  LayoutUnit available_height = container_height;
  DCHECK(!top || !bottom);
  if (!top && !bottom) {
    if (static_position.HasTop())
      available_height -= static_position.Top();
    else
      available_height = static_position.Bottom();
  } else if (!bottom) {
    available_height -= *top;
  } else {  // !top
    available_height -= *bottom;
  }
  LayoutUnit margins = (margin_top ? *margin_top : LayoutUnit()) +
                       (margin_bottom ? *margin_bottom : LayoutUnit());
  return (available_height - margins).ClampNegativeToZero();
}

// Implement absolute horizontal size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
void ComputeAbsoluteHorizontal(const NGConstraintSpace& space,
                               const ComputedStyle& style,
                               const NGBoxStrut& border_padding,
                               const base::Optional<LayoutUnit>& incoming_width,
                               const NGPhysicalStaticPosition& static_position,
                               const base::Optional<MinMaxSize>& child_minmax,
                               const WritingMode container_writing_mode,
                               const TextDirection container_direction,
                               NGAbsolutePhysicalPosition* position) {
  LayoutUnit percentage_width =
      LIKELY(space.GetWritingMode() == WritingMode::kHorizontalTb)
          ? space.PercentageResolutionInlineSize()
          : space.PercentageResolutionBlockSize();

  base::Optional<LayoutUnit> margin_left;
  if (!style.MarginLeft().IsAuto())
    margin_left = ResolveMargin(space, style.MarginLeft());
  base::Optional<LayoutUnit> margin_right;
  if (!style.MarginRight().IsAuto())
    margin_right = ResolveMargin(space, style.MarginRight());
  base::Optional<LayoutUnit> left;
  if (!style.Left().IsAuto())
    left = MinimumValueForLength(style.Left(), percentage_width);
  base::Optional<LayoutUnit> right;
  if (!style.Right().IsAuto())
    right = MinimumValueForLength(style.Right(), percentage_width);
  base::Optional<LayoutUnit> width = incoming_width;
  PhysicalSize container_size =
      ToPhysicalSize(space.AvailableSize(), space.GetWritingMode());
  DCHECK_NE(container_size.width, kIndefiniteSize);

  // Solving the equation:
  // left + marginLeft + width + marginRight + right  = container width
  if (!left && !right && !width) {
    // Standard: "If all three of left, width, and right are auto:"
    if (!margin_left)
      margin_left = LayoutUnit();
    if (!margin_right)
      margin_right = LayoutUnit();
    DCHECK(child_minmax.has_value());

    width = child_minmax->ShrinkToFit(
        ComputeAvailableWidth(container_size.width, left, right, margin_left,
                              margin_right, static_position));
    if (IsLeftDominant(container_writing_mode, container_direction)) {
      left = static_position.LeftInset(container_size.width, *width,
                                       *margin_left, *margin_right);
    } else {
      right = static_position.RightInset(container_size.width, *width,
                                         *margin_left, *margin_right);
    }
  } else if (left && right && width) {
    // Standard: "If left, right, and width are not auto:"
    // Compute margins.
    LayoutUnit margin_space = container_size.width - *left - *right - *width;
    // When both margins are auto.
    if (!margin_left && !margin_right) {
      if (margin_space > 0) {
        margin_left = margin_space / 2;
        margin_right = margin_space - *margin_left;
      } else {
        // Margins are negative.
        if (IsLeftDominant(container_writing_mode, container_direction)) {
          margin_left = LayoutUnit();
          margin_right = margin_space;
        } else {
          margin_right = LayoutUnit();
          margin_left = margin_space;
        }
      }
    } else if (!margin_left) {
      margin_left = margin_space - *margin_right;
    } else if (!margin_right) {
      margin_right = margin_space - *margin_left;
    } else {
      // Are values overconstrained?
      LayoutUnit margin_extra = margin_space - *margin_left - *margin_right;
      if (margin_extra) {
        // Relax the end.
        if (IsLeftDominant(container_writing_mode, container_direction))
          right = *right + margin_extra;
        else
          left = *left + margin_extra;
      }
    }
  }

  // Set unknown margins.
  if (!margin_left)
    margin_left = LayoutUnit();
  if (!margin_right)
    margin_right = LayoutUnit();

  // Rules 1 through 3, 2 out of 3 are unknown.
  if (!left && !width) {
    // Rule 1: left/width are unknown.
    DCHECK(right.has_value());
    DCHECK(child_minmax.has_value());
    width = child_minmax->ShrinkToFit(
        ComputeAvailableWidth(container_size.width, left, right, margin_left,
                              margin_right, static_position));
  } else if (!left && !right) {
    // Rule 2.
    DCHECK(width.has_value());
    if (IsLeftDominant(container_writing_mode, container_direction))
      left = static_position.LeftInset(container_size.width, *width,
                                       *margin_left, *margin_right);
    else
      right = static_position.RightInset(container_size.width, *width,
                                         *margin_left, *margin_right);
  } else if (!width && !right) {
    // Rule 3.
    DCHECK(child_minmax.has_value());
    width = child_minmax->ShrinkToFit(
        ComputeAvailableWidth(container_size.width, left, right, margin_left,
                              margin_right, static_position));
  }

  // Rules 4 through 6, 1 out of 3 are unknown.
  if (!left) {
    left =
        container_size.width - *width - *right - *margin_left - *margin_right;
  } else if (!right) {
    right =
        container_size.width - *width - *left - *margin_left - *margin_right;
  } else if (!width) {
    width =
        container_size.width - *left - *right - *margin_left - *margin_right;
  }

#if DCHECK_IS_ON()
  // The DCHECK is useful, but only holds true when not saturated.
  if (!(left->MightBeSaturated() || right->MightBeSaturated() ||
        width->MightBeSaturated() || margin_left->MightBeSaturated() ||
        margin_right->MightBeSaturated() ||
        container_size.width.MightBeSaturated())) {
    DCHECK_EQ(container_size.width,
              *left + *right + *margin_left + *margin_right + *width);
  }
#endif  // #if DCHECK_IS_ON()

  // If calculated width is outside of min/max constraints,
  // rerun the algorithm with constrained width.
  LayoutUnit min = ResolveMinWidth(space, style, border_padding, child_minmax);
  LayoutUnit max = ResolveMaxWidth(space, style, border_padding, child_minmax,
                                   style.MaxWidth());
  LayoutUnit constrained_width = ConstrainByMinMax(*width, min, max);
  if (width != constrained_width) {
    width = constrained_width;
    // Because this function only changes "width" when it's not already
    // set, it is safe to recursively call ourselves here because on the
    // second call it is guaranteed to be within min..max.
    ComputeAbsoluteHorizontal(
        space, style, border_padding, width, static_position, child_minmax,
        container_writing_mode, container_direction, position);
    return;
  }

  // Negative widths are not allowed.
  LayoutUnit horizontal_border_padding =
      border_padding
          .ConvertToPhysical(space.GetWritingMode(), space.Direction())
          .HorizontalSum();
  width = std::max(*width, horizontal_border_padding);

  position->inset.left = *left + *margin_left;
  position->inset.right = *right + *margin_right;
  position->margins.left = *margin_left;
  position->margins.right = *margin_right;
  position->size.width = *width;
}

// Implements absolute vertical size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-height
void ComputeAbsoluteVertical(const NGConstraintSpace& space,
                             const ComputedStyle& style,
                             const NGBoxStrut& border_padding,
                             const base::Optional<LayoutUnit>& incoming_height,
                             const NGPhysicalStaticPosition& static_position,
                             const base::Optional<MinMaxSize>& child_minmax,
                             const WritingMode container_writing_mode,
                             const TextDirection container_direction,
                             NGAbsolutePhysicalPosition* position) {
  LayoutUnit percentage_height =
      LIKELY(space.GetWritingMode() == WritingMode::kHorizontalTb)
          ? space.PercentageResolutionBlockSize()
          : space.PercentageResolutionInlineSize();

  base::Optional<LayoutUnit> margin_top;
  if (!style.MarginTop().IsAuto())
    margin_top = ResolveMargin(space, style.MarginTop());
  base::Optional<LayoutUnit> margin_bottom;
  if (!style.MarginBottom().IsAuto())
    margin_bottom = ResolveMargin(space, style.MarginBottom());
  base::Optional<LayoutUnit> top;
  if (!style.Top().IsAuto())
    top = MinimumValueForLength(style.Top(), percentage_height);
  base::Optional<LayoutUnit> bottom;
  if (!style.Bottom().IsAuto())
    bottom = MinimumValueForLength(style.Bottom(), percentage_height);
  base::Optional<LayoutUnit> height = incoming_height;

  PhysicalSize container_size =
      ToPhysicalSize(space.AvailableSize(), space.GetWritingMode());
  DCHECK_NE(container_size.height, kIndefiniteSize);

  // Solving the equation:
  // top + marginTop + height + marginBottom + bottom
  // + border_padding = container height
  if (!top && !bottom && !height) {
    // Standard: "If all three of top, height, and bottom are auto:"
    if (!margin_top)
      margin_top = LayoutUnit();
    if (!margin_bottom)
      margin_bottom = LayoutUnit();
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(
        ComputeAvailableHeight(container_size.height, top, bottom, margin_top,
                               margin_bottom, static_position));
    if (IsTopDominant(container_writing_mode, container_direction)) {
      top = static_position.TopInset(container_size.height, *height,
                                     *margin_top, *margin_bottom);
    } else {
      bottom = static_position.BottomInset(container_size.height, *height,
                                           *margin_top, *margin_bottom);
    }
  } else if (top && bottom && height) {
    // Standard: "If top, bottom, and height are not auto:"
    // Compute margins.
    LayoutUnit margin_space = container_size.height - *top - *bottom - *height;
    if (!margin_top && !margin_bottom) {
      // When both margins are auto.
      margin_top = margin_space / 2;
      margin_bottom = margin_space - *margin_top;
    } else if (!margin_top) {
      margin_top = margin_space - *margin_bottom;
    } else if (!margin_bottom) {
      margin_bottom = margin_space - *margin_top;
    } else {
      // Since none of the margins are auto (and we have non-auto top, bottom
      // and height), we are over-constrained. Keep the dominant inset and
      // override the other.
      LayoutUnit margin_extra = margin_space - *margin_top - *margin_bottom;
      if (margin_extra) {
        if (IsTopDominant(container_writing_mode, container_direction))
          bottom = *bottom + margin_extra;
        else
          top = *top + margin_extra;
      }
    }
  }

  // Set unknown margins.
  if (!margin_top)
    margin_top = LayoutUnit();
  if (!margin_bottom)
    margin_bottom = LayoutUnit();

  // Rules 1 through 3, 2 out of 3 are unknown, fix 1.
  if (!top && !height) {
    // Rule 1.
    DCHECK(bottom.has_value());
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(
        ComputeAvailableHeight(container_size.height, top, bottom, margin_top,
                               margin_bottom, static_position));
  } else if (!top && !bottom) {
    // Rule 2.
    DCHECK(height.has_value());
    if (IsTopDominant(container_writing_mode, container_direction)) {
      top = static_position.TopInset(container_size.height, *height,
                                     *margin_top, *margin_bottom);
    } else {
      bottom = static_position.BottomInset(container_size.height, *height,
                                           *margin_top, *margin_bottom);
    }
  } else if (!height && !bottom) {
    // Rule 3.
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(
        ComputeAvailableHeight(container_size.height, top, bottom, margin_top,
                               margin_bottom, static_position));
  }

  // Rules 4 through 6, 1 out of 3 are unknown.
  if (!top) {
    top = container_size.height - *height - *bottom - *margin_top -
          *margin_bottom;
  } else if (!bottom) {
    bottom =
        container_size.height - *height - *top - *margin_top - *margin_bottom;
  } else if (!height) {
    height =
        container_size.height - *top - *bottom - *margin_top - *margin_bottom;
  }

#if DCHECK_IS_ON()
  // The DCHECK is useful, but only holds true when not saturated.
  if (!(top->MightBeSaturated() || bottom->MightBeSaturated() ||
        height->MightBeSaturated() || margin_top->MightBeSaturated() ||
        margin_bottom->MightBeSaturated() ||
        container_size.height.MightBeSaturated())) {
    DCHECK_EQ(container_size.height,
              *top + *bottom + *margin_top + *margin_bottom + *height);
  }
#endif  // #if DCHECK_IS_ON()

  // If calculated height is outside of min/max constraints,
  // rerun the algorithm with constrained width.
  LayoutUnit min = ResolveMinHeight(space, style, border_padding, child_minmax);
  LayoutUnit max = ResolveMaxHeight(space, style, border_padding, child_minmax,
                                    style.MaxHeight());
  if (height != ConstrainByMinMax(*height, min, max)) {
    height = ConstrainByMinMax(*height, min, max);
    // Because this function only changes "height" when it's not already
    // set, it is safe to recursively call ourselves here because on the
    // second call it is guaranteed to be within min..max.
    ComputeAbsoluteVertical(
        space, style, border_padding, height, static_position, child_minmax,
        container_writing_mode, container_direction, position);
    return;
  }
  // Negative heights are not allowed.
  LayoutUnit vertical_border_padding =
      border_padding
          .ConvertToPhysical(space.GetWritingMode(), space.Direction())
          .VerticalSum();
  height = std::max(*height, vertical_border_padding);

  position->inset.top = *top + *margin_top;
  position->inset.bottom = *bottom + *margin_bottom;
  position->margins.top = *margin_top;
  position->margins.bottom = *margin_bottom;
  position->size.height = *height;
}

}  // namespace

String NGAbsolutePhysicalPosition::ToString() const {
  return String::Format("INSET(LRTB):%d,%d,%d,%d SIZE:%dx%d",
                        inset.left.ToInt(), inset.right.ToInt(),
                        inset.top.ToInt(), inset.bottom.ToInt(),
                        size.width.ToInt(), size.height.ToInt());
}

bool AbsoluteNeedsChildBlockSize(const ComputedStyle& style) {
  if (style.IsHorizontalWritingMode())
    return AbsoluteVerticalNeedsEstimate(style);
  else
    return AbsoluteHorizontalNeedsEstimate(style);
}

bool AbsoluteNeedsChildInlineSize(const ComputedStyle& style) {
  if (style.IsHorizontalWritingMode())
    return AbsoluteHorizontalNeedsEstimate(style);
  else
    return AbsoluteVerticalNeedsEstimate(style);
}

base::Optional<LayoutUnit> ComputeAbsoluteDialogYPosition(
    const LayoutObject& dialog,
    LayoutUnit height) {
  if (!IsHTMLDialogElement(dialog.GetNode()))
    return base::nullopt;

  // This code implements <dialog> static position spec.
  // //
  // https://html.spec.whatwg.org/C/#the-dialog-element
  HTMLDialogElement* dialog_node = ToHTMLDialogElement(dialog.GetNode());
  if (dialog_node->GetCenteringMode() == HTMLDialogElement::kNotCentered)
    return base::nullopt;

  bool can_center_dialog =
      (dialog.Style()->GetPosition() == EPosition::kAbsolute ||
       dialog.Style()->GetPosition() == EPosition::kFixed) &&
      dialog.Style()->HasAutoTopAndBottom();

  if (dialog_node->GetCenteringMode() == HTMLDialogElement::kCentered) {
    if (can_center_dialog)
      return dialog_node->CenteredPosition();
    return base::nullopt;
  }

  DCHECK_EQ(dialog_node->GetCenteringMode(),
            HTMLDialogElement::kNeedsCentering);
  if (!can_center_dialog) {
    dialog_node->SetNotCentered();
    return base::nullopt;
  }

  auto* scrollable_area = dialog.GetDocument().View()->LayoutViewport();
  LayoutUnit top =
      LayoutUnit((dialog.Style()->GetPosition() == EPosition::kFixed)
                     ? 0
                     : scrollable_area->ScrollOffsetInt().Height());

  int visible_height = dialog.GetDocument().View()->Height();
  if (height < visible_height)
    top += (visible_height - height) / 2;
  dialog_node->SetCentered(top);
  return top;
}

NGAbsolutePhysicalPosition ComputePartialAbsoluteWithChildInlineSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const NGPhysicalStaticPosition& static_position,
    const base::Optional<MinMaxSize>& child_minmax,
    const base::Optional<LogicalSize>& replaced_size,
    const WritingMode container_writing_mode,
    const TextDirection container_direction) {
  NGAbsolutePhysicalPosition position;
  if (style.IsHorizontalWritingMode()) {
    base::Optional<LayoutUnit> width;
    if (!TableAwareWidth(style).IsAuto()) {
      width = ResolveMainWidth(space, style, border_padding, child_minmax,
                               style.Width());
    } else if (replaced_size.has_value()) {
      width = replaced_size->inline_size;
    }
    ComputeAbsoluteHorizontal(
        space, style, border_padding, width, static_position, child_minmax,
        container_writing_mode, container_direction, &position);
  } else {
    base::Optional<LayoutUnit> height;
    if (!TableAwareHeight(style).IsAuto()) {
      height = ResolveMainHeight(space, style, border_padding, child_minmax,
                                 style.Height());
    } else if (replaced_size.has_value()) {
      height = replaced_size->inline_size;
    }
    ComputeAbsoluteVertical(
        space, style, border_padding, height, static_position, child_minmax,
        container_writing_mode, container_direction, &position);
  }
  return position;
}

void ComputeFullAbsoluteWithChildBlockSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const NGPhysicalStaticPosition& static_position,
    const base::Optional<LayoutUnit>& child_block_size,
    const base::Optional<LogicalSize>& replaced_size,
    const WritingMode container_writing_mode,
    const TextDirection container_direction,
    NGAbsolutePhysicalPosition* position) {
  // After partial size has been computed, child block size is either
  // unknown, or fully computed, there is no minmax.
  // To express this, a 'fixed' minmax is created where
  // min and max are the same.
  base::Optional<MinMaxSize> child_minmax;
  if (child_block_size.has_value()) {
    child_minmax = MinMaxSize{*child_block_size, *child_block_size};
  }
  if (style.IsHorizontalWritingMode()) {
    base::Optional<LayoutUnit> height;
    if (!TableAwareHeight(style).IsAuto()) {
      height = ResolveMainHeight(space, style, border_padding, child_minmax,
                                 style.Height());
    } else if (replaced_size.has_value()) {
      height = replaced_size->block_size;
    }
    ComputeAbsoluteVertical(
        space, style, border_padding, height, static_position, child_minmax,
        container_writing_mode, container_direction, position);
  } else {
    base::Optional<LayoutUnit> width;
    if (!TableAwareWidth(style).IsAuto()) {
      width = ResolveMainWidth(space, style, border_padding, child_minmax,
                               style.Width());
    } else if (replaced_size.has_value()) {
      width = replaced_size->block_size;
    }
    ComputeAbsoluteHorizontal(
        space, style, border_padding, width, static_position, child_minmax,
        container_writing_mode, container_direction, position);
  }
}

}  // namespace blink
