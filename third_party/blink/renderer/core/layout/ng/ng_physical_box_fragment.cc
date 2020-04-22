// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"

namespace blink {

namespace {

struct SameSizeAsNGPhysicalBoxFragment : NGPhysicalContainerFragment {
  LayoutUnit baseline;
  LayoutUnit last_baseline;
  NGLink children[];
};

static_assert(sizeof(NGPhysicalBoxFragment) ==
                  sizeof(SameSizeAsNGPhysicalBoxFragment),
              "NGPhysicalBoxFragment should stay small");

bool HasControlClip(const NGPhysicalBoxFragment& self) {
  const LayoutBox* box = ToLayoutBoxOrNull(self.GetLayoutObject());
  return box && box->HasControlClip();
}

LayoutUnit BorderWidth(unsigned edges, unsigned edge, float border_width) {
  return (edges & edge) ? LayoutUnit(border_width) : LayoutUnit();
}

}  // namespace

scoped_refptr<const NGPhysicalBoxFragment> NGPhysicalBoxFragment::Create(
    NGBoxFragmentBuilder* builder,
    WritingMode block_or_line_writing_mode) {
  const NGPhysicalBoxStrut borders =
      builder->initial_fragment_geometry_->border.ConvertToPhysical(
          builder->GetWritingMode(), builder->Direction());
  const NGPhysicalBoxStrut padding =
      builder->initial_fragment_geometry_->padding.ConvertToPhysical(
          builder->GetWritingMode(), builder->Direction());
  size_t byte_size = sizeof(NGPhysicalBoxFragment) +
                     sizeof(NGLink) * builder->children_.size() +
                     (borders.IsZero() ? 0 : sizeof(borders)) +
                     (padding.IsZero() ? 0 : sizeof(padding));
  if (const NGFragmentItemsBuilder* items_builder = builder->ItemsBuilder())
    byte_size += NGFragmentItems::ByteSizeFor(items_builder->Size());
  // We store the children list inline in the fragment as a flexible
  // array. Therefore, we need to make sure to allocate enough space for
  // that array here, which requires a manual allocation + placement new.
  // The initialization of the array is done by NGPhysicalContainerFragment;
  // we pass the buffer as a constructor argument.
  void* data = ::WTF::Partitions::FastMalloc(
      byte_size, ::WTF::GetStringWithTypeName<NGPhysicalBoxFragment>());
  new (data) NGPhysicalBoxFragment(PassKey(), builder, borders, padding,
                                   block_or_line_writing_mode);
  return base::AdoptRef(static_cast<NGPhysicalBoxFragment*>(data));
}

NGPhysicalBoxFragment::NGPhysicalBoxFragment(
    PassKey key,
    NGBoxFragmentBuilder* builder,
    const NGPhysicalBoxStrut& borders,
    const NGPhysicalBoxStrut& padding,
    WritingMode block_or_line_writing_mode)
    : NGPhysicalContainerFragment(builder,
                                  block_or_line_writing_mode,
                                  children_,
                                  kFragmentBox,
                                  builder->BoxType()) {
  DCHECK(layout_object_);
  DCHECK(layout_object_->IsBoxModelObject());
  if (NGFragmentItemsBuilder* items_builder = builder->ItemsBuilder()) {
    has_fragment_items_ = true;
    NGFragmentItems* items =
        const_cast<NGFragmentItems*>(ComputeItemsAddress());
    items_builder->ToFragmentItems(block_or_line_writing_mode,
                                   builder->Direction(), Size(), items);
  } else {
    has_fragment_items_ = false;
  }
  has_borders_ = !borders.IsZero();
  if (has_borders_)
    *const_cast<NGPhysicalBoxStrut*>(ComputeBordersAddress()) = borders;
  has_padding_ = !padding.IsZero();
  if (has_padding_)
    *const_cast<NGPhysicalBoxStrut*>(ComputePaddingAddress()) = padding;
  is_first_for_node_ = builder->is_first_for_node_;
  is_fieldset_container_ = builder->is_fieldset_container_;
  is_legacy_layout_root_ = builder->is_legacy_layout_root_;
  is_painted_atomically_ =
      builder->space_ && builder->space_->IsPaintedAtomically();
  border_edge_ = builder->border_edges_.ToPhysical(builder->GetWritingMode());
  is_inline_formatting_context_ = builder->is_inline_formatting_context_;
  is_math_fraction_ = builder->is_math_fraction_;

  bool has_layout_containment = layout_object_->ShouldApplyLayoutContainment();
  if (builder->baseline_.has_value() && !has_layout_containment) {
    has_baseline_ = true;
    baseline_ = *builder->baseline_;
  } else {
    has_baseline_ = false;
    baseline_ = LayoutUnit::Min();
  }
  if (builder->last_baseline_.has_value() && !has_layout_containment) {
    has_last_baseline_ = true;
    last_baseline_ = *builder->last_baseline_;
  } else {
    has_last_baseline_ = false;
    last_baseline_ = LayoutUnit::Min();
  }

#if DCHECK_IS_ON()
  CheckIntegrity();
#endif
}

scoped_refptr<const NGLayoutResult>
NGPhysicalBoxFragment::CloneAsHiddenForPaint() const {
  const ComputedStyle& style = Style();
  NGBoxFragmentBuilder builder(GetMutableLayoutObject(), &style,
                               style.GetWritingMode(), style.Direction());
  builder.SetBoxType(BoxType());
  NGFragmentGeometry initial_fragment_geometry{
      Size().ConvertToLogical(style.GetWritingMode())};
  builder.SetInitialFragmentGeometry(initial_fragment_geometry);
  builder.SetIsHiddenForPaint(true);
  return builder.ToBoxFragment();
}

bool NGPhysicalBoxFragment::HasSelfPaintingLayer() const {
  if (!IsCSSBox())
    return false;
  SECURITY_DCHECK(GetLayoutObject() && GetLayoutObject()->IsBoxModelObject());
  return (static_cast<const LayoutBoxModelObject*>(GetLayoutObject()))
      ->HasSelfPaintingLayer();
}

PhysicalRect NGPhysicalBoxFragment::OverflowClipRect(
    const PhysicalOffset& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  DCHECK(GetLayoutObject() && GetLayoutObject()->IsBox());
  const LayoutBox* box = ToLayoutBox(GetLayoutObject());
  return box->OverflowClipRect(location, overlay_scrollbar_clip_behavior);
}

PhysicalRect NGPhysicalBoxFragment::ScrollableOverflow() const {
  DCHECK(GetLayoutObject());
  const LayoutObject* layout_object = GetLayoutObject();
  if (layout_object->IsBox()) {
    if (HasOverflowClip())
      return PhysicalRect({}, Size());
    // Legacy is the source of truth for overflow
    return PhysicalRect(ToLayoutBox(layout_object)->LayoutOverflowRect());
  } else if (layout_object->IsLayoutInline()) {
    // Inline overflow is a union of child overflows.
    PhysicalRect overflow({}, Size());
    WritingMode container_writing_mode = Style().GetWritingMode();
    TextDirection container_direction = Style().Direction();
    for (const auto& child_fragment : Children()) {
      PhysicalRect child_overflow =
          child_fragment->ScrollableOverflowForPropagation(*this);
      if (child_fragment->Style() != Style()) {
        PhysicalOffset relative_offset = ComputeRelativeOffset(
            child_fragment->Style(), container_writing_mode,
            container_direction, Size());
        child_overflow.offset += relative_offset;
      }
      child_overflow.offset += child_fragment.Offset();
      overflow.Unite(child_overflow);
    }
    return overflow;
  } else {
    NOTREACHED();
  }
  return PhysicalRect({}, Size());
}

PhysicalRect NGPhysicalBoxFragment::ScrollableOverflowFromChildren() const {
  const NGFragmentItems* items = Items();
  if (Children().empty() && !items)
    return PhysicalRect();

  // Internal struct to share logic between child fragments and child items.
  // - Inline children's overflow expands by padding end/after.
  // - Float / OOF overflow is added as is.
  // - Children not reachable by scroll overflow do not contribute to it.
  struct ComputeOverflowContext {
    ComputeOverflowContext(const NGPhysicalBoxFragment& container)
        : container(container),
          style(container.Style()),
          writing_mode(style.GetWritingMode()),
          direction(style.Direction()),
          border_inline_start(LayoutUnit(style.BorderStartWidth())),
          border_block_start(LayoutUnit(style.BorderBeforeWidth())) {
      DCHECK_EQ(&style, container.GetLayoutObject()->Style(
                            container.UsesFirstLineStyle()));

      // End and under padding are added to scroll overflow of inline children.
      // https://github.com/w3c/csswg-drafts/issues/129
      DCHECK_EQ(container.HasOverflowClip(),
                container.GetLayoutObject()->HasOverflowClip());
      if (container.HasOverflowClip()) {
        const LayoutBox* layout_object =
            ToLayoutBox(container.GetLayoutObject());
        padding_strut = NGBoxStrut(LayoutUnit(), layout_object->PaddingEnd(),
                                   LayoutUnit(), layout_object->PaddingAfter())
                            .ConvertToPhysical(writing_mode, direction);
      }
    }

    // Rectangles not reachable by scroll should not be added to overflow.
    bool IsRectReachableByScroll(const PhysicalRect& rect) {
      LogicalOffset rect_logical_end =
          rect.offset.ConvertToLogical(writing_mode, direction,
                                       container.Size(), rect.size) +
          rect.size.ConvertToLogical(writing_mode);
      return rect_logical_end.inline_offset > border_inline_start &&
             rect_logical_end.block_offset > border_block_start;
    }

    void AddChild(const PhysicalRect& child_scrollable_overflow) {
      // Do not add overflow if fragment is not reachable by scrolling.
      if (IsRectReachableByScroll(child_scrollable_overflow))
        children_overflow.Unite(child_scrollable_overflow);
    }

    void AddFloatingOrOutOfFlowPositionedChild(
        const NGPhysicalFragment& child,
        const PhysicalOffset& child_offset) {
      DCHECK(child.IsFloatingOrOutOfFlowPositioned());
      PhysicalRect child_scrollable_overflow =
          child.ScrollableOverflowForPropagation(container);
      child_scrollable_overflow.offset += ComputeRelativeOffset(
          child.Style(), writing_mode, direction, container.Size());
      child_scrollable_overflow.offset += child_offset;
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxChild(const NGPhysicalLineBoxFragment& child,
                         const PhysicalOffset& child_offset) {
      if (padding_strut)
        AddLineBoxRect({child_offset, child.Size()});
      PhysicalRect child_scrollable_overflow =
          child.ScrollableOverflow(container, style);
      child_scrollable_overflow.offset += child_offset;
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxChild(const NGFragmentItem& child,
                         const NGInlineCursor& cursor) {
      DCHECK_EQ(&child, cursor.CurrentItem());
      DCHECK_EQ(child.Type(), NGFragmentItem::kLine);
      if (padding_strut)
        AddLineBoxRect(child.RectInContainerBlock());
      const NGPhysicalLineBoxFragment* line_box = child.LineBoxFragment();
      DCHECK(line_box);
      PhysicalRect child_scrollable_overflow =
          line_box->ScrollableOverflowForLine(container, style, child, cursor);
      AddChild(child_scrollable_overflow);
    }

    void AddLineBoxRect(const PhysicalRect& linebox_rect) {
      DCHECK(padding_strut);
      if (lineboxes_enclosing_rect)
        lineboxes_enclosing_rect->Unite(linebox_rect);
      else
        lineboxes_enclosing_rect = linebox_rect;
    }

    void AddPaddingToLineBoxChildren() {
      if (lineboxes_enclosing_rect) {
        DCHECK(padding_strut);
        lineboxes_enclosing_rect->Expand(*padding_strut);
        AddChild(*lineboxes_enclosing_rect);
      }
    }

    const NGPhysicalBoxFragment& container;
    const ComputedStyle& style;
    const WritingMode writing_mode;
    const TextDirection direction;
    const LayoutUnit border_inline_start;
    const LayoutUnit border_block_start;
    base::Optional<NGPhysicalBoxStrut> padding_strut;
    base::Optional<PhysicalRect> lineboxes_enclosing_rect;
    PhysicalRect children_overflow;
  } context(*this);

  // Traverse child items.
  if (items) {
    for (NGInlineCursor cursor(*items); cursor;
         cursor.MoveToNextSkippingChildren()) {
      const NGFragmentItem* item = cursor.CurrentItem();
      if (item->Type() == NGFragmentItem::kLine) {
        context.AddLineBoxChild(*item, cursor);
        continue;
      }

      if (const NGPhysicalBoxFragment* child_box = item->BoxFragment()) {
        if (child_box->IsFloatingOrOutOfFlowPositioned()) {
          context.AddFloatingOrOutOfFlowPositionedChild(
              *child_box, item->OffsetInContainerBlock());
        }
      }
    }
  }

  // Traverse child fragments.
  const bool children_inline = IsInlineFormattingContext();
  // Only add overflow for fragments NG has not reflected into Legacy.
  // These fragments are:
  // - inline fragments,
  // - out of flow fragments whose css container is inline box.
  // TODO(layout-dev) Transforms also need to be applied to compute overflow
  // correctly. NG is not yet transform-aware. crbug.com/855965
  for (const auto& child : Children()) {
    if (child->IsFloatingOrOutOfFlowPositioned()) {
      context.AddFloatingOrOutOfFlowPositionedChild(*child, child.Offset());
    } else if (children_inline && child->IsLineBox()) {
      context.AddLineBoxChild(To<NGPhysicalLineBoxFragment>(*child),
                              child.Offset());
    }
  }

  context.AddPaddingToLineBoxChildren();

  return context.children_overflow;
}

LayoutSize NGPhysicalBoxFragment::PixelSnappedScrolledContentOffset() const {
  DCHECK(GetLayoutObject() && GetLayoutObject()->IsBox());
  const LayoutBox* box = ToLayoutBox(GetLayoutObject());
  return box->PixelSnappedScrolledContentOffset();
}

PhysicalSize NGPhysicalBoxFragment::ScrollSize() const {
  DCHECK(GetLayoutObject() && GetLayoutObject()->IsBox());
  const LayoutBox* box = ToLayoutBox(GetLayoutObject());
  return {box->ScrollWidth(), box->ScrollHeight()};
}

PhysicalRect NGPhysicalBoxFragment::ComputeSelfInkOverflow() const {
  CheckCanUpdateInkOverflow();
  const ComputedStyle& style = Style();
  PhysicalRect ink_overflow({}, Size().ToLayoutSize());

  DCHECK(GetLayoutObject());
  if (style.HasVisualOverflowingEffect()) {
    ink_overflow.Expand(style.BoxDecorationOutsets());
    if (NGOutlineUtils::HasPaintedOutline(style,
                                          GetLayoutObject()->GetNode()) &&
        NGOutlineUtils::ShouldPaintOutline(*this)) {
      Vector<PhysicalRect> outline_rects;
      // The result rects are in coordinates of this object's border box.
      AddSelfOutlineRects(
          PhysicalOffset(),
          GetLayoutObject()->OutlineRectsShouldIncludeBlockVisualOverflow(),
          &outline_rects);
      PhysicalRect rect = UnionRectEvenIfEmpty(outline_rects);
      rect.Inflate(LayoutUnit(style.OutlineOutsetExtent()));
      ink_overflow.Unite(rect);
    }
  }
  return ink_overflow;
}

void NGPhysicalBoxFragment::AddSelfOutlineRects(
    const PhysicalOffset& additional_offset,
    NGOutlineType outline_type,
    Vector<PhysicalRect>* outline_rects) const {
  if (!NGOutlineUtils::ShouldPaintOutline(*this))
    return;

  if (IsInlineBox()) {
    const LayoutObject* layout_object = GetLayoutObject();
    DCHECK(layout_object);
    DCHECK(layout_object->IsLayoutInline());
    Vector<PhysicalRect> blockflow_outline_rects =
        layout_object->OutlineRects(PhysicalOffset(), outline_type);
    // The rectangles returned are offset from the containing block. We need the
    // offset from this fragment.
    if (blockflow_outline_rects.size() > 0) {
      PhysicalOffset first_fragment_offset = blockflow_outline_rects[0].offset;
      PhysicalOffset corrected_offset =
          additional_offset - first_fragment_offset;
      for (auto& outline : blockflow_outline_rects) {
        // Skip if both width and height are zero. Containing blocks in empty
        // linebox is one such case.
        if (outline.size.IsZero())
          continue;
        outline.Move(corrected_offset);
        outline_rects->push_back(outline);
      }
    }
    return;
  }

  // For anonymous blocks, the children add outline rects.
  if (!IsAnonymousBlock())
    outline_rects->emplace_back(additional_offset, Size().ToLayoutSize());

  if (outline_type == NGOutlineType::kIncludeBlockVisualOverflow &&
      !HasOverflowClip() && !HasControlClip(*this)) {
    // Tricky code ahead: we pass a 0,0 additional_offset to
    // AddOutlineRectsForNormalChildren, and add it in after the call.
    // This is necessary because AddOutlineRectsForNormalChildren expects
    // additional_offset to be an offset from containing_block.
    // Since containing_block is our layout object, offset must be 0,0.
    // https://crbug.com/968019
    Vector<PhysicalRect> children_rects;
    AddOutlineRectsForNormalChildren(&children_rects, PhysicalOffset(),
                                     outline_type,
                                     ToLayoutBoxModelObject(GetLayoutObject()));
    if (!additional_offset.IsZero()) {
      for (auto& rect : children_rects)
        rect.offset += additional_offset;
    }
    outline_rects->AppendVector(children_rects);
    // LayoutBlock::AddOutlineRects also adds out of flow objects here.
    // In LayoutNG out of flow objects are not part of the outline.
  }
  // TODO(kojii): Needs inline_element_continuation logic from
  // LayoutBlockFlow::AddOutlineRects?
}

UBiDiLevel NGPhysicalBoxFragment::BidiLevel() const {
  // TODO(xiaochengh): Make the implementation more efficient.
  DCHECK(IsInline());
  DCHECK(IsAtomicInline());
  const auto& inline_items = InlineItemsOfContainingBlock();
  const NGInlineItem* self_item =
      std::find_if(inline_items.begin(), inline_items.end(),
                   [this](const NGInlineItem& item) {
                     return GetLayoutObject() == item.GetLayoutObject();
                   });
  DCHECK(self_item);
  DCHECK_NE(self_item, inline_items.end());
  return self_item->BidiLevel();
}

NGPixelSnappedPhysicalBoxStrut NGPhysicalBoxFragment::BorderWidths() const {
  unsigned edges = BorderEdges();
  NGPhysicalBoxStrut box_strut(
      BorderWidth(edges, NGBorderEdges::kTop, Style().BorderTopWidth()),
      BorderWidth(edges, NGBorderEdges::kRight, Style().BorderRightWidth()),
      BorderWidth(edges, NGBorderEdges::kBottom, Style().BorderBottomWidth()),
      BorderWidth(edges, NGBorderEdges::kLeft, Style().BorderLeftWidth()));
  return box_strut.SnapToDevicePixels();
}

#if DCHECK_IS_ON()
void NGPhysicalBoxFragment::CheckSameForSimplifiedLayout(
    const NGPhysicalBoxFragment& other,
    bool check_same_block_size) const {
  DCHECK_EQ(layout_object_, other.layout_object_);

  LogicalSize size = size_.ConvertToLogical(Style().GetWritingMode());
  LogicalSize other_size =
      other.size_.ConvertToLogical(Style().GetWritingMode());
  DCHECK_EQ(size.inline_size, other_size.inline_size);
  if (check_same_block_size)
    DCHECK_EQ(size.block_size, other_size.block_size);

  // "simplified" layout doesn't work within a fragmentation context.
  DCHECK(!break_token_ && !other.break_token_);

  DCHECK_EQ(type_, other.type_);
  DCHECK_EQ(sub_type_, other.sub_type_);
  DCHECK_EQ(style_variant_, other.style_variant_);

  // |has_floating_descendants_for_paint_| can change during simplified layout.
  DCHECK_EQ(has_orthogonal_flow_roots_, other.has_orthogonal_flow_roots_);
  DCHECK_EQ(may_have_descendant_above_block_start_,
            other.may_have_descendant_above_block_start_);
  DCHECK_EQ(depends_on_percentage_block_size_,
            other.depends_on_percentage_block_size_);

  DCHECK_EQ(is_inline_formatting_context_, other.is_inline_formatting_context_);
  DCHECK_EQ(is_fieldset_container_, other.is_fieldset_container_);
  DCHECK_EQ(is_legacy_layout_root_, other.is_legacy_layout_root_);
  DCHECK_EQ(is_painted_atomically_, other.is_painted_atomically_);
  DCHECK_EQ(border_edge_, other.border_edge_);

  // The oof_positioned_descendants_ vector can change during "simplified"
  // layout. This occurs when an OOF-descendant changes from "fixed" to
  // "absolute" (or visa versa) changing its containing block.

  // Legacy layout can (incorrectly) shift baseline position(s) during
  // "simplified" layout.
  DCHECK(IsLegacyLayoutRoot() || Baseline() == other.Baseline());
  DCHECK(IsLegacyLayoutRoot() || LastBaseline() == other.LastBaseline());
  DCHECK(Borders() == other.Borders());
  DCHECK(Padding() == other.Padding());
}

// Check our flags represent the actual children correctly.
void NGPhysicalBoxFragment::CheckIntegrity() const {
  bool has_inflow_blocks = false;
  bool has_inlines = false;
  bool has_line_boxes = false;
  bool has_floats = false;
  bool has_list_markers = false;

  for (const NGLink& child : Children()) {
    if (child->IsFloating())
      has_floats = true;
    else if (child->IsOutOfFlowPositioned())
      ;  // OOF can be in the fragment tree regardless of |HasItems|.
    else if (child->IsLineBox())
      has_line_boxes = true;
    else if (child->IsListMarker())
      has_list_markers = true;
    else if (child->IsInline())
      has_inlines = true;
    else
      has_inflow_blocks = true;
  }

  // If we have line boxes, |IsInlineFormattingContext()| is true, but the
  // reverse is not always true.
  if (has_line_boxes || has_inlines)
    DCHECK(IsInlineFormattingContext());

  // If display-locked, we may not have any children.
  DCHECK(layout_object_);
  if (layout_object_ && layout_object_->PaintBlockedByDisplayLock(
                            DisplayLockLifecycleTarget::kChildren))
    return;

  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    if (RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled()) {
      if (has_line_boxes)
        DCHECK(HasItems());
    } else {
      DCHECK_EQ(HasItems(), has_line_boxes);
    }

    if (has_line_boxes) {
      DCHECK(!has_inlines);
      DCHECK(!has_inflow_blocks);
      // Following objects should be in the items, not in the tree.
      DCHECK(!has_floats);
      DCHECK(!has_list_markers);
    }
  }
}
#endif

}  // namespace blink
