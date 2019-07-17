// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/logical_values.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace {

inline scoped_refptr<const NGLayoutResult> LayoutInflow(
    const NGConstraintSpace& space,
    const NGBreakToken* break_token,
    NGLayoutInputNode* node,
    NGInlineChildLayoutContext* context) {
  auto* inline_node = DynamicTo<NGInlineNode>(node);
  return inline_node ? inline_node->Layout(space, break_token, context)
                     : To<NGBlockNode>(node)->Layout(space, break_token);
}

NGAdjoiningObjectTypes ToAdjoiningObjectTypes(EClear clear) {
  switch (clear) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case EClear::kNone:
      return kAdjoiningNone;
    case EClear::kLeft:
      return kAdjoiningFloatLeft;
    case EClear::kRight:
      return kAdjoiningFloatRight;
    case EClear::kBoth:
      return kAdjoiningFloatBoth;
  };
}

// Return true if a child is to be cleared past adjoining floats. These are
// floats that would otherwise (if 'clear' were 'none') be pulled down by the
// BFC block offset of the child. If the child is to clear floats, though, we
// obviously need separate the child from the floats and move it past them,
// since that's what clearance is all about. This means that if we have any such
// floats to clear, we know for sure that we get clearance, even before layout.
inline bool HasClearancePastAdjoiningFloats(
    NGAdjoiningObjectTypes adjoining_object_types,
    const ComputedStyle& child_style,
    const ComputedStyle& cb_style) {
  return ToAdjoiningObjectTypes(ResolvedClear(child_style, cb_style)) &
         adjoining_object_types;
}

// Adjust BFC block offset for clearance, if applicable. Return true of
// clearance was applied.
//
// Clearance applies either when the BFC block offset calculated simply isn't
// past all relevant floats, *or* when we have already determined that we're
// directly preceded by clearance.
//
// The latter is the case when we need to force ourselves past floats that would
// otherwise be adjoining, were it not for the predetermined clearance.
// Clearance inhibits margin collapsing and acts as spacing before the
// block-start margin of the child. It needs to be exactly what takes the
// block-start border edge of the cleared block adjacent to the block-end outer
// edge of the "bottommost" relevant float.
//
// We cannot reliably calculate the actual clearance amount at this point,
// because 1) this block right here may actually be a descendant of the block
// that is to be cleared, and 2) we may not yet have separated the margin before
// and after the clearance. None of this matters, though, because we know where
// to place this block if clearance applies: exactly at the ConstraintSpace's
// ClearanceOffset().
bool ApplyClearance(const NGConstraintSpace& constraint_space,
                    LayoutUnit* bfc_block_offset) {
  if (constraint_space.HasClearanceOffset() &&
      *bfc_block_offset < constraint_space.ClearanceOffset()) {
    *bfc_block_offset = constraint_space.ClearanceOffset();
    return true;
  }
  return false;
}

LayoutUnit LogicalFromBfcLineOffset(LayoutUnit child_bfc_line_offset,
                                    LayoutUnit parent_bfc_line_offset,
                                    LayoutUnit child_inline_size,
                                    LayoutUnit parent_inline_size,
                                    TextDirection direction) {
  // We need to respect the current text direction to calculate the logical
  // offset correctly.
  LayoutUnit relative_line_offset =
      child_bfc_line_offset - parent_bfc_line_offset;

  LayoutUnit inline_offset =
      direction == TextDirection::kLtr
          ? relative_line_offset
          : parent_inline_size - relative_line_offset - child_inline_size;

  return inline_offset;
}

LogicalOffset LogicalFromBfcOffsets(const NGBfcOffset& child_bfc_offset,
                                    const NGBfcOffset& parent_bfc_offset,
                                    LayoutUnit child_inline_size,
                                    LayoutUnit parent_inline_size,
                                    TextDirection direction) {
  LayoutUnit inline_offset = LogicalFromBfcLineOffset(
      child_bfc_offset.line_offset, parent_bfc_offset.line_offset,
      child_inline_size, parent_inline_size, direction);

  return {inline_offset,
          child_bfc_offset.block_offset - parent_bfc_offset.block_offset};
}

// Stop margin collapsing on one side of a block when
// -webkit-margin-{after,before}-collapse is something other than 'collapse'
// (the initial value)
void StopMarginCollapsing(EMarginCollapse collapse_value,
                          LayoutUnit this_margin,
                          LayoutUnit* logical_block_offset,
                          NGMarginStrut* margin_strut) {
  DCHECK_NE(collapse_value, EMarginCollapse::kCollapse);
  if (collapse_value == EMarginCollapse::kSeparate) {
    // Separate margins between previously adjoining margins and this margin,
    // AND between this margin and adjoining margins to come.
    *logical_block_offset += margin_strut->Sum() + this_margin;
    *margin_strut = NGMarginStrut();
    return;
  }
  DCHECK_EQ(collapse_value, EMarginCollapse::kDiscard);
  // Discard previously adjoining margins, this margin AND all adjoining margins
  // to come, so that the sum becomes 0.
  margin_strut->discard_margins = true;
}

}  // namespace

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      border_padding_(params.fragment_geometry.border +
                      params.fragment_geometry.padding),
      border_scrollbar_padding_(border_padding_ +
                                params.fragment_geometry.scrollbar),
      is_resuming_(IsResumingLayout(params.break_token)),
      exclusion_space_(params.space.ExclusionSpace()) {
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGBlockLayoutAlgorithm::~NGBlockLayoutAlgorithm() = default;

void NGBlockLayoutAlgorithm::SetBoxType(NGPhysicalFragment::NGBoxType type) {
  container_builder_.SetBoxType(type);
}

base::Optional<MinMaxSize> NGBlockLayoutAlgorithm::ComputeMinMaxSize(
    const MinMaxSizeInput& input) const {
  base::Optional<MinMaxSize> sizes = CalculateMinMaxSizesIgnoringChildren(
      node_, border_scrollbar_padding_, input.size_type);
  if (sizes)
    return sizes;

  sizes.emplace();
  LayoutUnit child_percentage_resolution_block_size =
      CalculateChildPercentageBlockSizeForMinMax(
          ConstraintSpace(), Node(), border_padding_,
          input.percentage_resolution_block_size);

  const TextDirection direction = Style().Direction();
  LayoutUnit float_left_inline_size = input.float_left_inline_size;
  LayoutUnit float_right_inline_size = input.float_right_inline_size;

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned() || child.IsColumnSpanAll())
      continue;

    const ComputedStyle& child_style = child.Style();
    const EClear child_clear = ResolvedClear(child_style, Style());
    bool child_is_new_fc = child.CreatesNewFormattingContext();

    // Conceptually floats and a single new-FC would just get positioned on a
    // single "line". If there is a float/new-FC with clearance, this creates a
    // new "line", resetting the appropriate float size trackers.
    //
    // Both of the float size trackers get reset for anything that isn't a float
    // (inflow and new-FC) at the end of the loop, as this creates a new "line".
    if (child.IsFloating() || child_is_new_fc) {
      LayoutUnit float_inline_size =
          float_left_inline_size + float_right_inline_size;

      if (child_clear != EClear::kNone)
        sizes->max_size = std::max(sizes->max_size, float_inline_size);

      if (child_clear == EClear::kBoth || child_clear == EClear::kLeft)
        float_left_inline_size = LayoutUnit();

      if (child_clear == EClear::kBoth || child_clear == EClear::kRight)
        float_right_inline_size = LayoutUnit();
    }

    MinMaxSizeInput child_input(child_percentage_resolution_block_size);
    if (child.IsInline() || child.IsAnonymousBlock()) {
      child_input.float_left_inline_size = float_left_inline_size;
      child_input.float_right_inline_size = float_right_inline_size;
    }

    MinMaxSize child_sizes;
    if (child.IsInline()) {
      // From |NGBlockLayoutAlgorithm| perspective, we can handle |NGInlineNode|
      // almost the same as |NGBlockNode|, because an |NGInlineNode| includes
      // all inline nodes following |child| and their descendants, and produces
      // an anonymous box that contains all line boxes.
      // |NextSibling| returns the next block sibling, or nullptr, skipping all
      // following inline siblings and descendants.
      child_sizes =
          child.ComputeMinMaxSize(Style().GetWritingMode(), child_input);
    } else {
      child_sizes =
          ComputeMinAndMaxContentContribution(Style(), child, child_input);
    }
    DCHECK_LE(child_sizes.min_size, child_sizes.max_size) << child.ToString();

    // Determine the max inline contribution of the child.
    NGBoxStrut margins = ComputeMinMaxMargins(Style(), child);
    LayoutUnit max_inline_contribution;

    if (child.IsFloating()) {
      // A float adds to its inline size to the current "line". The new max
      // inline contribution is just the sum of all the floats on that "line".
      LayoutUnit float_inline_size = child_sizes.max_size + margins.InlineSum();

      // float_inline_size is negative when the float is completely outside of
      // the content area, by e.g., negative margins. Such floats do not affect
      // the content size.
      if (float_inline_size > 0) {
        if (ResolvedFloating(child_style, Style()) == EFloat::kLeft)
          float_left_inline_size += float_inline_size;
        else
          float_right_inline_size += float_inline_size;
      }

      max_inline_contribution =
          float_left_inline_size + float_right_inline_size;
    } else if (child_is_new_fc) {
      // As floats are line relative, we perform the margin calculations in the
      // line relative coordinate system as well.
      LayoutUnit margin_line_left = margins.LineLeft(direction);
      LayoutUnit margin_line_right = margins.LineRight(direction);

      // line_left_inset and line_right_inset are the "distance" from their
      // respective edges of the parent that the new-FC would take. If the
      // margin is positive the inset is just whichever of the floats inline
      // size and margin is larger, and if negative it just subtracts from the
      // float inline size.
      LayoutUnit line_left_inset =
          margin_line_left > LayoutUnit()
              ? std::max(float_left_inline_size, margin_line_left)
              : float_left_inline_size + margin_line_left;

      LayoutUnit line_right_inset =
          margin_line_right > LayoutUnit()
              ? std::max(float_right_inline_size, margin_line_right)
              : float_right_inline_size + margin_line_right;

      max_inline_contribution =
          child_sizes.max_size + line_left_inset + line_right_inset;
    } else {
      // This is just a standard inflow child.
      max_inline_contribution = child_sizes.max_size + margins.InlineSum();
    }
    sizes->max_size = std::max(sizes->max_size, max_inline_contribution);

    // The min inline contribution just assumes that floats are all on their own
    // "line".
    LayoutUnit min_inline_contribution =
        child_sizes.min_size + margins.InlineSum();
    sizes->min_size = std::max(sizes->min_size, min_inline_contribution);

    // Anything that isn't a float will create a new "line" resetting the float
    // size trackers.
    if (!child.IsFloating()) {
      float_left_inline_size = LayoutUnit();
      float_right_inline_size = LayoutUnit();
    }
  }

  DCHECK_GE(sizes->min_size, LayoutUnit());
  DCHECK_LE(sizes->min_size, sizes->max_size) << Node().ToString();

  if (input.size_type == NGMinMaxSizeType::kBorderBoxSize)
    *sizes += border_scrollbar_padding_.InlineSum();
  return sizes;
}

LogicalOffset NGBlockLayoutAlgorithm::CalculateLogicalOffset(
    const NGFragment& fragment,
    LayoutUnit child_bfc_line_offset,
    const base::Optional<LayoutUnit>& child_bfc_block_offset) {
  LayoutUnit inline_size = container_builder_.Size().inline_size;
  TextDirection direction = ConstraintSpace().Direction();

  if (child_bfc_block_offset && container_builder_.BfcBlockOffset()) {
    return LogicalFromBfcOffsets(
        {child_bfc_line_offset, *child_bfc_block_offset}, ContainerBfcOffset(),
        fragment.InlineSize(), inline_size, direction);
  }

  LayoutUnit inline_offset = LogicalFromBfcLineOffset(
      child_bfc_line_offset, container_builder_.BfcLineOffset(),
      fragment.InlineSize(), inline_size, direction);

  // If we've reached here, either the parent, or the child don't have a BFC
  // block-offset yet. Children in this situation are always placed at a
  // logical block-offset of zero.
  return {inline_offset, LayoutUnit()};
}

scoped_refptr<const NGLayoutResult> NGBlockLayoutAlgorithm::Layout() {
  // Inline children require an inline child layout context to be
  // passed between siblings. We want to stack-allocate that one, but
  // only on demand, as it's quite big.
  if (Node().ChildrenInline())
    return LayoutWithInlineChildLayoutContext();
  return Layout(nullptr);
}

NOINLINE scoped_refptr<const NGLayoutResult>
NGBlockLayoutAlgorithm::LayoutWithInlineChildLayoutContext() {
  NGInlineChildLayoutContext context;
  return Layout(&context);
}

inline scoped_refptr<const NGLayoutResult> NGBlockLayoutAlgorithm::Layout(
    NGInlineChildLayoutContext* inline_child_layout_context) {
  const LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();
  child_available_size_ =
      ShrinkAvailableSize(border_box_size, border_scrollbar_padding_);

  child_percentage_size_ = CalculateChildPercentageSize(
      ConstraintSpace(), Node(), child_available_size_);
  replaced_child_percentage_size_ = CalculateReplacedChildPercentageSize(
      ConstraintSpace(), Node(), border_box_size, border_scrollbar_padding_,
      border_padding_);

  // All of the above calculations with border_scrollbar_padding_ shouldn't
  // include the table cell's intrinsic padding. We can now add this.
  border_scrollbar_padding_ +=
      ComputeIntrinsicPadding(ConstraintSpace(), Node());

  if (ConstraintSpace().HasBlockFragmentation())
    container_builder_.SetHasBlockFragmentation();
  container_builder_.SetBfcLineOffset(
      ConstraintSpace().BfcOffset().line_offset);

  if (NGAdjoiningObjectTypes adjoining_object_types =
          ConstraintSpace().AdjoiningObjectTypes()) {
    DCHECK(!ConstraintSpace().IsNewFormattingContext());
    DCHECK(!container_builder_.BfcBlockOffset());

    // If there were preceding adjoining objects, they will be affected when the
    // BFC block-offset gets resolved or updated. We then need to roll back and
    // re-layout those objects with the new BFC block-offset, once the BFC
    // block-offset is updated.
    abort_when_bfc_block_offset_updated_ = true;

    container_builder_.SetAdjoiningObjectTypes(adjoining_object_types);
  }

  // If we are resuming from a break token our start border and padding is
  // within a previous fragment.
  LayoutUnit content_edge =
      is_resuming_ ? LayoutUnit() : border_scrollbar_padding_.block_start;

  NGPreviousInflowPosition previous_inflow_position = {
      LayoutUnit(), ConstraintSpace().MarginStrut(),
      /* self_collapsing_child_had_clearance */ false};

  // Do not collapse margins between parent and its child if:
  //
  // A: There is border/padding between them.
  // B: This is a new formatting context
  // C: We're resuming layout from a break token. Margin struts cannot pass from
  //    one fragment to another if they are generated by the same block; they
  //    must be dealt with at the first fragment.
  // D: We're forced to stop margin collapsing by a CSS property
  //
  // In all those cases we can and must resolve the BFC block offset now.
  if (border_scrollbar_padding_.block_start || is_resuming_ ||
      ConstraintSpace().IsNewFormattingContext() ||
      Style().MarginBeforeCollapse() != EMarginCollapse::kCollapse) {
    bool discard_subsequent_margins =
        previous_inflow_position.margin_strut.discard_margins &&
        !border_scrollbar_padding_.block_start;
    if (!ResolveBfcBlockOffset(&previous_inflow_position)) {
      // There should be no preceding content that depends on the BFC block
      // offset of a new formatting context block, and likewise when resuming
      // from a break token.
      DCHECK(!ConstraintSpace().IsNewFormattingContext());
      DCHECK(!is_resuming_);
      return container_builder_.Abort(NGLayoutResult::kBfcBlockOffsetResolved);
    }
    // Move to the content edge. This is where the first child should be placed.
    previous_inflow_position.logical_block_offset = content_edge;

    // If we resolved the BFC block offset now, the margin strut has been
    // reset. If margins are to be discarded, and this box would otherwise have
    // adjoining margins between its own margin and those subsequent content,
    // we need to make sure subsequent content discard theirs.
    if (discard_subsequent_margins)
      previous_inflow_position.margin_strut.discard_margins = true;
  }

#if DCHECK_IS_ON()
  // If this is a new formatting context, we should definitely be at the origin
  // here. If we're resuming from a break token (for a block that doesn't
  // establish a new formatting context), that may not be the case,
  // though. There may e.g. be clearance involved, or inline-start margins.
  if (ConstraintSpace().IsNewFormattingContext())
    DCHECK_EQ(*container_builder_.BfcBlockOffset(), LayoutUnit());
  // If this is a new formatting context, or if we're resuming from a break
  // token, no margin strut must be lingering around at this point.
  if (ConstraintSpace().IsNewFormattingContext() || is_resuming_)
    DCHECK(ConstraintSpace().MarginStrut().IsEmpty());

  if (!container_builder_.BfcBlockOffset()) {
    // New formatting-contexts, and when we have a self-collapsing child
    // affected by clearance must already have their BFC block-offset resolved.
    DCHECK(!previous_inflow_position.self_collapsing_child_had_clearance);
    DCHECK(!ConstraintSpace().IsNewFormattingContext());
  }
#endif

  // If this node is a quirky container, (we are in quirks mode and either a
  // table cell or body), we set our margin strut to a mode where it only
  // considers non-quirky margins. E.g.
  // <body>
  //   <p></p>
  //   <div style="margin-top: 10px"></div>
  //   <h1>Hello</h1>
  // </body>
  // In the above example <p>'s & <h1>'s margins are ignored as they are
  // quirky, and we only consider <div>'s 10px margin.
  if (node_.IsQuirkyContainer())
    previous_inflow_position.margin_strut.is_quirky_container_start = true;

  // Before we descend into children (but after we have determined our inline
  // size), give the autosizer an opportunity to adjust the font size on the
  // children.
  TextAutosizer::NGLayoutScope text_autosizer_layout_scope(
      Node(), border_box_size.inline_size);

  // Try to reuse line box fragments from cached fragments if possible.
  // When possible, this adds fragments to |container_builder_| and update
  // |previous_inflow_position| and |BreakToken()|.
  scoped_refptr<const NGInlineBreakToken> previous_inline_break_token;

  NGBlockChildIterator child_iterator(Node().FirstChild(), BreakToken());

  // If this layout is blocked by a display-lock, then we pretend this node has
  // no children and that there are no break tokens. Due to this, we skip layout
  // on these children.
  if (Node().LayoutBlockedByDisplayLock(DisplayLockContext::kChildren))
    child_iterator = NGBlockChildIterator(NGBlockNode(nullptr), nullptr);

  for (auto entry = child_iterator.NextChild();
       NGLayoutInputNode child = entry.node;
       entry = child_iterator.NextChild(previous_inline_break_token.get())) {
    const NGBreakToken* child_break_token = entry.token;

    if (child.IsOutOfFlowPositioned()) {
      DCHECK(!child_break_token);
      HandleOutOfFlowPositioned(previous_inflow_position,
                                To<NGBlockNode>(child));
    } else if (child.IsFloating()) {
      HandleFloat(previous_inflow_position, To<NGBlockNode>(child),
                  To<NGBlockBreakToken>(child_break_token));
    } else if (child.IsListMarker() && !child.ListMarkerOccupiesWholeLine()) {
      container_builder_.SetUnpositionedListMarker(
          NGUnpositionedListMarker(To<NGBlockNode>(child)));
    } else {
      // We need to propagate the initial break-before value up our container
      // chain, until we reach a container that's not a first child. If we get
      // all the way to the root of the fragmentation context without finding
      // any such container, we have no valid class A break point, and if a
      // forced break was requested, none will be inserted.
      if (!child.IsInline() && ConstraintSpace().HasBlockFragmentation())
        container_builder_.SetInitialBreakBefore(child.Style().BreakBefore());

      bool abort;
      if (child.CreatesNewFormattingContext()) {
        abort = !HandleNewFormattingContext(child, child_break_token,
                                            &previous_inflow_position);
        previous_inline_break_token = nullptr;
      } else {
        abort = !HandleInflow(
            child, child_break_token, &previous_inflow_position,
            inline_child_layout_context, &previous_inline_break_token);
      }

      if (abort) {
        // We need to abort the layout, as our BFC block offset was resolved.
        return container_builder_.Abort(
            NGLayoutResult::kBfcBlockOffsetResolved);
      }
      if (container_builder_.DidBreak() &&
          IsFragmentainerOutOfSpace(
              previous_inflow_position.logical_block_offset))
        break;
      has_processed_first_child_ = true;
    }
  }

  // The intrinsic block size is not allowed to be less than the content edge
  // offset, as that could give us a negative content box size.
  intrinsic_block_size_ = content_edge;

  // To save space of the stack when we recurse into children, the rest of this
  // function is continued within |FinishLayout|. However it should be read as
  // one function.
  return FinishLayout(&previous_inflow_position);
}

scoped_refptr<const NGLayoutResult> NGBlockLayoutAlgorithm::FinishLayout(
    NGPreviousInflowPosition* previous_inflow_position) {
  LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();
  NGMarginStrut end_margin_strut = previous_inflow_position->margin_strut;

  // If the current layout is a new formatting context, we need to encapsulate
  // all of our floats.
  if (ConstraintSpace().IsNewFormattingContext()) {
    intrinsic_block_size_ = std::max(
        intrinsic_block_size_, exclusion_space_.ClearanceOffset(EClear::kBoth));
  }

  // The end margin strut of an in-flow fragment contributes to the size of the
  // current fragment if:
  //  - There is block-end border/scrollbar/padding.
  //  - There was a self-collapsing child affected by clearance.
  //  - We are a new formatting context.
  // Additionally this fragment produces no end margin strut.
  if (border_scrollbar_padding_.block_end ||
      previous_inflow_position->self_collapsing_child_had_clearance ||
      ConstraintSpace().IsNewFormattingContext()) {
    // If we are a quirky container, we ignore any quirky margins and
    // just consider normal margins to extend our size.  Other UAs
    // perform this calculation differently, e.g. by just ignoring the
    // *last* quirky margin.
    // TODO: revisit previous implementation to avoid changing behavior and
    // https://html.spec.whatwg.org/C/#margin-collapsing-quirks
    LayoutUnit margin_strut_sum = node_.IsQuirkyContainer()
                                      ? end_margin_strut.QuirkyContainerSum()
                                      : end_margin_strut.Sum();
    if (!container_builder_.BfcBlockOffset()) {
      // If we have collapsed through the block start and all children (if any),
      // now is the time to determine the BFC block offset, because finally we
      // have found something solid to hang on to (like clearance or a bottom
      // border, for instance). If we're a new formatting context, though, we
      // shouldn't be here, because then the offset should already have been
      // determined.
      DCHECK(!ConstraintSpace().IsNewFormattingContext());
      if (!ResolveBfcBlockOffset(previous_inflow_position)) {
        return container_builder_.Abort(
            NGLayoutResult::kBfcBlockOffsetResolved);
      }
      DCHECK(container_builder_.BfcBlockOffset());
    } else {
      // The trailing margin strut will be part of our intrinsic block size, but
      // only if there is something that separates the end margin strut from the
      // input margin strut (typically child content, block start
      // border/padding, or this being a new BFC). If the margin strut from a
      // previous sibling or ancestor managed to collapse through all our
      // children (if any at all, that is), it means that the resulting end
      // margin strut actually pushes us down, and it should obviously not be
      // doubly accounted for as our block size.
      intrinsic_block_size_ = std::max(
          intrinsic_block_size_,
          previous_inflow_position->logical_block_offset + margin_strut_sum);
    }

    intrinsic_block_size_ += border_scrollbar_padding_.block_end;
    end_margin_strut = NGMarginStrut();
  } else {
    // Update our intrinsic block size to be just past the block-end border edge
    // of the last in-flow child. The pending margin is to be propagated to our
    // container, so ignore it.
    intrinsic_block_size_ = std::max(
        intrinsic_block_size_, previous_inflow_position->logical_block_offset);
  }

  intrinsic_block_size_ = std::max(intrinsic_block_size_,
                                   CalculateMinimumBlockSize(end_margin_strut));

  // TODO(layout-dev): Is CalculateMinimumBlockSize common to other algorithms,
  // and should move into ClampIntrinsicBlockSize?
  intrinsic_block_size_ = ClampIntrinsicBlockSize(
      Node(), border_scrollbar_padding_, intrinsic_block_size_);

  // Recompute the block-axis size now that we know our content size.
  border_box_size.block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Node(), border_padding_, intrinsic_block_size_);
  container_builder_.SetBlockSize(border_box_size.block_size);

  // If our BFC block-offset is still unknown, we check:
  //  - If we have a non-zero block-size (margins don't collapse through us).
  //  - If we have a break token. (Even if we are self-collapsing we position
  //    ourselves at the very start of the fragmentainer).
  if (!container_builder_.BfcBlockOffset() &&
      (border_box_size.block_size || BreakToken())) {
    if (!ResolveBfcBlockOffset(previous_inflow_position))
      return container_builder_.Abort(NGLayoutResult::kBfcBlockOffsetResolved);
    DCHECK(container_builder_.BfcBlockOffset());
  }

  if (container_builder_.BfcBlockOffset()) {
    // Do not collapse margins between the last in-flow child and bottom margin
    // of its parent if:
    //  - The parent has block-size != auto.
    //  - The block-size differs from the intrinsic size.
    if (!Style().LogicalHeight().IsAuto() ||
        border_box_size.block_size != intrinsic_block_size_) {
      end_margin_strut = NGMarginStrut();
    }
  }

  // List markers should have been positioned if we had line boxes, or boxes
  // that have line boxes. If there were no line boxes, position without line
  // boxes.
  if (container_builder_.UnpositionedListMarker() && node_.IsListItem()) {
    if (!PositionListMarkerWithoutLineBoxes(previous_inflow_position))
      return container_builder_.Abort(NGLayoutResult::kBfcBlockOffsetResolved);
  }

  container_builder_.SetEndMarginStrut(end_margin_strut);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);

  if (container_builder_.BfcBlockOffset()) {
    // If we know our BFC block-offset we should have correctly placed all
    // adjoining objects, and shouldn't propagate this information to siblings.
    container_builder_.ResetAdjoiningObjectTypes();
  } else {
    // If we don't know our BFC block-offset yet, we know that for
    // margin-collapsing purposes we are self-collapsing.
    container_builder_.SetIsSelfCollapsing();

    // If we've been forced at a particular BFC block-offset, (either from
    // clearance past adjoining floats, or a re-layout), we can safely set our
    // BFC block-offset now.
    if (ConstraintSpace().ForcedBfcBlockOffset()) {
      container_builder_.SetBfcBlockOffset(
          *ConstraintSpace().ForcedBfcBlockOffset());
    }
  }

  // We only finalize for fragmentation if the fragment has a BFC block offset.
  // This may occur with a zero block size fragment. We need to know the BFC
  // block offset to determine where the fragmentation line is relative to us.
  if (container_builder_.BfcBlockOffset() &&
      ConstraintSpace().HasBlockFragmentation())
    FinalizeForFragmentation();

  NGOutOfFlowLayoutPart(
      Node(), ConstraintSpace(),
      container_builder_.Borders() + container_builder_.Scrollbar(),
      &container_builder_)
      .Run();

#if DCHECK_IS_ON()
  // If we're not participating in a fragmentation context, no block
  // fragmentation related fields should have been set.
  if (!ConstraintSpace().HasBlockFragmentation())
    container_builder_.CheckNoBlockFragmentation();
#endif

  PropagateBaselinesFromChildren();

  // An exclusion space is confined to nodes within the same formatting context.
  if (!ConstraintSpace().IsNewFormattingContext()) {
    container_builder_.SetExclusionSpace(std::move(exclusion_space_));
  }

  if (ConstraintSpace().UseFirstLineStyle())
    container_builder_.SetStyleVariant(NGStyleVariant::kFirstLine);

  return container_builder_.ToBoxFragment();
}

const NGInlineBreakToken* NGBlockLayoutAlgorithm::TryReuseFragmentsFromCache(
    NGInlineNode inline_node,
    NGPreviousInflowPosition* previous_inflow_position,
    bool* aborted_out) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGLineCacheEnabled());
  LayoutBox* layout_box = inline_node.GetLayoutBox();
  if (layout_box->SelfNeedsLayout())
    return nullptr;

  // If floats are intruding into this node, re-layout may be needed.
  if (!exclusion_space_.IsEmpty())
    return nullptr;

  // Laying out from a break token is not supported yet, because this logic
  // synthesize a break token.
  if (BreakToken())
    return nullptr;

  const NGPaintFragment* lineboxes =
      inline_node.ReusableLineBoxContainer(ConstraintSpace());
  if (!lineboxes)
    return nullptr;

  // Following is a copy of logic from HandleInFlow(). They need to keep in
  // sync.
  if (inline_node.IsEmptyInline())
    return nullptr;
  if (!ResolveBfcBlockOffset(previous_inflow_position)) {
    *aborted_out = true;
    return nullptr;
  }
  DCHECK(container_builder_.BfcBlockOffset());

  WritingMode writing_mode = container_builder_.GetWritingMode();
  TextDirection direction = container_builder_.Direction();
  DCHECK_EQ(writing_mode, lineboxes->Style().GetWritingMode());
  DCHECK_EQ(direction, lineboxes->Style().Direction());
  const PhysicalSize outer_size = lineboxes->Size();

  LayoutUnit used_block_size = previous_inflow_position->logical_block_offset;
  NGBreakToken* last_break_token = nullptr;
  for (const NGPaintFragment* child : lineboxes->Children()) {
    if (child->IsDirty())
      break;

    // Abort if the line propagated its descendants to outside of the line. They
    // are propagated through NGLayoutResult, which we don't cache.
    const NGPhysicalLineBoxFragment* line =
        DynamicTo<NGPhysicalLineBoxFragment>(&child->PhysicalFragment());
    if (!line || line->HasPropagatedDescendants())
      break;

    // TODO(kojii): Running the normal layout code at least once for this child
    // helps reducing the code to setup internal states after the reuse. Remove
    // the last fragment if it is the end of the fragmentation to do so, but we
    // should figure out how to setup the states without doing this.
    NGBreakToken* break_token = line->BreakToken();
    DCHECK(break_token);
    if (break_token->IsFinished())
      break;

    last_break_token = break_token;
    LogicalOffset logical_offset = child->Offset().ConvertToLogical(
        writing_mode, direction, outer_size, line->Size());
    container_builder_.AddChild(
        *line, {logical_offset.inline_offset, used_block_size});
    used_block_size += line->Size().ConvertToLogical(writing_mode).block_size;
  }
  if (!last_break_token)
    return nullptr;

  // Update the internal states to after the re-used fragments.
  previous_inflow_position->logical_block_offset = used_block_size;

  // In order to layout the rest of lines, return the break token from the last
  // reused line box.
  DCHECK(last_break_token);
  DCHECK(!last_break_token->IsFinished());
  return To<NGInlineBreakToken>(last_break_token);
}

void NGBlockLayoutAlgorithm::HandleOutOfFlowPositioned(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGBlockNode child) {
  DCHECK(child.IsOutOfFlowPositioned());
  LogicalOffset static_offset = {border_scrollbar_padding_.inline_start,
                                 previous_inflow_position.logical_block_offset};

  // We only include the margin strut in the OOF static-position if we know we
  // aren't going to be a zero-block-size fragment.
  if (container_builder_.BfcBlockOffset())
    static_offset.block_offset += previous_inflow_position.margin_strut.Sum();

  if (child.Style().IsOriginalDisplayInlineType()) {
    // The static-position of inline-level OOF-positioned nodes depends on
    // previous floats (if any).
    //
    // Due to this we need to mark this node as having adjoining objects, and
    // perform a re-layout if our position shifts.
    if (!container_builder_.BfcBlockOffset()) {
      container_builder_.AddAdjoiningObjectTypes(kAdjoiningInlineOutOfFlow);
      abort_when_bfc_block_offset_updated_ = true;
    }

    LayoutUnit origin_bfc_block_offset =
        container_builder_.BfcBlockOffset().value_or(
            ConstraintSpace().ExpectedBfcBlockOffset()) +
        static_offset.block_offset;

    NGBfcOffset origin_bfc_offset = {
        ConstraintSpace().BfcOffset().line_offset +
            border_scrollbar_padding_.LineLeft(Style().Direction()),
        origin_bfc_block_offset};

    static_offset.inline_offset += CalculateOutOfFlowStaticInlineLevelOffset(
        Style(), origin_bfc_offset, exclusion_space_,
        child_available_size_.inline_size);
  }

  container_builder_.AddOutOfFlowChildCandidate(child, static_offset);
}

void NGBlockLayoutAlgorithm::HandleFloat(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGBlockNode child,
    const NGBlockBreakToken* child_break_token) {
  // If we're resuming layout, we must always know our position in the BFC.
  DCHECK(!IsResumingLayout(child_break_token) ||
         container_builder_.BfcBlockOffset());

  NGUnpositionedFloat unpositioned_float(child, child_break_token);

  if (!container_builder_.BfcBlockOffset()) {
    container_builder_.AddAdjoiningObjectTypes(
        unpositioned_float.IsLineLeft(ConstraintSpace().Direction())
            ? kAdjoiningFloatLeft
            : kAdjoiningFloatRight);
    // If we don't have a forced BFC block-offset yet, we'll optimistically
    // place floats at the "expected" BFC block-offset. If this differs from
    // our final BFC block-offset we'll need to re-layout.
    if (!ConstraintSpace().ForcedBfcBlockOffset())
      abort_when_bfc_block_offset_updated_ = true;
  }

  // If we don't have a BFC block-offset yet, the "expected" BFC block-offset
  // is used to optimistically place floats.
  NGBfcOffset origin_bfc_offset = {
      ConstraintSpace().BfcOffset().line_offset +
          border_scrollbar_padding_.LineLeft(ConstraintSpace().Direction()),
      container_builder_.BfcBlockOffset()
          ? NextBorderEdge(previous_inflow_position)
          : ConstraintSpace().ExpectedBfcBlockOffset()};

  NGPositionedFloat positioned_float = PositionFloat(
      child_available_size_, child_percentage_size_,
      replaced_child_percentage_size_, origin_bfc_offset, &unpositioned_float,
      ConstraintSpace(), Style(), &exclusion_space_);

  const auto& physical_fragment =
      positioned_float.layout_result->PhysicalFragment();
  LayoutUnit float_inline_size =
      NGFragment(ConstraintSpace().GetWritingMode(), physical_fragment)
          .InlineSize();

  NGBfcOffset bfc_offset = {ConstraintSpace().BfcOffset().line_offset,
                            container_builder_.BfcBlockOffset().value_or(
                                ConstraintSpace().ExpectedBfcBlockOffset())};

  LogicalOffset logical_offset = LogicalFromBfcOffsets(
      positioned_float.bfc_offset, bfc_offset, float_inline_size,
      container_builder_.Size().inline_size, ConstraintSpace().Direction());

  container_builder_.AddResult(*positioned_float.layout_result, logical_offset);
}

bool NGBlockLayoutAlgorithm::HandleNewFormattingContext(
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token,
    NGPreviousInflowPosition* previous_inflow_position) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK(!child.IsOutOfFlowPositioned());
  DCHECK(child.CreatesNewFormattingContext());
  DCHECK(child.IsBlock());

  const ComputedStyle& child_style = child.Style();
  const TextDirection direction = ConstraintSpace().Direction();
  NGInflowChildData child_data =
      ComputeChildData(*previous_inflow_position, child, child_break_token,
                       /* is_new_fc */ true);

  LayoutUnit child_origin_line_offset =
      ConstraintSpace().BfcOffset().line_offset +
      border_scrollbar_padding_.LineLeft(direction) +
      child_data.margins.LineLeft(direction).ClampNegativeToZero();

  // If the child has a block-start margin, and the BFC block offset is still
  // unresolved, and we have preceding adjoining floats, things get complicated
  // here. Depending on whether the child fits beside the floats, the margin may
  // or may not be adjoining with the current margin strut. This affects the
  // position of the preceding adjoining floats. We may have to resolve the BFC
  // block offset once with the child's margin tentatively adjoining, then
  // realize that the child isn't going to fit beside the floats at the current
  // position, and therefore re-resolve the BFC block offset with the child's
  // margin non-adjoining. This is akin to clearance.
  NGMarginStrut adjoining_margin_strut(previous_inflow_position->margin_strut);
  adjoining_margin_strut.Append(child_data.margins.block_start,
                                child_style.HasMarginBeforeQuirk());
  LayoutUnit adjoining_bfc_offset_estimate =
      child_data.bfc_offset_estimate.block_offset +
      adjoining_margin_strut.Sum();
  LayoutUnit non_adjoining_bfc_offset_estimate =
      child_data.bfc_offset_estimate.block_offset +
      previous_inflow_position->margin_strut.Sum();
  LayoutUnit child_bfc_offset_estimate = adjoining_bfc_offset_estimate;
  bool bfc_offset_already_resolved = false;
  bool child_determined_bfc_offset = false;
  bool child_margin_got_separated = false;
  bool has_adjoining_floats = false;

  if (!container_builder_.BfcBlockOffset()) {
    has_adjoining_floats =
        container_builder_.AdjoiningObjectTypes() & kAdjoiningFloatBoth;

    // If this node, or an arbitrary ancestor had clearance past adjoining
    // floats, we consider the margin "separated". We should *never* attempt to
    // re-resolve the BFC block-offset in this case.
    bool has_clearance_past_adjoining_floats =
        ConstraintSpace().AncestorHasClearancePastAdjoiningFloats() ||
        HasClearancePastAdjoiningFloats(
            container_builder_.AdjoiningObjectTypes(), child_style, Style());

    if (has_clearance_past_adjoining_floats) {
      child_bfc_offset_estimate = NextBorderEdge(*previous_inflow_position);
      child_margin_got_separated = true;
    } else if (ConstraintSpace().ForcedBfcBlockOffset()) {
      // This is not the first time we're here. We already have a suggested BFC
      // block offset.
      bfc_offset_already_resolved = true;
      child_bfc_offset_estimate = *ConstraintSpace().ForcedBfcBlockOffset();
      // We require that the BFC block offset be the one we'd get with either
      // margins adjoining or margins separated. Anything else is a bug.
      DCHECK(child_bfc_offset_estimate == adjoining_bfc_offset_estimate ||
             child_bfc_offset_estimate == non_adjoining_bfc_offset_estimate);
      // Figure out if the child margin has already got separated from the
      // margin strut or not.
      child_margin_got_separated =
          child_bfc_offset_estimate != adjoining_bfc_offset_estimate;
    }

    // The BFC block offset of this container gets resolved because of this
    // child.
    child_determined_bfc_offset = true;
    if (!ResolveBfcBlockOffset(previous_inflow_position,
                               child_bfc_offset_estimate)) {
      // If we need to abort here, it means that we had preceding unpositioned
      // floats. This is only expected if we're here for the first time.
      DCHECK(!bfc_offset_already_resolved);
      return false;
    }

    // We reset the block offset here as it may have been affected by clearance.
    child_bfc_offset_estimate = ContainerBfcOffset().block_offset;
  }

  // If the child has a non-zero block-start margin, our initial estimate will
  // be that any pending floats will be flush (block-start-wise) with this
  // child, since they are affected by margin collapsing. Furthermore, this
  // child's margin may also pull parent blocks downwards. However, this is only
  // the case if the child fits beside the floats at the current block
  // offset. If it doesn't (or if it gets clearance), the child needs to be
  // pushed down. In this case, the child's margin no longer collapses with the
  // previous margin strut, so the pending floats and parent blocks need to
  // ignore this margin, which may cause them to end up at completely different
  // positions than initially estimated. In other words, we'll need another
  // layout pass if this happens.
  bool abort_if_cleared = child_data.margins.block_start != LayoutUnit() &&
                          !child_margin_got_separated &&
                          child_determined_bfc_offset;
  NGLayoutOpportunity opportunity;
  scoped_refptr<const NGLayoutResult> layout_result;
  std::tie(layout_result, opportunity) = LayoutNewFormattingContext(
      child, child_break_token, child_data,
      {child_origin_line_offset, child_bfc_offset_estimate}, abort_if_cleared);

  if (!layout_result) {
    DCHECK(abort_if_cleared);
    // Layout got aborted, because the child got pushed down by floats, and we
    // may have had pending floats that we tentatively positioned incorrectly
    // (since the child's margin shouldn't have affected them). Try again
    // without the child's margin. So, we need another layout pass. Figure out
    // if we can do it right away from here, or if we have to roll back and
    // reposition floats first.
    if (child_determined_bfc_offset) {
      // The BFC block offset was calculated when we got to this child, with
      // the child's margin adjoining. Since that turned out to be wrong,
      // re-resolve the BFC block offset without the child's margin.
      LayoutUnit old_offset = *container_builder_.BfcBlockOffset();
      container_builder_.ResetBfcBlockOffset();

      // Re-resolving the BFC block-offset with a different "forced" BFC
      // block-offset is only safe if an ancestor *never* had clearance past
      // adjoining floats.
      DCHECK(!ConstraintSpace().AncestorHasClearancePastAdjoiningFloats());
      ResolveBfcBlockOffset(previous_inflow_position,
                            non_adjoining_bfc_offset_estimate,
                            /* forced_bfc_block_offset */ base::nullopt);

      if ((bfc_offset_already_resolved || has_adjoining_floats) &&
          old_offset != *container_builder_.BfcBlockOffset()) {
        // The first BFC block offset resolution turned out to be wrong, and we
        // positioned preceding adjacent floats based on that. Now we have to
        // roll back and position them at the correct offset. The only expected
        // incorrect estimate is with the child's margin adjoining. Any other
        // incorrect estimate will result in failed layout.
        DCHECK_EQ(old_offset, adjoining_bfc_offset_estimate);
        return false;
      }
    }

    DCHECK_GT(opportunity.rect.start_offset.block_offset,
              child_bfc_offset_estimate);
    child_bfc_offset_estimate = non_adjoining_bfc_offset_estimate;
    child_margin_got_separated = true;

    // We can re-layout the child right away. This re-layout *must* produce a
    // fragment and opportunity which fits within the exclusion space.
    std::tie(layout_result, opportunity) = LayoutNewFormattingContext(
        child, child_break_token, child_data,
        {child_origin_line_offset, child_bfc_offset_estimate},
        /* abort_if_cleared */ false);
  }

  NGFragment fragment(ConstraintSpace().GetWritingMode(),
                      layout_result->PhysicalFragment());

  // Auto-margins are applied within the layout opportunity which fits. We'll
  // pretend that computed margins are 0 here, as they have already been
  // excluded from the layout opportunity rectangle.
  NGBoxStrut auto_margins;
  if (child.IsListMarker()) {
    // Deal with marker's margin. It happens only when marker needs to occupy
    // the whole line.
    DCHECK(child.ListMarkerOccupiesWholeLine());
    // Because the marker is laid out as a normal block child, its inline size
    // is extended to fill up the space. Compute the regular marker size from
    // the first child.
    const NGPhysicalContainerFragment& marker_fragment =
        layout_result->PhysicalFragment();
    DCHECK(!marker_fragment.Children().empty());
    const NGPhysicalFragment& marker_child_fragment =
        *marker_fragment.Children().front();
    LayoutUnit marker_inline_size =
        marker_child_fragment.Size()
            .ConvertToLogical(ConstraintSpace().GetWritingMode())
            .inline_size;
    auto_margins.inline_start = NGUnpositionedListMarker(To<NGBlockNode>(child))
                                    .InlineOffset(marker_inline_size);
    auto_margins.inline_end = opportunity.rect.InlineSize() -
                              fragment.InlineSize() - auto_margins.inline_start;
  } else {
    LayoutUnit inline_size = fragment.InlineSize();
    // Negative margins are not used to determine opportunity, but need to take
    // them into account for positioning.
    LayoutUnit inline_margin = child_data.margins.InlineSum();
    if (inline_margin < 0)
      inline_size += inline_margin;
    ResolveInlineMargins(child_style, Style(), opportunity.rect.InlineSize(),
                         inline_size, &auto_margins);
  }

  LayoutUnit child_bfc_line_offset = opportunity.rect.start_offset.line_offset +
                                     auto_margins.LineLeft(direction);

  // When there are negative margins present, a new formatting context can move
  // outside its layout opportunity. This occurs when the *line-left* edge
  // hasn't been shifted by floats.
  //
  // NOTE: Firefox and EdgeHTML both match this behaviour of only considering
  // the line-left edge. WebKit also considers this line-right edge, but this
  // is slightly more complicated to implement, and probably not needed for web
  // compatibility.
  bool can_move_outside_opportunity =
      opportunity.rect.start_offset.line_offset == child_origin_line_offset;

  if (can_move_outside_opportunity) {
    child_bfc_line_offset +=
        child_data.margins.LineLeft(direction).ClampPositiveToZero();
  }

  NGBfcOffset child_bfc_offset(child_bfc_line_offset,
                               opportunity.rect.start_offset.block_offset);

  LogicalOffset logical_offset = LogicalFromBfcOffsets(
      child_bfc_offset, ContainerBfcOffset(), fragment.InlineSize(),
      container_builder_.Size().inline_size, ConstraintSpace().Direction());

  if (ConstraintSpace().HasBlockFragmentation()) {
    bool is_pushed_by_floats =
        child_margin_got_separated ||
        child_bfc_offset.block_offset > child_bfc_offset_estimate ||
        layout_result->IsPushedByFloats();
    if (BreakBeforeChild(child, *layout_result, previous_inflow_position,
                         logical_offset.block_offset, is_pushed_by_floats))
      return true;
    EBreakBetween break_after = JoinFragmentainerBreakValues(
        layout_result->FinalBreakAfter(), child.Style().BreakAfter());
    container_builder_.SetPreviousBreakAfter(break_after);
  }

  if (!PositionOrPropagateListMarker(*layout_result, &logical_offset,
                                     previous_inflow_position))
    return false;

  container_builder_.AddResult(*layout_result, logical_offset);

  // The margins we store will be used by e.g. getComputedStyle().
  // When calculating these values, ignore any floats that might have
  // affected the child. This is what Edge does.
  ResolveInlineMargins(child_style, Style(), child_available_size_.inline_size,
                       fragment.InlineSize(), &child_data.margins);
  To<NGBlockNode>(child).StoreMargins(ConstraintSpace(), child_data.margins);

  *previous_inflow_position = ComputeInflowPosition(
      *previous_inflow_position, child, child_data,
      child_bfc_offset.block_offset, logical_offset, *layout_result, fragment,
      /* self_collapsing_child_had_clearance */ false);

  return true;
}

std::pair<scoped_refptr<const NGLayoutResult>, NGLayoutOpportunity>
NGBlockLayoutAlgorithm::LayoutNewFormattingContext(
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token,
    const NGInflowChildData& child_data,
    NGBfcOffset origin_offset,
    bool abort_if_cleared) {
  // The origin offset is where we should start looking for layout
  // opportunities. It needs to be adjusted by the child's clearance.
  AdjustToClearance(
      exclusion_space_.ClearanceOffset(ResolvedClear(child.Style(), Style())),
      &origin_offset);
  DCHECK(container_builder_.BfcBlockOffset());

  // Before we lay out, figure out how much inline space we have available at
  // the start block offset estimate (the child is not allowed to overlap with
  // floats, so we need to find out how much space is used by floats at this
  // block offset). This may affect the inline size of the child, e.g. when it's
  // specified as auto, or if it's a table (with table-layout:auto). This will
  // not affect percentage resolution, because that's going to be resolved
  // against the containing block, regardless of adjacent floats. When looking
  // for space, we ignore inline margins, as they will overlap with any adjacent
  // floats.
  LayoutUnit inline_margin = child_data.margins.InlineSum();
  LayoutUnit inline_size =
      (child_available_size_.inline_size - inline_margin.ClampNegativeToZero())
          .ClampNegativeToZero();

  LayoutOpportunityVector opportunities =
      exclusion_space_.AllLayoutOpportunities(origin_offset, inline_size);

  // We should always have at least one opportunity.
  DCHECK_GT(opportunities.size(), 0u);

  // Now we lay out. This will give us a child fragment and thus its size, which
  // means that we can find out if it's actually going to fit. If it doesn't
  // fit where it was laid out, and is pushed downwards, we'll lay out over
  // again, since a new BFC block offset could result in a new fragment size,
  // e.g. when inline size is auto, or if we're block-fragmented.
  for (const auto opportunity : opportunities) {
    if (abort_if_cleared &&
        origin_offset.block_offset < opportunity.rect.BlockStartOffset()) {
      // Abort if we got pushed downwards. We need to adjust
      // origin_offset.block_offset, reposition any floats affected by that, and
      // try again.
      return std::make_pair(nullptr, opportunity);
    }

    // When the inline dimensions of layout opportunity match the available
    // space, a new formatting context can expand outside of the opportunity if
    // negative margins are present.
    bool can_expand_outside_opportunity =
        (opportunity.rect.start_offset.line_offset ==
             origin_offset.line_offset &&
         opportunity.rect.InlineSize() == inline_size);

    LayoutUnit inline_negative_margin =
        can_expand_outside_opportunity ? inline_margin.ClampPositiveToZero()
                                       : LayoutUnit();

    // The available inline size in the child constraint space needs to include
    // inline margins, since layout algorithms (both legacy and NG) will resolve
    // auto inline size by subtracting the inline margins from available inline
    // size. We have calculated a layout opportunity without margins in mind,
    // since they overlap with adjacent floats. Now we need to add them.
    LogicalSize child_available_size = {
        (opportunity.rect.InlineSize() - inline_negative_margin + inline_margin)
            .ClampNegativeToZero(),
        child_available_size_.block_size};
    NGConstraintSpace child_space = CreateConstraintSpaceForChild(
        child, child_data, child_available_size, /* is_new_fc */ true);

    // All formatting context roots (like this child) should start with an empty
    // exclusion space.
    DCHECK(child_space.ExclusionSpace().IsEmpty());

    scoped_refptr<const NGLayoutResult> layout_result =
        To<NGBlockNode>(child).Layout(child_space, child_break_token);

    // Since this child establishes a new formatting context, no exclusion space
    // should be returned.
    DCHECK(layout_result->ExclusionSpace().IsEmpty());

    NGFragment fragment(ConstraintSpace().GetWritingMode(),
                        layout_result->PhysicalFragment());

    // Now we can check if the fragment will fit in this layout opportunity.
    if ((opportunity.rect.InlineSize() >= fragment.InlineSize() ||
         opportunity.rect.InlineSize() == inline_size) &&
        opportunity.rect.BlockSize() >= fragment.BlockSize())
      return std::make_pair(std::move(layout_result), opportunity);
  }

  NOTREACHED();
  return std::make_pair(nullptr, NGLayoutOpportunity());
}

bool NGBlockLayoutAlgorithm::HandleInflow(
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token,
    NGPreviousInflowPosition* previous_inflow_position,
    NGInlineChildLayoutContext* inline_child_layout_context,
    scoped_refptr<const NGInlineBreakToken>* previous_inline_break_token) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK(!child.IsOutOfFlowPositioned());
  DCHECK(!child.CreatesNewFormattingContext());

  auto* child_inline_node = DynamicTo<NGInlineNode>(child);
  if (child_inline_node && !child_break_token &&
      RuntimeEnabledFeatures::LayoutNGLineCacheEnabled()) {
    DCHECK(!*previous_inline_break_token);
    bool aborted = false;
    *previous_inline_break_token = TryReuseFragmentsFromCache(
        *child_inline_node, previous_inflow_position, &aborted);
    if (*previous_inline_break_token)
      return true;
    if (aborted)
      return false;
  }

  bool is_non_empty_inline =
      child_inline_node && !child_inline_node->IsEmptyInline();
  bool has_clearance_past_adjoining_floats =
      !container_builder_.BfcBlockOffset() && child.IsBlock() &&
      HasClearancePastAdjoiningFloats(container_builder_.AdjoiningObjectTypes(),
                                      child.Style(), Style());

  base::Optional<LayoutUnit> forced_bfc_block_offset;

  // If we can separate the previous margin strut from what is to follow, do
  // that. Then we're able to resolve *our* BFC block offset and position any
  // pending floats. There are two situations where this is necessary:
  //  1. If the child is to be cleared by adjoining floats.
  //  2. If the child is a non-empty inline.
  //
  // Note this logic is copied to TryReuseFragmentsFromCache(), they need to
  // keep in sync.
  if (has_clearance_past_adjoining_floats || is_non_empty_inline) {
    if (!ResolveBfcBlockOffset(previous_inflow_position))
      return false;

    // If we had clearance past any adjoining floats, we already know where the
    // child is going to be (the child's margins won't have any effect).
    //
    // Set the forced BFC block-offset to the appropriate clearance offset to
    // force this placement of this child.
    if (has_clearance_past_adjoining_floats) {
      forced_bfc_block_offset = exclusion_space_.ClearanceOffset(
          ResolvedClear(child.Style(), Style()));
    }
  }

  // Perform layout on the child.
  NGInflowChildData child_data =
      ComputeChildData(*previous_inflow_position, child, child_break_token,
                       /* is_new_fc */ false);
  NGConstraintSpace child_space = CreateConstraintSpaceForChild(
      child, child_data, child_available_size_, /* is_new_fc */ false,
      forced_bfc_block_offset, has_clearance_past_adjoining_floats);
  scoped_refptr<const NGLayoutResult> layout_result = LayoutInflow(
      child_space, child_break_token, &child, inline_child_layout_context);

  // To save space of the stack when we recurse into |NGBlockNode::Layout|
  // above, the rest of this function is continued within |FinishInflow|.
  // However it should be read as one function.
  return FinishInflow(child, child_break_token, child_space,
                      has_clearance_past_adjoining_floats,
                      std::move(layout_result), &child_data,
                      previous_inflow_position, inline_child_layout_context,
                      previous_inline_break_token);
}

bool NGBlockLayoutAlgorithm::FinishInflow(
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token,
    const NGConstraintSpace& child_space,
    bool has_clearance_past_adjoining_floats,
    scoped_refptr<const NGLayoutResult> layout_result,
    NGInflowChildData* child_data,
    NGPreviousInflowPosition* previous_inflow_position,
    NGInlineChildLayoutContext* inline_child_layout_context,
    scoped_refptr<const NGInlineBreakToken>* previous_inline_break_token) {
  base::Optional<LayoutUnit> child_bfc_block_offset =
      layout_result->BfcBlockOffset();

  bool is_self_collapsing = layout_result->IsSelfCollapsing();

  // Only non self-collapsing children (e.g. "normal children") can be pushed
  // by floats in this way.
  bool normal_child_had_clearance = layout_result->IsPushedByFloats();
  DCHECK(!normal_child_had_clearance || !is_self_collapsing);

  // A child may have aborted its layout if it resolved its BFC block-offset.
  // If we don't have a BFC block-offset yet, we need to propagate the abort
  // signal up to our parent.
  if (layout_result->Status() == NGLayoutResult::kBfcBlockOffsetResolved &&
      !container_builder_.BfcBlockOffset()) {
    // There's no need to do anything apart from resolving the BFC block-offset
    // here, so make sure that it aborts before trying to position floats or
    // anything like that, which would just be waste of time.
    //
    // This is simply propagating an abort up to a node which is able to
    // restart the layout (a node that has resolved its BFC block-offset).
    DCHECK(child_bfc_block_offset);
    abort_when_bfc_block_offset_updated_ = true;

    LayoutUnit bfc_block_offset = *child_bfc_block_offset;

    if (normal_child_had_clearance) {
      // If the child has the same clearance-offset as ourselves it means that
      // we should *also* resolve ourselves at that offset, (and we also have
      // been pushed by floats).
      if (ConstraintSpace().ClearanceOffset() == child_space.ClearanceOffset())
        container_builder_.SetIsPushedByFloats();
      else
        bfc_block_offset = NextBorderEdge(*previous_inflow_position);
    }

    // A new formatting-context may have previously tried to resolve the BFC
    // block-offset. In this case we'll have a "forced" BFC block-offset
    // present, but we shouldn't apply it (instead preferring the child's new
    // BFC block-offset).
    DCHECK(!ConstraintSpace().AncestorHasClearancePastAdjoiningFloats());
    if (!ResolveBfcBlockOffset(previous_inflow_position, bfc_block_offset,
                               /* forced_bfc_block_offset */ base::nullopt))
      return false;
  }

  // We have special behaviour for a self-collapsing child which gets pushed
  // down due to clearance, see comment inside |ComputeInflowPosition|.
  bool self_collapsing_child_had_clearance =
      is_self_collapsing && has_clearance_past_adjoining_floats;

  // We try and position the child within the block formatting-context. This
  // may cause our BFC block-offset to be resolved, in which case we should
  // abort our layout if needed.
  if (!child_bfc_block_offset) {
    DCHECK(is_self_collapsing);
    if (child_space.HasClearanceOffset() &&
        child.Style().Clear() != EClear::kNone) {
      // This is a self-collapsing child that we collapsed through, so we have
      // to detect clearance manually. See if the child's hypothetical border
      // edge is past the relevant floats. If it's not, we need to apply
      // clearance before it.
      LayoutUnit child_block_offset_estimate =
          BfcBlockOffset() + layout_result->EndMarginStrut().Sum();
      if (child_block_offset_estimate < child_space.ClearanceOffset())
        self_collapsing_child_had_clearance = true;
    }
  }

  bool child_had_clearance =
      self_collapsing_child_had_clearance || normal_child_had_clearance;
  if (child_had_clearance) {
    // The child has clearance. Clearance inhibits margin collapsing and acts as
    // spacing before the block-start margin of the child. Our BFC block offset
    // is therefore resolvable, and if it hasn't already been resolved, we'll
    // do it now to separate the child's collapsed margin from this container.
    if (!ResolveBfcBlockOffset(previous_inflow_position))
      return false;
  }

  bool self_collapsing_child_needs_relayout = false;
  if (!child_bfc_block_offset) {
    // Layout wasn't able to determine the BFC block-offset of the child. This
    // has to mean that the child is self-collapsing.
    DCHECK(is_self_collapsing);

    if (container_builder_.BfcBlockOffset()) {
      // Since we know our own BFC block-offset, though, we can calculate that
      // of the child as well.
      child_bfc_block_offset = PositionSelfCollapsingChildWithParentBfc(
          child, child_space, *child_data, *layout_result);

      // We may need to relayout this child if it had any objects which were
      // positioned in the incorrect position.
      //
      // TODO(layout-dev): A more optimal version of this is to set this flag
      // only if the child tree *added* any adjoining objects which it failed
      // to position. Currently, we risk relaying out the sub-tree for no
      // reason, because we're not able to make this distinction.
      if (layout_result->AdjoiningObjectTypes() &&
          !child_space.ForcedBfcBlockOffset())
        self_collapsing_child_needs_relayout = true;
    }
  } else if (!child_had_clearance && !is_self_collapsing) {
    // Only non self-collapsing children are allowed resolve their parent's BFC
    // block-offset. We check the BFC block-offset at the end of layout
    // determine if this fragment is self-collapsing.
    //
    // The child's BFC block-offset is known, and since there's no clearance,
    // this container will get the same offset, unless it has already been
    // resolved.
    if (!ResolveBfcBlockOffset(previous_inflow_position,
                               *child_bfc_block_offset))
      return false;
  }

  // We need to re-layout a self-collapsing child if it was affected by
  // clearance in order to produce a new margin strut. For example:
  // <div style="margin-bottom: 50px;"></div>
  // <div id="float" style="height: 50px;"></div>
  // <div id="zero" style="clear: left; margin-top: -20px;">
  //   <div id="zero-inner" style="margin-top: 40px; margin-bottom: -30px;">
  // </div>
  //
  // The end margin strut for #zero will be {50, -30}. #zero will be affected
  // by clearance (as 50 > {50, -30}).
  //
  // As #zero doesn't touch the incoming margin strut now we need to perform a
  // relayout with an empty incoming margin strut.
  //
  // The resulting margin strut in the above example will be {40, -30}. See
  // |ComputeInflowPosition| for how this end margin strut is used.
  if (self_collapsing_child_had_clearance) {
    NGMarginStrut margin_strut;
    margin_strut.Append(child_data->margins.block_start,
                        child.Style().HasMarginBeforeQuirk());

    // We only need to relayout if the new margin strut is different to the
    // previous one.
    if (child_data->margin_strut != margin_strut) {
      child_data->margin_strut = margin_strut;
      self_collapsing_child_needs_relayout = true;
    }
  }

  // We need to layout a child if we know its BFC block offset and:
  //  - It aborted its layout as it resolved its BFC block offset.
  //  - It has some unpositioned floats.
  //  - It was affected by clearance.
  if ((layout_result->Status() == NGLayoutResult::kBfcBlockOffsetResolved ||
       self_collapsing_child_needs_relayout) &&
      child_bfc_block_offset) {
    NGConstraintSpace new_child_space = CreateConstraintSpaceForChild(
        child, *child_data, child_available_size_, /* is_new_fc */ false,
        child_bfc_block_offset);
    layout_result = LayoutInflow(new_child_space, child_break_token, &child,
                                 inline_child_layout_context);

    if (layout_result->Status() == NGLayoutResult::kBfcBlockOffsetResolved) {
      // Even a second layout pass may abort, if the BFC block offset initially
      // calculated turned out to be wrong. This happens when we discover that
      // an in-flow block-level descendant that establishes a new formatting
      // context doesn't fit beside the floats at its initial position. Allow
      // one more pass.
      child_bfc_block_offset = layout_result->BfcBlockOffset();
      DCHECK(child_bfc_block_offset);
      new_child_space = CreateConstraintSpaceForChild(
          child, *child_data, child_available_size_, /* is_new_fc */ false,
          child_bfc_block_offset);
      layout_result = LayoutInflow(new_child_space, child_break_token, &child,
                                   inline_child_layout_context);
    }

    DCHECK_EQ(layout_result->Status(), NGLayoutResult::kSuccess);
  }

  // It is now safe to update our version of the exclusion space, and any
  // propagated adjoining floats.
  exclusion_space_ = layout_result->ExclusionSpace();

  // Only self-collapsing children should have adjoining objects.
  DCHECK(!layout_result->AdjoiningObjectTypes() || is_self_collapsing);
  container_builder_.SetAdjoiningObjectTypes(
      layout_result->AdjoiningObjectTypes());

  // If we don't know our BFC block-offset yet, and the child stumbled into
  // something that needs it (unable to position floats yet), we need abort
  // layout, and trigger a re-layout once we manage to resolve it.
  //
  // NOTE: This check is performed after the optional second layout pass above,
  // since we may have been able to resolve our BFC block-offset (e.g. due to
  // clearance) and position any descendant floats in the second pass.
  // In particular, when it comes to clearance of self-collapsing children, if
  // we just applied it and resolved the BFC block-offset to separate the
  // margins before and after clearance, we cannot abort and re-layout this
  // child, or clearance would be lost.
  //
  // If we are a new formatting context, the child will get re-laid out once it
  // has been positioned.
  if (!container_builder_.BfcBlockOffset()) {
    abort_when_bfc_block_offset_updated_ |=
        layout_result->AdjoiningObjectTypes();
    // If our BFC block offset is unknown, and the child got pushed down by
    // floats, so will we.
    if (layout_result->IsPushedByFloats())
      container_builder_.SetIsPushedByFloats();
  }

  const auto& physical_fragment = layout_result->PhysicalFragment();
  NGFragment fragment(ConstraintSpace().GetWritingMode(), physical_fragment);

  LogicalOffset logical_offset = CalculateLogicalOffset(
      fragment, layout_result->BfcLineOffset(), child_bfc_block_offset);

  if (ConstraintSpace().HasBlockFragmentation()) {
    if (BreakBeforeChild(child, *layout_result, previous_inflow_position,
                         logical_offset.block_offset,
                         layout_result->IsPushedByFloats()))
      return true;
    EBreakBetween break_after = JoinFragmentainerBreakValues(
        layout_result->FinalBreakAfter(), child.Style().BreakAfter());
    container_builder_.SetPreviousBreakAfter(break_after);
  }

  if (!PositionOrPropagateListMarker(*layout_result, &logical_offset,
                                     previous_inflow_position))
    return false;

  container_builder_.AddResult(*layout_result, logical_offset);

  if (auto* block_child = DynamicTo<NGBlockNode>(child)) {
    // We haven't yet resolved margins wrt. overconstrainedness, unless that was
    // also required to calculate line-left offset (due to block alignment)
    // before layout. Do so now, so that we store the correct values (which is
    // required by e.g. getComputedStyle()).
    if (!child_data->margins_fully_resolved) {
      ResolveInlineMargins(child.Style(), Style(),
                           child_available_size_.inline_size,
                           fragment.InlineSize(), &child_data->margins);
      child_data->margins_fully_resolved = true;
    }

    block_child->StoreMargins(ConstraintSpace(), child_data->margins);
  }

  *previous_inflow_position = ComputeInflowPosition(
      *previous_inflow_position, child, *child_data, child_bfc_block_offset,
      logical_offset, *layout_result, fragment,
      self_collapsing_child_had_clearance);

  *previous_inline_break_token =
      child.IsInline() ? To<NGInlineBreakToken>(physical_fragment.BreakToken())
                       : nullptr;

  return true;
}

NGInflowChildData NGBlockLayoutAlgorithm::ComputeChildData(
    const NGPreviousInflowPosition& previous_inflow_position,
    NGLayoutInputNode child,
    const NGBreakToken* child_break_token,
    bool is_new_fc) {
  DCHECK(child);
  DCHECK(!child.IsFloating());
  DCHECK_EQ(is_new_fc, child.CreatesNewFormattingContext());

  // Calculate margins in parent's writing mode.
  bool margins_fully_resolved;
  NGBoxStrut margins = CalculateMargins(child, is_new_fc, child_break_token,
                                        &margins_fully_resolved);

  // Append the current margin strut with child's block start margin.
  // Non empty border/padding, and new formatting-context use cases are handled
  // inside of the child's layout
  NGMarginStrut margin_strut = previous_inflow_position.margin_strut;

  LayoutUnit logical_block_offset =
      previous_inflow_position.logical_block_offset;

  EMarginCollapse margin_before_collapse = child.Style().MarginBeforeCollapse();
  if (margin_before_collapse != EMarginCollapse::kCollapse) {
    // Stop margin collapsing on the block-start side of the child.
    StopMarginCollapsing(child.Style().MarginBeforeCollapse(),
                         margins.block_start, &logical_block_offset,
                         &margin_strut);

    if (margin_before_collapse == EMarginCollapse::kSeparate) {
      UseCounter::Count(Node().GetDocument(),
                        WebFeature::kWebkitMarginBeforeCollapseSeparate);
      if (margin_strut != previous_inflow_position.margin_strut ||
          logical_block_offset !=
              previous_inflow_position.logical_block_offset) {
        UseCounter::Count(
            Node().GetDocument(),
            WebFeature::kWebkitMarginBeforeCollapseSeparateMaybeDoesSomething);
      }
    } else if (margin_before_collapse == EMarginCollapse::kDiscard) {
      UseCounter::Count(Node().GetDocument(),
                        WebFeature::kWebkitMarginBeforeCollapseDiscard);
    }
  } else {
    margin_strut.Append(margins.block_start,
                        child.Style().HasMarginBeforeQuirk());
  }

  NGBfcOffset child_bfc_offset = {
      ConstraintSpace().BfcOffset().line_offset +
          border_scrollbar_padding_.LineLeft(ConstraintSpace().Direction()) +
          margins.LineLeft(ConstraintSpace().Direction()),
      BfcBlockOffset() + logical_block_offset};

  return {child_bfc_offset, margin_strut, margins, margins_fully_resolved};
}

NGPreviousInflowPosition NGBlockLayoutAlgorithm::ComputeInflowPosition(
    const NGPreviousInflowPosition& previous_inflow_position,
    const NGLayoutInputNode child,
    const NGInflowChildData& child_data,
    const base::Optional<LayoutUnit>& child_bfc_block_offset,
    const LogicalOffset& logical_offset,
    const NGLayoutResult& layout_result,
    const NGFragment& fragment,
    bool self_collapsing_child_had_clearance) {
  // Determine the child's end logical offset, for the next child to use.
  LayoutUnit logical_block_offset;

  bool is_self_collapsing = layout_result.IsSelfCollapsing();
  if (is_self_collapsing) {
    // The default behaviour for self-collapsing children is they just pass
    // through the previous inflow position.
    logical_block_offset = previous_inflow_position.logical_block_offset;

    if (self_collapsing_child_had_clearance) {
      // If there's clearance, we must have applied that by now and thus
      // resolved our BFC block-offset.
      DCHECK(container_builder_.BfcBlockOffset());
      DCHECK(child_bfc_block_offset.has_value());

      // If a self-collapsing child was affected by clearance (that is it got
      // pushed down past a float), we need to do something slightly bizarre.
      //
      // Instead of just passing through the previous inflow position, we make
      // the inflow position our new position (which was affected by the
      // float), minus what the margin strut which the self-collapsing child
      // produced.
      //
      // Another way of thinking about this is that when you *add* back the
      // margin strut, you end up with the same position as you started with.
      //
      // This is essentially what the spec refers to as clearance [1], and,
      // while we normally don't have to calculate it directly, in the case of
      // a self-collapsing cleared child like here, we actually have to.
      //
      // We have to calculate clearance for self-collapsing cleared children,
      // because we need the margin that's between the clearance and this block
      // to collapse correctly with subsequent content. This is something that
      // needs to take place after the margin strut preceding and following the
      // clearance have been separated. Clearance may be positive, negative or
      // zero, depending on what it takes to (hypothetically) place this child
      // just below the last relevant float. Since the margins before and after
      // the clearance have been separated, we may have to pull the child back,
      // and that's an example of negative clearance.
      //
      // (In the other case, when a cleared child is non self-collapsing (i.e.
      // when we don't end up here), we don't need to explicitly calculate
      // clearance, because then we just place its border edge where it should
      // be and we're done with it.)
      //
      // [1] https://www.w3.org/TR/CSS22/visuren.html#flow-control

      // First move past the margin that is to precede the clearance. It will
      // not participate in any subsequent margin collapsing.
      LayoutUnit margin_before_clearance =
          previous_inflow_position.margin_strut.Sum();
      logical_block_offset += margin_before_clearance;

      // Calculate and apply actual clearance.
      LayoutUnit clearance = *child_bfc_block_offset -
                             layout_result.EndMarginStrut().Sum() -
                             NextBorderEdge(previous_inflow_position);
      logical_block_offset += clearance;
    }
    if (!container_builder_.BfcBlockOffset())
      DCHECK_EQ(logical_block_offset, LayoutUnit());
  } else {
    logical_block_offset = logical_offset.block_offset + fragment.BlockSize();
  }

  NGMarginStrut margin_strut = layout_result.EndMarginStrut();

  EMarginCollapse margin_after_collapse = child.Style().MarginAfterCollapse();
  if (margin_after_collapse != EMarginCollapse::kCollapse) {
    LayoutUnit logical_block_offset_copy = logical_block_offset;
    // Stop margin collapsing on the block-end side of the child.
    StopMarginCollapsing(margin_after_collapse, child_data.margins.block_end,
                         &logical_block_offset, &margin_strut);

    if (margin_after_collapse == EMarginCollapse::kSeparate) {
      UseCounter::Count(Node().GetDocument(),
                        WebFeature::kWebkitMarginAfterCollapseSeparate);
      if (margin_strut != layout_result.EndMarginStrut() ||
          logical_block_offset != logical_block_offset_copy) {
        UseCounter::Count(
            Node().GetDocument(),
            WebFeature::kWebkitMarginAfterCollapseSeparateMaybeDoesSomething);
      }
    } else if (margin_after_collapse == EMarginCollapse::kDiscard) {
      UseCounter::Count(Node().GetDocument(),
                        WebFeature::kWebkitMarginAfterCollapseDiscard);
    }
  } else {
    // Self collapsing child's end margin can "inherit" quirkiness from its
    // start margin. E.g.
    // <ol style="margin-bottom: 20px"></ol>
    bool is_quirky =
        (is_self_collapsing && child.Style().HasMarginBeforeQuirk()) ||
        child.Style().HasMarginAfterQuirk();
    margin_strut.Append(child_data.margins.block_end, is_quirky);
  }

  // This flag is subtle, but in order to determine our size correctly we need
  // to check if our last child is self-collapsing, and it was affected by
  // clearance *or* an adjoining self-collapsing sibling was affected by
  // clearance. E.g.
  // <div id="container">
  //   <div id="float"></div>
  //   <div id="zero-with-clearance"></div>
  //   <div id="another-zero"></div>
  // </div>
  // In the above case #container's size will depend on the end margin strut of
  // #another-zero, even though usually it wouldn't.
  bool self_or_sibling_self_collapsing_child_had_clearance =
      self_collapsing_child_had_clearance ||
      (previous_inflow_position.self_collapsing_child_had_clearance &&
       is_self_collapsing);

  return {logical_block_offset, margin_strut,
          self_or_sibling_self_collapsing_child_had_clearance};
}

LayoutUnit NGBlockLayoutAlgorithm::PositionSelfCollapsingChildWithParentBfc(
    const NGLayoutInputNode& child,
    const NGConstraintSpace& child_space,
    const NGInflowChildData& child_data,
    const NGLayoutResult& layout_result) const {
  DCHECK(layout_result.IsSelfCollapsing());

  // The child must be an in-flow zero-block-size fragment, use its end margin
  // strut for positioning.
  LayoutUnit child_bfc_block_offset =
      child_data.bfc_offset_estimate.block_offset +
      layout_result.EndMarginStrut().Sum();

  ApplyClearance(child_space, &child_bfc_block_offset);

  return child_bfc_block_offset;
}

LayoutUnit NGBlockLayoutAlgorithm::FragmentainerSpaceAvailable() const {
  DCHECK(container_builder_.BfcBlockOffset());
  return ConstraintSpace().FragmentainerSpaceAtBfcStart() -
         *container_builder_.BfcBlockOffset();
}

bool NGBlockLayoutAlgorithm::IsFragmentainerOutOfSpace(
    LayoutUnit block_offset) const {
  if (!ConstraintSpace().HasBlockFragmentation())
    return false;
  if (!container_builder_.BfcBlockOffset().has_value())
    return false;
  return block_offset >= FragmentainerSpaceAvailable();
}

void NGBlockLayoutAlgorithm::FinalizeForFragmentation() {
  if (first_overflowing_line_ && !fit_all_lines_) {
    // A line box overflowed the fragmentainer, but we continued layout anyway,
    // in order to determine where to break in order to honor the widows
    // request. We never got around to actually breaking, before we ran out of
    // lines. So do it now.
    intrinsic_block_size_ = FragmentainerSpaceAvailable();
    container_builder_.SetDidBreak();
  }

  LayoutUnit used_block_size =
      BreakToken() ? BreakToken()->UsedBlockSize() : LayoutUnit();
  LayoutUnit block_size =
      ComputeBlockSizeForFragment(ConstraintSpace(), Node(), border_padding_,
                                  used_block_size + intrinsic_block_size_);

  block_size -= used_block_size;
  DCHECK_GE(block_size, LayoutUnit())
      << "Adding and subtracting the used_block_size shouldn't leave the "
         "block_size for this fragment smaller than zero.";

  LayoutUnit space_left = FragmentainerSpaceAvailable();

  if (space_left <= LayoutUnit()) {
    // The amount of space available may be zero, or even negative, if the
    // border-start edge of this block starts exactly at, or even after the
    // fragmentainer boundary. We're going to need a break before this block,
    // because no part of it fits in the current fragmentainer. Due to margin
    // collapsing with children, this situation is something that we cannot
    // always detect prior to layout. The fragment produced by this algorithm is
    // going to be thrown away. The parent layout algorithm will eventually
    // detect that there's no room for a fragment for this node, and drop the
    // fragment on the floor. Therefore it doesn't matter how we set up the
    // container builder, so just return.
    return;
  }

  if (container_builder_.DidBreak()) {
    // One of our children broke. Even if we fit within the remaining space we
    // need to prepare a break token.
    container_builder_.SetUsedBlockSize(std::min(space_left, block_size) +
                                        used_block_size);
    container_builder_.SetBlockSize(std::min(space_left, block_size));
    container_builder_.SetIntrinsicBlockSize(space_left);

    if (first_overflowing_line_) {
      int line_number;
      if (fit_all_lines_) {
        line_number = first_overflowing_line_;
      } else {
        // We managed to finish layout of all the lines for the node, which
        // means that we won't have enough widows, unless we break earlier than
        // where we overflowed.
        int line_count = container_builder_.LineCount();
        line_number = std::max(line_count - Style().Widows(),
                               std::min(line_count, int(Style().Orphans())));
      }
      container_builder_.AddBreakBeforeLine(line_number);
    }
    return;
  }

  if (block_size > space_left) {
    // Need a break inside this block.
    container_builder_.SetUsedBlockSize(space_left + used_block_size);
    container_builder_.SetDidBreak();
    container_builder_.SetBlockSize(space_left);
    container_builder_.SetIntrinsicBlockSize(space_left);
    container_builder_.PropagateSpaceShortage(block_size - space_left);
    return;
  }

  // The end of the block fits in the current fragmentainer.
  container_builder_.SetUsedBlockSize(used_block_size + block_size);
  container_builder_.SetBlockSize(block_size);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
}

bool NGBlockLayoutAlgorithm::BreakBeforeChild(
    NGLayoutInputNode child,
    const NGLayoutResult& layout_result,
    NGPreviousInflowPosition* previous_inflow_position,
    LayoutUnit block_offset,
    bool is_pushed_by_floats) {
  DCHECK(ConstraintSpace().HasBlockFragmentation());
  BreakType break_type = BreakTypeBeforeChild(
      child, layout_result, block_offset, is_pushed_by_floats);
  if (break_type == NoBreak)
    return false;

  LayoutUnit space_available = FragmentainerSpaceAvailable();
  LayoutUnit space_shortage;
  if (layout_result.MinimalSpaceShortage() == LayoutUnit::Max()) {
    // Calculate space shortage: Figure out how much more space would have been
    // sufficient to make the child fit right here in the current fragment.
    NGFragment fragment(ConstraintSpace().GetWritingMode(),
                        layout_result.PhysicalFragment());
    LayoutUnit space_left = space_available - block_offset;
    space_shortage = fragment.BlockSize() - space_left;
  } else {
    // However, if space shortage was reported inside the child, use that. If we
    // broke inside the child, we didn't complete layout, so calculating space
    // shortage for the child as a whole would be impossible and pointless.
    space_shortage = layout_result.MinimalSpaceShortage();
  }

  if (child.IsInline()) {
    DCHECK_EQ(break_type, SoftBreak);
    if (!first_overflowing_line_) {
      // We're at the first overflowing line. This is the space shortage that
      // we are going to report. We do this in spite of not yet knowing
      // whether breaking here would violate orphans and widows requests. This
      // approach may result in a lower space shortage than what's actually
      // true, which leads to more layout passes than we'd otherwise
      // need. However, getting this optimal for orphans and widows would
      // require an additional piece of machinery. This case should be rare
      // enough (to worry about performance), so let's focus on code
      // simplicity instead.
      container_builder_.PropagateSpaceShortage(space_shortage);
    }
    // Attempt to honor orphans and widows requests.
    if (int line_count = container_builder_.LineCount()) {
      if (!first_overflowing_line_)
        first_overflowing_line_ = line_count;
      bool is_first_fragment = !BreakToken();
      // Figure out how many lines we need before the break. That entails to
      // attempt to honor the orphans request.
      int minimum_line_count = Style().Orphans();
      if (!is_first_fragment) {
        // If this isn't the first fragment, it means that there's a break both
        // before and after this fragment. So what was seen as trailing widows
        // in the previous fragment is essentially orphans for us now.
        minimum_line_count =
            std::max(minimum_line_count, static_cast<int>(Style().Widows()));
      }
      if (line_count < minimum_line_count) {
        if (is_first_fragment) {
          // Not enough orphans. Our only hope is if we can break before the
          // start of this block to improve on the situation. That's not
          // something we can determine at this point though. Permit the break,
          // but mark it as undesirable.
          container_builder_.SetHasLastResortBreak();
        }
        // We're already failing with orphans, so don't even try to deal with
        // widows.
        fit_all_lines_ = true;
      } else {
        // There are enough lines before the break. Try to make sure that
        // there'll be enough lines after the break as well. Attempt to honor
        // the widows request.
        DCHECK_GE(line_count, first_overflowing_line_);
        int widows_found = line_count - first_overflowing_line_ + 1;
        if (widows_found < Style().Widows()) {
          // Although we're out of space, we have to continue layout to figure
          // out exactly where to break in order to honor the widows
          // request. We'll make sure that we're going to leave at least as many
          // lines as specified by the 'widows' property for the next fragment
          // (if at all possible), which means that lines that could fit in the
          // current fragment (that we have already laid out) may have to be
          // saved for the next fragment.
          return false;
        } else {
          // We have determined that there are plenty of lines for the next
          // fragment, so we can just break exactly where we ran out of space,
          // rather than pushing some of the line boxes over to the next
          // fragment.
          fit_all_lines_ = true;
        }
      }
    }
  }

  if (!has_processed_first_child_ &&
      (container_builder_.IsPushedByFloats() || !is_pushed_by_floats)) {
    // We're breaking before the first piece of in-flow content inside this
    // block, even if it's not a valid class C break point [1] in this case. We
    // really don't want to break here, if we can find something better. A class
    // C break point occurs if a first child has been pushed by floats, but this
    // only applies to the outermost block that gets pushed (in case this parent
    // and the child have adjoining top margins).
    //
    // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
    container_builder_.SetHasLastResortBreak();
  }

  // The remaining part of the fragmentainer (the unusable space for child
  // content, due to the break) should still be occupied by this container.
  // TODO(mstensho): Figure out if we really need to <0 here. It doesn't seem
  // right to have negative available space.
  previous_inflow_position->logical_block_offset =
      space_available.ClampNegativeToZero();
  // Drop the fragment on the floor and retry at the start of the next
  // fragmentainer.
  container_builder_.AddBreakBeforeChild(child);
  container_builder_.SetDidBreak();
  if (break_type == ForcedBreak) {
    container_builder_.SetHasForcedBreak();
  } else {
    // Report space shortage, unless we're at a line box (in that case we've
    // already dealt with it further up).
    if (!child.IsInline()) {
      // TODO(mstensho): Turn this into a DCHECK, when the engine is ready for
      // it. Space shortage should really be positive here, or we might
      // ultimately fail to stretch the columns (column balancing).
      if (space_shortage > LayoutUnit())
        container_builder_.PropagateSpaceShortage(space_shortage);
    }
  }
  return true;
}

NGBlockLayoutAlgorithm::BreakType NGBlockLayoutAlgorithm::BreakTypeBeforeChild(
    NGLayoutInputNode child,
    const NGLayoutResult& layout_result,
    LayoutUnit block_offset,
    bool is_pushed_by_floats) const {
  if (!container_builder_.BfcBlockOffset().has_value())
    return NoBreak;

  const NGPhysicalContainerFragment& physical_fragment =
      layout_result.PhysicalFragment();

  // If we haven't used any space at all in the fragmentainer yet, we cannot
  // break, or there'd be no progress. We'd end up creating an infinite number
  // of fragmentainers without putting any content into them.
  auto space_left = FragmentainerSpaceAvailable() - block_offset;
  if (space_left >= ConstraintSpace().FragmentainerBlockSize())
    return NoBreak;

  if (child.IsInline()) {
    NGFragment fragment(ConstraintSpace().GetWritingMode(), physical_fragment);
    return fragment.BlockSize() > space_left ? SoftBreak : NoBreak;
  }

  EBreakBetween break_before = JoinFragmentainerBreakValues(
      child.Style().BreakBefore(), layout_result.InitialBreakBefore());
  EBreakBetween break_between =
      container_builder_.JoinedBreakBetweenValue(break_before);
  if (IsForcedBreakValue(ConstraintSpace(), break_between)) {
    // There should be a forced break before this child, and if we're not at the
    // first in-flow child, just go ahead and break.
    if (has_processed_first_child_)
      return ForcedBreak;
  }

  // If the block offset is past the fragmentainer boundary (or exactly at the
  // boundary), no part of the fragment is going to fit in the current
  // fragmentainer. Fragments may be pushed past the fragmentainer boundary by
  // margins.
  if (space_left <= LayoutUnit())
    return SoftBreak;

  const NGBreakToken* token = physical_fragment.BreakToken();
  if (!token || token->IsFinished())
    return NoBreak;
  auto* block_break_token = DynamicTo<NGBlockBreakToken>(token);
  if (block_break_token && block_break_token->HasLastResortBreak()) {
    // We've already found a place to break inside the child, but it wasn't an
    // optimal one, because it would violate some rules for breaking. Consider
    // breaking before this child instead, but only do so if it's at a valid
    // break point. It's a valid break point if we're between siblings, or if
    // it's a first child at a class C break point [1] (if it got pushed down by
    // floats). The break we've already found has been marked as a last-resort
    // break, but moving that last-resort break to an earlier (but equally bad)
    // last-resort break would just waste fragmentainer space and slow down
    // content progression.
    //
    // [1] https://www.w3.org/TR/css-break-3/#possible-breaks
    if (has_processed_first_child_ || is_pushed_by_floats) {
      // This is a valid break point, and we can resolve the last-resort
      // situation.
      return SoftBreak;
    }
  }
  // TODO(mstensho): There are other break-inside values to consider here.
  if (child.Style().BreakInside() != EBreakInside::kAvoid)
    return NoBreak;

  // The child broke, and we're not at the start of a fragmentainer, and we're
  // supposed to avoid breaking inside the child.
  DCHECK(IsFirstFragment(ConstraintSpace(), physical_fragment));
  return SoftBreak;
}

NGBoxStrut NGBlockLayoutAlgorithm::CalculateMargins(
    NGLayoutInputNode child,
    bool is_new_fc,
    const NGBreakToken* child_break_token,
    bool* margins_fully_resolved) {
  // We need to at least partially resolve margins before creating a constraint
  // space for layout. Layout needs to know the line-left offset before
  // starting. If the line-left offset cannot be calculated without fully
  // resolving the margins (because of block alignment), we have to create a
  // temporary constraint space now to figure out the inline size first. In all
  // other cases we'll postpone full resolution until after child layout, when
  // we actually have a child constraint space to use (and know the inline
  // size).
  *margins_fully_resolved = false;

  DCHECK(child);
  if (child.IsInline())
    return {};
  const ComputedStyle& child_style = child.Style();
  bool needs_inline_size =
      NeedsInlineSizeToResolveLineLeft(child_style, Style());
  if (!needs_inline_size && !child_style.MayHaveMargin())
    return {};

  NGBoxStrut margins = ComputeMarginsFor(
      child_style, child_percentage_size_.inline_size,
      ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction());
  if (ShouldIgnoreBlockStartMargin(ConstraintSpace(), child, child_break_token))
    margins.block_start = LayoutUnit();

  // As long as the child isn't establishing a new formatting context, we need
  // to know its line-left offset before layout, to be able to position child
  // floats correctly. If we need to resolve auto margins or other alignment
  // properties to calculate the line-left offset, we also need to calculate its
  // inline size first.
  if (!is_new_fc && needs_inline_size) {
    NGConstraintSpace space =
        NGConstraintSpaceBuilder(ConstraintSpace(),
                                 child_style.GetWritingMode(),
                                 /* is_new_fc */ false)
            .SetAvailableSize(child_available_size_)
            .SetPercentageResolutionSize(child_percentage_size_)
            .ToConstraintSpace();

    NGBoxStrut child_border_padding =
        ComputeBorders(space, child) + ComputePadding(space, child.Style());
    LayoutUnit child_inline_size =
        ComputeInlineSizeForFragment(space, child, child_border_padding);

    ResolveInlineMargins(child_style, Style(),
                         space.AvailableSize().inline_size, child_inline_size,
                         &margins);
    *margins_fully_resolved = true;
  }
  return margins;
}

NGConstraintSpace NGBlockLayoutAlgorithm::CreateConstraintSpaceForChild(
    const NGLayoutInputNode child,
    const NGInflowChildData& child_data,
    const LogicalSize child_available_size,
    bool is_new_fc,
    const base::Optional<LayoutUnit> forced_bfc_block_offset,
    bool has_clearance_past_adjoining_floats) {
  const ComputedStyle& style = Style();
  const ComputedStyle& child_style = child.Style();
  WritingMode child_writing_mode =
      child.IsInline() ? style.GetWritingMode() : child_style.GetWritingMode();

  NGConstraintSpaceBuilder builder(ConstraintSpace(), child_writing_mode,
                                   is_new_fc);
  SetOrthogonalFallbackInlineSizeIfNeeded(Style(), child, &builder);

  if (!IsParallelWritingMode(ConstraintSpace().GetWritingMode(),
                             child_writing_mode))
    builder.SetIsShrinkToFit(child_style.LogicalWidth().IsAuto());

  builder.SetAvailableSize(child_available_size)
      .SetPercentageResolutionSize(child_percentage_size_)
      .SetReplacedPercentageResolutionSize(replaced_child_percentage_size_);

  if (Node().IsTableCell()) {
    // If we have a fixed block-size we are in the "layout" phase.
    builder.SetTableCellChildLayoutPhase(
        ConstraintSpace().IsFixedSizeBlock()
            ? NGTableCellChildLayoutPhase::kLayout
            : NGTableCellChildLayoutPhase::kMeasure);

    if (Node().IsRestrictedBlockSizeTableCell())
      builder.SetIsInRestrictedBlockSizeTableCell();
  }

  if (NGBaseline::ShouldPropagateBaselines(child))
    builder.AddBaselineRequests(ConstraintSpace().BaselineRequests());

  builder.SetBfcOffset(child_data.bfc_offset_estimate)
      .SetMarginStrut(child_data.margin_strut);

  bool has_bfc_block_offset = container_builder_.BfcBlockOffset().has_value();

  // Propagate the |NGConstraintSpace::ForcedBfcBlockOffset| down to our
  // children.
  if (!has_bfc_block_offset && ConstraintSpace().ForcedBfcBlockOffset())
    builder.SetForcedBfcBlockOffset(*ConstraintSpace().ForcedBfcBlockOffset());
  if (forced_bfc_block_offset)
    builder.SetForcedBfcBlockOffset(*forced_bfc_block_offset);

  if (has_bfc_block_offset && child.IsBlock()) {
    // Typically we aren't allowed to look at the previous layout result within
    // a layout algorithm. However this is fine (honest), as it is just a hint
    // to the child algorithm for where floats should be placed. If it doesn't
    // have this flag, or gets this estimate wrong, it'll relayout with the
    // appropriate "forced" BFC block-offset.
    if (const NGLayoutResult* previous_result =
            child.GetLayoutBox()->GetCachedLayoutResult()) {
      const NGConstraintSpace& prev_space =
          previous_result->GetConstraintSpaceForCaching();

      // To increase the hit-rate we adjust the previous "optimistic"/"forced"
      // BFC block-offset by how much the child has shifted from the previous
      // layout.
      LayoutUnit bfc_block_delta = child_data.bfc_offset_estimate.block_offset -
                                   prev_space.BfcOffset().block_offset;
      if (prev_space.ForcedBfcBlockOffset()) {
        builder.SetOptimisticBfcBlockOffset(*prev_space.ForcedBfcBlockOffset() +
                                            bfc_block_delta);
      } else if (prev_space.OptimisticBfcBlockOffset()) {
        builder.SetOptimisticBfcBlockOffset(
            *prev_space.OptimisticBfcBlockOffset() + bfc_block_delta);
      }
    }
  } else if (ConstraintSpace().OptimisticBfcBlockOffset()) {
    // Propagate the |NGConstraintSpace::OptimisticBfcBlockOffset| down to our
    // children.
    builder.SetOptimisticBfcBlockOffset(
        *ConstraintSpace().OptimisticBfcBlockOffset());
  }

  // Propagate the |NGConstraintSpace::AncestorHasClearancePastAdjoiningFloats|
  // flag down to our children.
  if (!has_bfc_block_offset &&
      ConstraintSpace().AncestorHasClearancePastAdjoiningFloats())
    builder.SetAncestorHasClearancePastAdjoiningFloats();
  if (has_clearance_past_adjoining_floats)
    builder.SetAncestorHasClearancePastAdjoiningFloats();

  LayoutUnit clearance_offset = ConstraintSpace().IsNewFormattingContext()
                                    ? LayoutUnit::Min()
                                    : ConstraintSpace().ClearanceOffset();
  if (child.IsBlock()) {
    LayoutUnit child_clearance_offset =
        exclusion_space_.ClearanceOffset(ResolvedClear(child_style, Style()));
    clearance_offset = std::max(clearance_offset, child_clearance_offset);
    builder.SetTextDirection(child_style.Direction());

    // PositionListMarker() requires a first line baseline.
    if (container_builder_.UnpositionedListMarker()) {
      builder.AddBaselineRequest(
          {NGBaselineAlgorithmType::kFirstLine, style.GetFontBaseline()});
    }
  } else {
    builder.SetTextDirection(style.Direction());
  }
  builder.SetClearanceOffset(clearance_offset);

  if (!is_new_fc) {
    builder.SetExclusionSpace(exclusion_space_);
    if (!has_bfc_block_offset) {
      builder.SetAdjoiningObjectTypes(
          container_builder_.AdjoiningObjectTypes());
    }
  }

  if (ConstraintSpace().HasBlockFragmentation()) {
    LayoutUnit new_bfc_block_offset;
    // If a block establishes a new formatting context, we must know our
    // position in the formatting context, to be able to adjust the
    // fragmentation line.
    if (is_new_fc)
      new_bfc_block_offset = child_data.bfc_offset_estimate.block_offset;
    SetupFragmentation(ConstraintSpace(), new_bfc_block_offset, &builder);
  }

  return builder.ToConstraintSpace();
}

LayoutUnit NGBlockLayoutAlgorithm::ComputeLineBoxBaselineOffset(
    const NGBaselineRequest& request,
    const NGPhysicalLineBoxFragment& line_box,
    LayoutUnit line_box_block_offset) const {
  NGLineHeightMetrics metrics =
      line_box.BaselineMetrics(request.BaselineType());
  DCHECK(!metrics.IsEmpty());

  // NGLineHeightMetrics is line-relative, which matches to the flow-relative
  // unless this box is in flipped-lines writing-mode.
  if (!Style().IsFlippedLinesWritingMode())
    return metrics.ascent + line_box_block_offset;

  if (Node().IsInlineLevel()) {
    // If this box is inline-level, since we're in NGBlockLayoutAlgorithm, this
    // is an inline-block.
    DCHECK(Node().IsAtomicInlineLevel());
    // This box will be flipped when the containing line is flipped. Compute the
    // baseline offset from the block-end (right in vertical-lr) content edge.
    line_box_block_offset = container_builder_.Size().block_size -
                            (line_box_block_offset + line_box.Size().width);
    return metrics.ascent + line_box_block_offset;
  }

  // Otherwise, the baseline is offset by the descent from the block-start
  // content edge.
  return metrics.descent + line_box_block_offset;
}

// Add a baseline from a child box fragment.
// @return false if the specified child is not a box or is OOF.
bool NGBlockLayoutAlgorithm::AddBaseline(const NGBaselineRequest& request,
                                         const NGPhysicalFragment& child,
                                         LayoutUnit child_offset) {
  if (child.IsLineBox()) {
    const auto& line_box = To<NGPhysicalLineBoxFragment>(child);

    // Skip over a line-box which is empty. These don't have any baselines which
    // should be added.
    if (line_box.IsEmptyLineBox())
      return false;

    LayoutUnit offset =
        ComputeLineBoxBaselineOffset(request, line_box, child_offset);
    container_builder_.AddBaseline(request, offset);
    return true;
  }

  if (child.IsFloatingOrOutOfFlowPositioned())
    return false;

  if (const auto* box = DynamicTo<NGPhysicalBoxFragment>(child)) {
    if (base::Optional<LayoutUnit> baseline = box->Baseline(request)) {
      container_builder_.AddBaseline(request, *baseline + child_offset);
      return true;
    }
  }

  return false;
}

// Propagate computed baselines from children.
// Skip children that do not produce baselines (e.g., empty blocks.)
void NGBlockLayoutAlgorithm::PropagateBaselinesFromChildren() {
  const NGBaselineRequestList requests = ConstraintSpace().BaselineRequests();
  if (requests.IsEmpty())
    return;

  for (const auto& request : requests) {
    switch (request.AlgorithmType()) {
      case NGBaselineAlgorithmType::kAtomicInline: {
        if (Node().UseLogicalBottomMarginEdgeForInlineBlockBaseline()) {
          LayoutUnit block_end = container_builder_.BlockSize();
          NGBoxStrut margins =
              ComputeMarginsForSelf(ConstraintSpace(), Style());
          container_builder_.AddBaseline(request,
                                         block_end + margins.block_end);
          break;
        }

        const auto& children = container_builder_.Children();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
          if (AddBaseline(request, *it->fragment, it->offset.block_offset))
            break;
        }
        break;
      }
      case NGBaselineAlgorithmType::kFirstLine:
        for (const auto& child : container_builder_.Children()) {
          if (AddBaseline(request, *child.fragment, child.offset.block_offset))
            break;
        }
        break;
    }
  }
}

bool NGBlockLayoutAlgorithm::ResolveBfcBlockOffset(
    NGPreviousInflowPosition* previous_inflow_position,
    LayoutUnit bfc_block_offset,
    base::Optional<LayoutUnit> forced_bfc_block_offset) {
  if (container_builder_.BfcBlockOffset())
    return true;

  bfc_block_offset = forced_bfc_block_offset.value_or(bfc_block_offset);

  if (ApplyClearance(ConstraintSpace(), &bfc_block_offset))
    container_builder_.SetIsPushedByFloats();

  container_builder_.SetBfcBlockOffset(bfc_block_offset);

  if (NeedsAbortOnBfcBlockOffsetChange())
    return false;

  // Reset the previous inflow position. Clear the margin strut and set the
  // offset to our block-start border edge.
  //
  // We'll now end up at the block-start border edge. If the BFC block offset
  // was resolved due to a block-start border or padding, that must be added by
  // the caller, for subsequent layout to continue at the right position.
  // Whether we need to add border+padding or not isn't something we should
  // determine here, so it must be dealt with as part of initializing the
  // layout algorithm.
  previous_inflow_position->logical_block_offset = LayoutUnit();
  previous_inflow_position->margin_strut = NGMarginStrut();

  return true;
}

bool NGBlockLayoutAlgorithm::NeedsAbortOnBfcBlockOffsetChange() const {
  DCHECK(container_builder_.BfcBlockOffset());
  if (!abort_when_bfc_block_offset_updated_)
    return false;

  // If our position differs from our (potentially optimistic) estimate, abort.
  return *container_builder_.BfcBlockOffset() !=
         ConstraintSpace().ExpectedBfcBlockOffset();
}

LayoutUnit NGBlockLayoutAlgorithm::CalculateMinimumBlockSize(
    const NGMarginStrut& end_margin_strut) {
  if (!Node().IsQuirkyAndFillsViewport())
    return kIndefiniteSize;

  if (!Style().LogicalHeight().IsAuto())
    return kIndefiniteSize;

  NGBoxStrut margins = ComputeMarginsForSelf(ConstraintSpace(), Style());
  LayoutUnit margin_sum;
  if (Node().CreatesNewFormattingContext()) {
    margin_sum = margins.BlockSum();
  } else {
    DCHECK(Node().IsBody());
    if (container_builder_.BfcBlockOffset()) {
      NGMarginStrut body_strut = end_margin_strut;
      body_strut.Append(margins.block_end, Style().HasMarginAfterQuirk());
      margin_sum = *container_builder_.BfcBlockOffset() -
                   ConstraintSpace().BfcOffset().block_offset +
                   body_strut.Sum();
    } else {
      // The |end_margin_strut| is the block-start margin if the body doesn't
      // have a BFC block-offset.
      margin_sum = end_margin_strut.Sum() + margins.block_end;
    }
  }

  return (ConstraintSpace().AvailableSize().block_size - margin_sum)
      .ClampNegativeToZero();
}

bool NGBlockLayoutAlgorithm::PositionOrPropagateListMarker(
    const NGLayoutResult& layout_result,
    LogicalOffset* content_offset,
    NGPreviousInflowPosition* previous_inflow_position) {
  // If this is not a list-item, propagate unpositioned list markers to
  // ancestors.
  if (!node_.IsListItem()) {
    if (layout_result.UnpositionedListMarker()) {
      DCHECK(!container_builder_.UnpositionedListMarker());
      container_builder_.SetUnpositionedListMarker(
          layout_result.UnpositionedListMarker());
    }
    return true;
  }

  // If this is a list item, add the unpositioned list marker as a child.
  NGUnpositionedListMarker list_marker = layout_result.UnpositionedListMarker();
  if (!list_marker) {
    list_marker = container_builder_.UnpositionedListMarker();
    if (!list_marker)
      return true;
    container_builder_.SetUnpositionedListMarker(NGUnpositionedListMarker());
  }

  NGLineHeightMetrics content_metrics;
  const NGConstraintSpace& space = ConstraintSpace();
  const NGPhysicalFragment& content = layout_result.PhysicalFragment();
  FontBaseline baseline_type = Style().GetFontBaseline();
  if (list_marker.CanAddToBox(space, baseline_type, content,
                              &content_metrics)) {
    // TODO: We are reusing the ConstraintSpace for LI here. It works well for
    // now because authors cannot style list-markers currently. If we want to
    // support `::marker` pseudo, we need to create ConstraintSpace for marker
    // separately.
    scoped_refptr<const NGLayoutResult> marker_layout_result =
        list_marker.Layout(space, container_builder_.Style(), baseline_type);
    DCHECK(marker_layout_result);
    // If the BFC block-offset of li is still not resolved, resolved it now.
    if (!container_builder_.BfcBlockOffset() &&
        marker_layout_result->BfcBlockOffset()) {
      // TODO: Currently the margin-top of marker is always zero. To support
      // `::marker` pseudo, we should count marker's margin-top in.
#if DCHECK_IS_ON()
      list_marker.CheckMargin();
#endif
      if (!ResolveBfcBlockOffset(previous_inflow_position))
        return false;
    }

    list_marker.AddToBox(space, baseline_type, content,
                         border_scrollbar_padding_, content_metrics,
                         *marker_layout_result, content_offset,
                         &container_builder_);
    return true;
  }

  // If the list marker could not be positioned against this child because it
  // does not have the baseline to align to, keep it as unpositioned and try
  // the next child.
  container_builder_.SetUnpositionedListMarker(list_marker);
  return true;
}

bool NGBlockLayoutAlgorithm::PositionListMarkerWithoutLineBoxes(
    NGPreviousInflowPosition* previous_inflow_position) {
  DCHECK(node_.IsListItem());
  DCHECK(container_builder_.UnpositionedListMarker());

  NGUnpositionedListMarker list_marker =
      container_builder_.UnpositionedListMarker();
  const NGConstraintSpace& space = ConstraintSpace();
  FontBaseline baseline_type = Style().GetFontBaseline();
  // Layout the list marker.
  scoped_refptr<const NGLayoutResult> marker_layout_result =
      list_marker.Layout(space, container_builder_.Style(), baseline_type);
  DCHECK(marker_layout_result);
  // If the BFC block-offset of li is still not resolved, resolve it now.
  if (!container_builder_.BfcBlockOffset() &&
      marker_layout_result->BfcBlockOffset()) {
    // TODO: Currently the margin-top of marker is always zero. To support
    // `::marker` pseudo, we should count marker's margin-top in.
#if DCHECK_IS_ON()
    list_marker.CheckMargin();
#endif
    if (!ResolveBfcBlockOffset(previous_inflow_position))
      return false;
  }
  // Position the list marker without aligning to line boxes.
  LayoutUnit marker_block_size = list_marker.AddToBoxWithoutLineBoxes(
      space, baseline_type, *marker_layout_result, &container_builder_);
  container_builder_.SetUnpositionedListMarker(NGUnpositionedListMarker());

  // Whether the list marker should affect the block size or not is not
  // well-defined, but 3 out of 4 impls do.
  // https://github.com/w3c/csswg-drafts/issues/2418
  //
  // The BFC block-offset has been resolved after layout marker. We'll always
  // include the marker into the block-size.
  if (container_builder_.BfcBlockOffset()) {
    intrinsic_block_size_ = std::max(marker_block_size, intrinsic_block_size_);
    container_builder_.SetIntrinsicBlockSize(intrinsic_block_size_);
    container_builder_.SetBlockSize(
        std::max(marker_block_size, container_builder_.Size().block_size));
  }
  return true;
}

}  // namespace blink
