// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGOutOfFlowLayoutPart_h
#define NGOutOfFlowLayoutPart_h

#include "third_party/blink/renderer/core/core_export.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class LayoutObject;
class NGBlockNode;
class NGBoxFragmentBuilder;
class NGConstraintSpace;
class NGLayoutResult;
struct NGOutOfFlowPositionedDescendant;

// Helper class for positioning of out-of-flow blocks.
// It should be used together with NGBoxFragmentBuilder.
// See NGBoxFragmentBuilder::AddOutOfFlowChildCandidate documentation
// for example of using these classes together.
class CORE_EXPORT NGOutOfFlowLayoutPart {
  STACK_ALLOCATED();

 public:
  NGOutOfFlowLayoutPart(const NGBlockNode& container_node,
                        const NGConstraintSpace& container_space,
                        const NGBoxStrut& border_scrollbar,
                        NGBoxFragmentBuilder* container_builder);

  // The |container_builder|, |border_scrollbar|, |container_space|, and
  // |container_style| parameters are all with respect to the containing block
  // of the relevant out-of-flow positioned descendants. If the CSS "containing
  // block" of such an out-of-flow positioned descendant isn't a true block
  // (e.g. a relatively positioned inline instead), the containing block here is
  // the containing block of said non-block.
  NGOutOfFlowLayoutPart(
      bool contains_absolute,
      bool contains_fixed,
      const ComputedStyle& container_style,
      const NGConstraintSpace& container_space,
      const NGBoxStrut& border_scrollbar,
      NGBoxFragmentBuilder* container_builder,
      base::Optional<LogicalSize> initial_containing_block_fixed_size =
          base::nullopt);

  // Normally this function lays out and positions all out-of-flow objects from
  // the container_builder and additional ones it discovers through laying out
  // those objects. However, if only_layout is specified, only that object will
  // get laid out; any additional ones will be stored as out-of-flow
  // descendants in the builder for use via
  // LayoutResult::OutOfFlowPositionedDescendants.
  void Run(const LayoutBox* only_layout = nullptr);

 private:
  // Information needed to position descendant within a containing block.
  // Geometry expressed here is complicated:
  // There are two types of containing blocks:
  // 1) Default containing block (DCB)
  //    Containing block passed in NGOutOfFlowLayoutPart constructor.
  //    It is the block element inside which this algorighm runs.
  //    All OOF descendants not in inline containing block are placed in DCB.
  // 2) Inline containing block
  //    OOF descendants might be positioned wrt inline containing block.
  //    Inline containing block is positioned wrt default containing block.
  struct ContainingBlockInfo {
    STACK_ALLOCATED();

   public:
    // Containing block style.
    const ComputedStyle* style;
    // Logical in containing block coordinates.
    LogicalSize content_size_for_absolute;
    // Content size for fixed is different for the ICB.
    LogicalSize content_size_for_fixed;

    // Offsets (both logical and physical) of the container's padding-box, wrt.
    // the default container's border-box.
    LogicalOffset container_offset;
    PhysicalOffset physical_container_offset;

    LogicalSize ContentSize(EPosition position) const {
      return position == EPosition::kAbsolute ? content_size_for_absolute
                                              : content_size_for_fixed;
    }
  };

  bool SweepLegacyDescendants(HashSet<const LayoutObject*>* placed_objects);

  const ContainingBlockInfo& GetContainingBlockInfo(
      const NGOutOfFlowPositionedDescendant&) const;

  void ComputeInlineContainingBlocks(
      const Vector<NGOutOfFlowPositionedDescendant>&);

  void LayoutDescendantCandidates(
      Vector<NGOutOfFlowPositionedDescendant>* descendant_candidates,
      const LayoutBox* only_layout,
      HashSet<const LayoutObject*>* placed_objects);

  scoped_refptr<const NGLayoutResult> LayoutDescendant(
      const NGOutOfFlowPositionedDescendant&,
      const LayoutBox* only_layout);

  bool IsContainingBlockForDescendant(
      const NGOutOfFlowPositionedDescendant& descendant);

  scoped_refptr<const NGLayoutResult> GenerateFragment(
      NGBlockNode node,
      const LogicalSize& container_content_size_in_child_writing_mode,
      const base::Optional<LayoutUnit>& block_estimate,
      const NGAbsolutePhysicalPosition& node_position);

  const NGConstraintSpace& container_space_;
  NGBoxFragmentBuilder* container_builder_;
  ContainingBlockInfo default_containing_block_;
  HashMap<const LayoutObject*, ContainingBlockInfo> containing_blocks_map_;
  bool contains_absolute_;
  bool contains_fixed_;
};

}  // namespace blink

#endif
