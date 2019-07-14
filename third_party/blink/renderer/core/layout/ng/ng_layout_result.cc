// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"

namespace blink {

namespace {

struct SameSizeAsNGLayoutResult : public RefCounted<SameSizeAsNGLayoutResult> {
  const NGConstraintSpace space;
  void* physical_fragment;
  union {
    NGBfcOffset bfc_offset;
    LogicalOffset oof_positioned_offset;
    void* rare_data;
  };
  LayoutUnit intrinsic_block_size;
  unsigned bitfields[1];
};

static_assert(sizeof(NGLayoutResult) == sizeof(SameSizeAsNGLayoutResult),
              "NGLayoutResult should stay small.");

}  // namespace

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGBoxFragmentBuilder* builder)
    : NGLayoutResult(std::move(physical_fragment),
                     builder,
                     /* cache_space */ true) {
  bitfields_.is_initial_block_size_indefinite =
      builder->is_initial_block_size_indefinite_;
  intrinsic_block_size_ = builder->intrinsic_block_size_;
  if (builder->minimal_space_shortage_ != LayoutUnit::Max())
    EnsureRareData()->minimal_space_shortage = builder->minimal_space_shortage_;
  bitfields_.initial_break_before =
      static_cast<unsigned>(builder->initial_break_before_);
  bitfields_.final_break_after =
      static_cast<unsigned>(builder->previous_break_after_);
  bitfields_.has_forced_break = builder->has_forced_break_;
}

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGLineBoxFragmentBuilder* builder)
    : NGLayoutResult(std::move(physical_fragment),
                     builder,
                     /* cache_space */ false) {}

NGLayoutResult::NGLayoutResult(NGLayoutResultStatus status,
                               NGBoxFragmentBuilder* builder)
    : NGLayoutResult(/* physical_fragment */ nullptr,
                     builder,
                     /* cache_space */ false) {
  bitfields_.status = status;
  DCHECK_NE(status, kSuccess)
      << "Use the other constructor for successful layout";
}

NGLayoutResult::NGLayoutResult(const NGLayoutResult& other,
                               const NGConstraintSpace& new_space,
                               LayoutUnit bfc_line_offset,
                               base::Optional<LayoutUnit> bfc_block_offset)
    : space_(new_space),
      physical_fragment_(other.physical_fragment_),
      intrinsic_block_size_(other.intrinsic_block_size_),
      bitfields_(other.bitfields_) {
  if (HasRareData()) {
    rare_data_ = new RareData(*other.rare_data_);
    rare_data_->bfc_line_offset = bfc_line_offset;
    rare_data_->bfc_block_offset = bfc_block_offset;
  } else if (!bitfields_.has_oof_positioned_offset) {
    bfc_offset_.line_offset = bfc_line_offset;
    bfc_offset_.block_offset = bfc_block_offset.value_or(LayoutUnit());
    bitfields_.is_bfc_block_offset_nullopt = !bfc_block_offset.has_value();
  } else {
    DCHECK(physical_fragment_->IsOutOfFlowPositioned());
    DCHECK_EQ(bfc_line_offset, LayoutUnit());
    DCHECK(bfc_block_offset && bfc_block_offset.value() == LayoutUnit());
    oof_positioned_offset_ = LogicalOffset();
  }

  NGExclusionSpace new_exclusion_space = MergeExclusionSpaces(
      other, space_.ExclusionSpace(), bfc_line_offset, bfc_block_offset);

  if (new_exclusion_space != space_.ExclusionSpace()) {
    bitfields_.has_rare_data_exclusion_space = true;
    EnsureRareData()->exclusion_space = std::move(new_exclusion_space);
  } else {
    space_.ExclusionSpace().MoveDerivedGeometry(new_exclusion_space);
  }
}

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGContainerFragmentBuilder* builder,
    bool cache_space)
    : space_(builder->space_ ? NGConstraintSpace(*builder->space_)
                             : NGConstraintSpace()),
      physical_fragment_(std::move(physical_fragment)),
      bitfields_(
          /* has_valid_space */ cache_space && builder->space_,
          /* is_self_collapsing */ builder->is_self_collapsing_,
          /* is_pushed_by_floats */ builder->is_pushed_by_floats_,
          /* adjoining_object_types */ builder->adjoining_object_types_,
          /* has_descendant_that_depends_on_percentage_block_size */
          builder->has_descendant_that_depends_on_percentage_block_size_) {
#if DCHECK_IS_ON()
  if (bitfields_.is_self_collapsing && physical_fragment_) {
    // A new formatting-context shouldn't be self-collapsing.
    DCHECK(!physical_fragment_->IsBlockFormattingContextRoot());

    // Self-collapsing children must have a block-size of zero.
    NGFragment fragment(physical_fragment_->Style().GetWritingMode(),
                        *physical_fragment_);
    DCHECK_EQ(LayoutUnit(), fragment.BlockSize());
  }
#endif

  if (builder->end_margin_strut_ != NGMarginStrut())
    EnsureRareData()->end_margin_strut = builder->end_margin_strut_;
  if (builder->unpositioned_list_marker_) {
    EnsureRareData()->unpositioned_list_marker =
        builder->unpositioned_list_marker_;
  }
  if (builder->exclusion_space_ != space_.ExclusionSpace()) {
    bitfields_.has_rare_data_exclusion_space = true;
    EnsureRareData()->exclusion_space = std::move(builder->exclusion_space_);
  } else {
    space_.ExclusionSpace().MoveDerivedGeometry(builder->exclusion_space_);
  }

  if (HasRareData()) {
    rare_data_->bfc_line_offset = builder->bfc_line_offset_;
    rare_data_->bfc_block_offset = builder->bfc_block_offset_;
  } else {
    bfc_offset_.line_offset = builder->bfc_line_offset_;
    bfc_offset_.block_offset =
        builder->bfc_block_offset_.value_or(LayoutUnit());
    bitfields_.is_bfc_block_offset_nullopt =
        !builder->bfc_block_offset_.has_value();
  }
}

NGLayoutResult::~NGLayoutResult() {
  if (HasRareData())
    delete rare_data_;
}

NGExclusionSpace NGLayoutResult::MergeExclusionSpaces(
    const NGLayoutResult& other,
    const NGExclusionSpace& new_input_exclusion_space,
    LayoutUnit bfc_line_offset,
    base::Optional<LayoutUnit> bfc_block_offset) {
  // If we are merging exclusion spaces we should be copying a previous layout
  // result. It is impossible to reach a state where bfc_block_offset has a
  // value, and the result which we are copying doesn't (or visa versa).
  // This would imply the result has switched its "empty" state for margin
  // collapsing, which would mean it isn't possible to reuse the result.
  DCHECK_EQ(bfc_block_offset.has_value(), other.BfcBlockOffset().has_value());

  NGBfcDelta offset_delta = {bfc_line_offset - other.BfcLineOffset(),
                             bfc_block_offset && other.BfcBlockOffset()
                                 ? *bfc_block_offset - *other.BfcBlockOffset()
                                 : LayoutUnit()};

  return NGExclusionSpace::MergeExclusionSpaces(
      /* old_output */ other.ExclusionSpace(),
      /* old_input */ other.space_.ExclusionSpace(),
      /* new_input */ new_input_exclusion_space, offset_delta);
}

NGLayoutResult::RareData* NGLayoutResult::EnsureRareData() {
  if (!HasRareData()) {
    base::Optional<LayoutUnit> bfc_block_offset;
    if (!bitfields_.is_bfc_block_offset_nullopt)
      bfc_block_offset = bfc_offset_.block_offset;
    rare_data_ = new RareData(bfc_offset_.line_offset, bfc_block_offset);
    bitfields_.has_rare_data = true;
  }

  return rare_data_;
}

#if DCHECK_IS_ON()
void NGLayoutResult::CheckSameForSimplifiedLayout(
    const NGLayoutResult& other,
    bool check_same_block_size) const {
  To<NGPhysicalBoxFragment>(*physical_fragment_)
      .CheckSameForSimplifiedLayout(
          To<NGPhysicalBoxFragment>(*other.physical_fragment_),
          check_same_block_size);

  DCHECK(UnpositionedListMarker() == other.UnpositionedListMarker());
  ExclusionSpace().CheckSameForSimplifiedLayout(other.ExclusionSpace());

  // We ignore |BfcBlockOffset|, and |BfcLineOffset| as "simplified" layout
  // will move the layout result if required.

  // We ignore the |intrinsic_block_size_| as if a scrollbar gets added/removed
  // this may change (even if the size of the fragment remains the same).

  DCHECK(EndMarginStrut() == other.EndMarginStrut());
  DCHECK_EQ(MinimalSpaceShortage(), other.MinimalSpaceShortage());

  DCHECK_EQ(bitfields_.has_valid_space, other.bitfields_.has_valid_space);
  DCHECK_EQ(bitfields_.has_forced_break, other.bitfields_.has_forced_break);
  DCHECK_EQ(bitfields_.is_self_collapsing, other.bitfields_.is_self_collapsing);
  DCHECK_EQ(bitfields_.is_pushed_by_floats,
            other.bitfields_.is_pushed_by_floats);
  DCHECK_EQ(bitfields_.adjoining_object_types,
            other.bitfields_.adjoining_object_types);

  DCHECK_EQ(bitfields_.initial_break_before,
            other.bitfields_.initial_break_before);
  DCHECK_EQ(bitfields_.final_break_after, other.bitfields_.final_break_after);

  if (check_same_block_size) {
    DCHECK_EQ(bitfields_.is_initial_block_size_indefinite,
              other.bitfields_.is_initial_block_size_indefinite);
  }
  DCHECK_EQ(
      bitfields_.has_descendant_that_depends_on_percentage_block_size,
      other.bitfields_.has_descendant_that_depends_on_percentage_block_size);
  DCHECK_EQ(bitfields_.status, other.bitfields_.status);
}
#endif

}  // namespace blink
