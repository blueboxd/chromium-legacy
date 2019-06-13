// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMixin_h
#define LayoutNGMixin_h

#include <type_traits>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"

namespace blink {

enum class NGBaselineAlgorithmType;
class NGPaintFragment;
class NGPhysicalFragment;
struct NGInlineNodeData;

// This mixin holds code shared between LayoutNG subclasses of
// LayoutBlockFlow.

template <typename Base>
class LayoutNGMixin : public Base {
 public:
  explicit LayoutNGMixin(Element* element);
  ~LayoutNGMixin() override;

  bool IsLayoutNGObject() const final { return true; }

  NGInlineNodeData* TakeNGInlineNodeData() final;
  NGInlineNodeData* GetNGInlineNodeData() const final;
  void ResetNGInlineNodeData() final;
  void ClearNGInlineNodeData() final;
  bool HasNGInlineNodeData() const final { return ng_inline_node_data_.get(); }

  LayoutUnit FirstLineBoxBaseline() const final;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const final;

  void Paint(const PaintInfo&) const final;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) final;

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  const NGPaintFragment* PaintFragment() const final {
    // TODO(layout-dev) crbug.com/963103
    // Safer option here is to return nullptr only if
    // Lifecycle > DocumentLifecycle::kAfterPerformLayout, but this breaks
    // some layout tests.
    if (Base::NeedsLayout())
      return nullptr;
    return paint_fragment_.get();
  }
  void SetPaintFragment(const NGBlockBreakToken*,
                        scoped_refptr<const NGPhysicalFragment>) final;

 protected:
  bool IsOfType(LayoutObject::LayoutObjectType) const override;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void ComputeIntrinsicLogicalWidths(
      LayoutUnit& min_logical_width,
      LayoutUnit& max_logical_width) const override;

  void AddLayoutOverflowFromChildren() final;

  void AddOutlineRects(Vector<PhysicalRect>&,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const final;

  bool PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const final;

  const NGPhysicalBoxFragment* CurrentFragment() const final;

  base::Optional<LayoutUnit> FragmentBaseline(NGBaselineAlgorithmType) const;

  void DirtyLinesFromChangedChild(LayoutObject* child,
                                  MarkingBehavior marking_behavior) final;

  std::unique_ptr<NGInlineNodeData> ng_inline_node_data_;
  scoped_refptr<NGPaintFragment> paint_fragment_;

  friend class NGBaseLayoutAlgorithmTest;

 private:
  void AddScrollingOverflowFromChildren();
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutBlockFlow>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutTableCaption>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutTableCell>;

}  // namespace blink

#endif  // LayoutNGMixin_h
