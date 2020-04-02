// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_truncator.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_box_state.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

bool IsLeftMostOffset(const ShapeResult& shape_result, unsigned offset) {
  if (shape_result.Rtl())
    return offset == shape_result.NumCharacters();
  return offset == 0;
}

bool IsRightMostOffset(const ShapeResult& shape_result, unsigned offset) {
  if (shape_result.Rtl())
    return offset == 0;
  return offset == shape_result.NumCharacters();
}

}  // namespace

NGLineTruncator::NGLineTruncator(const NGLineInfo& line_info)
    : line_style_(&line_info.LineStyle()),
      available_width_(line_info.AvailableWidth() - line_info.TextIndent()),
      line_direction_(line_info.BaseDirection()) {}

const ComputedStyle& NGLineTruncator::EllipsisStyle() const {
  // The ellipsis is styled according to the line style.
  // https://drafts.csswg.org/css-ui/#ellipsing-details
  return *line_style_;
}

void NGLineTruncator::SetupEllipsis() {
  const Font& font = EllipsisStyle().GetFont();
  ellipsis_font_data_ = font.PrimaryFont();
  DCHECK(ellipsis_font_data_);
  ellipsis_text_ =
      ellipsis_font_data_ && ellipsis_font_data_->GlyphForCharacter(
                                 kHorizontalEllipsisCharacter)
          ? String(&kHorizontalEllipsisCharacter, 1)
          : String(u"...");
  HarfBuzzShaper shaper(ellipsis_text_);
  ellipsis_shape_result_ =
      ShapeResultView::Create(shaper.Shape(&font, line_direction_).get());
  ellipsis_width_ = ellipsis_shape_result_->SnappedWidth();
}

LayoutUnit NGLineTruncator::PlaceEllipsisNextTo(
    NGLineBoxFragmentBuilder::ChildList* line_box,
    NGLineBoxFragmentBuilder::Child* ellipsized_child) {
  // Create the ellipsis, associating it with the ellipsized child.
  DCHECK(ellipsized_child->HasInFlowFragment());
  LayoutObject* ellipsized_layout_object =
      ellipsized_child->PhysicalFragment()->GetMutableLayoutObject();
  DCHECK(ellipsized_layout_object);
  DCHECK(ellipsized_layout_object->IsInline());
  DCHECK(ellipsized_layout_object->IsText() ||
         ellipsized_layout_object->IsAtomicInlineLevel());
  NGTextFragmentBuilder builder(line_style_->GetWritingMode());
  builder.SetText(ellipsized_layout_object, ellipsis_text_, &EllipsisStyle(),
                  true /* is_ellipsis_style */,
                  std::move(ellipsis_shape_result_));

  // Now the offset of the ellpisis is determined. Place the ellpisis into the
  // line box.
  LayoutUnit ellipsis_inline_offset =
      IsLtr(line_direction_)
          ? ellipsized_child->InlineOffset() + ellipsized_child->inline_size
          : ellipsized_child->InlineOffset() - ellipsis_width_;
  LayoutUnit ellpisis_ascent;
  DCHECK(ellipsis_font_data_);
  if (ellipsis_font_data_) {
    FontBaseline baseline_type = line_style_->GetFontBaseline();
    NGLineHeightMetrics ellipsis_metrics(ellipsis_font_data_->GetFontMetrics(),
                                         baseline_type);
    ellpisis_ascent = ellipsis_metrics.ascent;
  }
  line_box->AddChild(builder.ToTextFragment(),
                     LogicalOffset{ellipsis_inline_offset, -ellpisis_ascent},
                     ellipsis_width_, 0);
  return ellipsis_inline_offset;
}

wtf_size_t NGLineTruncator::AddTruncatedChild(
    wtf_size_t source_index,
    bool leave_one_character,
    LayoutUnit position,
    TextDirection edge,
    NGLineBoxFragmentBuilder::ChildList* line_box,
    NGInlineLayoutStateStack* box_states) {
  NGLineBoxFragmentBuilder::ChildList& line = *line_box;

  scoped_refptr<ShapeResult> shape_result =
      line[source_index].fragment->TextShapeResult()->CreateShapeResult();
  unsigned text_offset = shape_result->OffsetToFit(position, edge);
  if (IsLtr(edge) ? IsLeftMostOffset(*shape_result, text_offset)
                  : IsRightMostOffset(*shape_result, text_offset)) {
    if (!leave_one_character)
      return kDidNotAddChild;
    text_offset =
        shape_result->OffsetToFit(shape_result->PositionForOffset(
                                      IsRtl(edge) == shape_result->Rtl()
                                          ? 1
                                          : shape_result->NumCharacters() - 1),
                                  edge);
  }

  const auto& fragment = line[source_index].fragment;
  const bool keep_start = edge == fragment->ResolvedDirection();
  scoped_refptr<const NGPhysicalTextFragment> truncated_fragment =
      keep_start ? fragment->TrimText(fragment->StartOffset(),
                                      fragment->StartOffset() + text_offset)
                 : fragment->TrimText(fragment->StartOffset() + text_offset,
                                      fragment->EndOffset());
  wtf_size_t new_index = line.size();
  line.AddChild();
  box_states->ChildInserted(new_index);
  line[new_index] = line[source_index];
  line[new_index].inline_size = line_style_->IsHorizontalWritingMode()
                                    ? truncated_fragment->Size().width
                                    : truncated_fragment->Size().height;
  line[new_index].fragment = std::move(truncated_fragment);
  return new_index;
}

LayoutUnit NGLineTruncator::TruncateLine(
    LayoutUnit line_width,
    NGLineBoxFragmentBuilder::ChildList* line_box,
    NGInlineLayoutStateStack* box_states) {
  // Shape the ellipsis and compute its inline size.
  SetupEllipsis();

  // Loop children from the logical last to the logical first to determine where
  // to place the ellipsis. Children maybe truncated or moved as part of the
  // process.
  NGLineBoxFragmentBuilder::Child* ellipsized_child = nullptr;
  scoped_refptr<const NGPhysicalTextFragment> truncated_fragment;
  if (IsLtr(line_direction_)) {
    NGLineBoxFragmentBuilder::Child* first_child = line_box->FirstInFlowChild();
    for (auto it = line_box->rbegin(); it != line_box->rend(); it++) {
      auto& child = *it;
      if (EllipsizeChild(line_width, ellipsis_width_, &child == first_child,
                         &child, &truncated_fragment)) {
        ellipsized_child = &child;
        break;
      }
    }
  } else {
    NGLineBoxFragmentBuilder::Child* first_child = line_box->LastInFlowChild();
    for (auto& child : *line_box) {
      if (EllipsizeChild(line_width, ellipsis_width_, &child == first_child,
                         &child, &truncated_fragment)) {
        ellipsized_child = &child;
        break;
      }
    }
  }

  // Abort if ellipsis could not be placed.
  if (!ellipsized_child)
    return line_width;

  // Truncate the text fragment if needed.
  if (truncated_fragment) {
    DCHECK(ellipsized_child->fragment);
    // In order to preserve layout information before truncated, hide the
    // original fragment and insert a truncated one.
    size_t child_index_to_truncate = ellipsized_child - line_box->begin();
    line_box->InsertChild(child_index_to_truncate + 1);
    box_states->ChildInserted(child_index_to_truncate + 1);
    NGLineBoxFragmentBuilder::Child* child_to_truncate =
        &(*line_box)[child_index_to_truncate];
    ellipsized_child = std::next(child_to_truncate);
    *ellipsized_child = *child_to_truncate;
    HideChild(child_to_truncate);
    LayoutUnit new_inline_size = line_style_->IsHorizontalWritingMode()
                                     ? truncated_fragment->Size().width
                                     : truncated_fragment->Size().height;
    DCHECK_LE(new_inline_size, ellipsized_child->inline_size);
    if (UNLIKELY(IsRtl(line_direction_))) {
      ellipsized_child->rect.offset.inline_offset +=
          ellipsized_child->inline_size - new_inline_size;
    }
    ellipsized_child->inline_size = new_inline_size;
    ellipsized_child->fragment = std::move(truncated_fragment);
  }

  // Create the ellipsis, associating it with the ellipsized child.
  LayoutUnit ellipsis_inline_offset =
      PlaceEllipsisNextTo(line_box, ellipsized_child);
  return std::max(ellipsis_inline_offset + ellipsis_width_, line_width);
}

// This function was designed to work only with <input type=file>.
// We assume the line box contains:
//     (Optional) children without in-flow fragments
//     Children with in-flow fragments, and
//     (Optional) children without in-flow fragments
// in this order, and the children with in-flow fragments have no padding,
// no border, and no margin.
// Children with IsPlaceholder() can appear anywhere.
LayoutUnit NGLineTruncator::TruncateLineInTheMiddle(
    LayoutUnit line_width,
    NGLineBoxFragmentBuilder::ChildList* line_box,
    NGInlineLayoutStateStack* box_states) {
  // Shape the ellipsis and compute its inline size.
  SetupEllipsis();

  NGLineBoxFragmentBuilder::ChildList& line = *line_box;
  wtf_size_t initial_index_left = kNotFound;
  wtf_size_t initial_index_right = kNotFound;
  for (wtf_size_t i = 0; i < line_box->size(); ++i) {
    auto& child = line[i];
    if (!child.fragment && child.IsPlaceholder())
      continue;
    if (child.HasOutOfFlowFragment() || !child.fragment ||
        !child.fragment->TextShapeResult()) {
      if (initial_index_right != kNotFound)
        break;
      continue;
    }
    if (initial_index_left == kNotFound)
      initial_index_left = i;
    initial_index_right = i;
  }
  // There are no truncatable children.
  if (initial_index_left == kNotFound)
    return line_width;
  DCHECK_NE(initial_index_right, kNotFound);
  DCHECK(line[initial_index_left].HasInFlowFragment());
  DCHECK(line[initial_index_right].HasInFlowFragment());

  // line[]:
  //     s s s p f f p f f s s
  //             ^       ^
  // initial_index_left  |
  //                     initial_index_right
  //   s: child without in-flow fragment
  //   p: placeholder child
  //   f: child with in-flow fragment

  const LayoutUnit static_width_left = line[initial_index_left].InlineOffset();
  LayoutUnit static_width_right = LayoutUnit(0);
  for (wtf_size_t i = initial_index_right + 1; i < line.size(); ++i)
    static_width_right += line[i].inline_size;
  const LayoutUnit available_width =
      available_width_ - static_width_left - static_width_right;
  if (available_width <= ellipsis_width_)
    return line_width;
  LayoutUnit available_width_left = (available_width - ellipsis_width_) / 2;
  LayoutUnit available_width_right = available_width_left;

  // Children for ellipsis and truncated fragments will have index which
  // is >= new_child_start.
  const wtf_size_t new_child_start = line.size();

  wtf_size_t index_left = initial_index_left;
  wtf_size_t index_right = initial_index_right;

  if (IsLtr(line_direction_)) {
    // Find truncation point at the left, truncate, and add an ellipsis.
    while (available_width_left >= line[index_left].inline_size)
      available_width_left -= line[index_left++].inline_size;
    DCHECK_LE(index_left, index_right);
    DCHECK(!line[index_left].IsPlaceholder());
    wtf_size_t new_index = AddTruncatedChild(
        index_left, index_left == initial_index_left, available_width_left,
        TextDirection::kLtr, line_box, box_states);
    if (new_index == kDidNotAddChild) {
      DCHECK_GT(index_left, initial_index_left);
      DCHECK_GT(index_left, 0u);
      wtf_size_t i = index_left;
      while (!line[--i].HasInFlowFragment())
        DCHECK(line[i].IsPlaceholder());
      PlaceEllipsisNextTo(line_box, &line[i]);
      available_width_right += available_width_left;
    } else {
      PlaceEllipsisNextTo(line_box, &line[new_index]);
      available_width_right +=
          available_width_left - line[new_index].inline_size;
    }

    // Find truncation point at the right.
    while (available_width_right >= line[index_right].inline_size)
      available_width_right -= line[index_right--].inline_size;
    LayoutUnit new_modified_right_offset =
        line[line.size() - 1].InlineOffset() + ellipsis_width_;
    DCHECK_LE(index_left, index_right);
    DCHECK(!line[index_right].IsPlaceholder());
    if (available_width_right > 0) {
      new_index = AddTruncatedChild(
          index_right, false,
          line[index_right].inline_size - available_width_right,
          TextDirection::kRtl, line_box, box_states);
      if (new_index != kDidNotAddChild) {
        line[new_index].rect.offset.inline_offset = new_modified_right_offset;
        new_modified_right_offset += line[new_index].inline_size;
      }
    }
    // Shift unchanged children at the right of the truncated child.
    // It's ok to modify existing children's offsets because they are not
    // web-exposed.
    LayoutUnit offset_diff = line[index_right].InlineOffset() +
                             line[index_right].inline_size -
                             new_modified_right_offset;
    for (wtf_size_t i = index_right + 1; i < new_child_start; ++i)
      line[i].rect.offset.inline_offset -= offset_diff;
    line_width -= offset_diff;

  } else {
    // Find truncation point at the right, truncate, and add an ellipsis.
    while (available_width_right >= line[index_right].inline_size)
      available_width_right -= line[index_right--].inline_size;
    DCHECK_LE(index_left, index_right);
    DCHECK(!line[index_right].IsPlaceholder());
    wtf_size_t new_index =
        AddTruncatedChild(index_right, index_right == initial_index_right,
                          line[index_right].inline_size - available_width_right,
                          TextDirection::kRtl, line_box, box_states);
    if (new_index == kDidNotAddChild) {
      DCHECK_LT(index_right, initial_index_right);
      wtf_size_t i = index_right;
      while (!line[++i].HasInFlowFragment())
        DCHECK(line[i].IsPlaceholder());
      PlaceEllipsisNextTo(line_box, &line[i]);
      available_width_left += available_width_right;
    } else {
      line[new_index].rect.offset.inline_offset +=
          line[index_right].inline_size - line[new_index].inline_size;
      PlaceEllipsisNextTo(line_box, &line[new_index]);
      available_width_left +=
          available_width_right - line[new_index].inline_size;
    }
    LayoutUnit ellipsis_offset = line[line.size() - 1].InlineOffset();

    // Find truncation point at the left.
    while (available_width_left >= line[index_left].inline_size)
      available_width_left -= line[index_left++].inline_size;
    DCHECK_LE(index_left, index_right);
    DCHECK(!line[index_left].IsPlaceholder());
    if (available_width_left > 0) {
      new_index = AddTruncatedChild(index_left, false, available_width_left,
                                    TextDirection::kLtr, line_box, box_states);
      if (new_index != kDidNotAddChild) {
        line[new_index].rect.offset.inline_offset =
            ellipsis_offset - line[new_index].inline_size;
      }
    }

    // Shift unchanged children at the left of the truncated child.
    // It's ok to modify existing children's offsets because they are not
    // web-exposed.
    LayoutUnit offset_diff =
        line[line.size() - 1].InlineOffset() - line[index_left].InlineOffset();
    for (wtf_size_t i = index_left; i > 0; --i)
      line[i - 1].rect.offset.inline_offset += offset_diff;
    line_width -= offset_diff;
  }
  // Hide left/right truncated children and children between them.
  for (wtf_size_t i = index_left; i <= index_right; ++i) {
    if (line[i].HasInFlowFragment())
      HideChild(&line[i]);
  }

  return line_width;
}

// Hide this child from being painted. Leaves a hidden fragment so that layout
// queries such as |offsetWidth| work as if it is not truncated.
void NGLineTruncator::HideChild(NGLineBoxFragmentBuilder::Child* child) {
  DCHECK(child->HasInFlowFragment());

  if (const NGPhysicalTextFragment* text = child->fragment.get()) {
    child->fragment = text->CloneAsHiddenForPaint();
    return;
  }

  if (const NGLayoutResult* layout_result = child->layout_result.get()) {
    // Need to propagate OOF descendants in this inline-block child.
    const auto& fragment =
        To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment());
    if (fragment.HasOutOfFlowPositionedDescendants())
      return;

    // If this child has self painting layer, not producing fragments will not
    // suppress painting because layers are painted separately. Move it out of
    // the clipping area.
    if (fragment.HasSelfPaintingLayer()) {
      // |available_width_| may not be enough when the containing block has
      // paddings, because clipping is at the content box but ellipsizing is at
      // the padding box. Just move to the max because we don't know paddings,
      // and max should do what we need.
      child->rect.offset.inline_offset = LayoutUnit::NearlyMax();
      return;
    }

    child->layout_result = fragment.CloneAsHiddenForPaint();
    return;
  }

  NOTREACHED();
}

// Return the offset to place the ellipsis.
//
// This function may truncate or move the child so that the ellipsis can fit.
bool NGLineTruncator::EllipsizeChild(
    LayoutUnit line_width,
    LayoutUnit ellipsis_width,
    bool is_first_child,
    NGLineBoxFragmentBuilder::Child* child,
    scoped_refptr<const NGPhysicalTextFragment>* truncated_fragment) {
  DCHECK(truncated_fragment && !*truncated_fragment);

  // Leave out-of-flow children as is.
  if (!child->HasInFlowFragment())
    return false;

  // Inline boxes should not be ellipsized. Usually they will be created in the
  // later phase, but empty inline box are already created.
  if (child->layout_result &&
      child->layout_result->PhysicalFragment().IsInlineBox())
    return false;

  // Can't place ellipsis if this child is completely outside of the box.
  LayoutUnit child_inline_offset =
      IsLtr(line_direction_)
          ? child->InlineOffset()
          : line_width - (child->InlineOffset() + child->inline_size);
  LayoutUnit space_for_child = available_width_ - child_inline_offset;
  if (space_for_child <= 0) {
    // This child is outside of the content box, but we still need to hide it.
    // When the box has paddings, this child outside of the content box maybe
    // still inside of the clipping box.
    if (!is_first_child)
      HideChild(child);
    return false;
  }

  // At least part of this child is in the box.
  // If not all of this child can fit, try to truncate.
  space_for_child -= ellipsis_width;
  if (space_for_child < child->inline_size &&
      !TruncateChild(space_for_child, is_first_child, *child,
                     truncated_fragment)) {
    // This child is partially in the box, but it should not be visible because
    // earlier sibling will be truncated and ellipsized.
    if (!is_first_child)
      HideChild(child);
    return false;
  }

  return true;
}

// Truncate the specified child. Returns true if truncated successfully, false
// otherwise.
//
// Note that this function may return true even if it can't fit the child when
// |is_first_child|, because the spec defines that the first character or atomic
// inline-level element on a line must be clipped rather than ellipsed.
// https://drafts.csswg.org/css-ui/#text-overflow
bool NGLineTruncator::TruncateChild(
    LayoutUnit space_for_child,
    bool is_first_child,
    const NGLineBoxFragmentBuilder::Child& child,
    scoped_refptr<const NGPhysicalTextFragment>* truncated_fragment) {
  DCHECK(truncated_fragment && !*truncated_fragment);

  // If the space is not enough, try the next child.
  if (space_for_child <= 0 && !is_first_child)
    return false;

  // Only text fragments can be truncated.
  if (!child.fragment)
    return is_first_child;
  auto& fragment = To<NGPhysicalTextFragment>(*child.fragment);

  // No need to truncate empty results.
  if (!fragment.TextShapeResult())
    return is_first_child;

  // TODO(layout-dev): Add support for OffsetToFit to ShapeResultView to avoid
  // this copy.
  scoped_refptr<blink::ShapeResult> shape_result =
      fragment.TextShapeResult()->CreateShapeResult();
  if (!shape_result)
    return is_first_child;

  // Compute the offset to truncate.
  unsigned new_length = shape_result->OffsetToFit(
      IsLtr(line_direction_) ? space_for_child
                             : shape_result->Width() - space_for_child,
      line_direction_);
  DCHECK_LE(new_length, fragment.TextLength());
  if (!new_length || new_length == fragment.TextLength()) {
    if (!is_first_child)
      return false;
    new_length = !new_length ? 1 : new_length - 1;
  }

  // Truncate the text fragment.
  *truncated_fragment =
      line_direction_ == shape_result->Direction()
          ? fragment.TrimText(fragment.StartOffset(),
                              fragment.StartOffset() + new_length)
          : fragment.TrimText(fragment.StartOffset() + new_length,
                              fragment.EndOffset());
  return true;
}

}  // namespace blink
