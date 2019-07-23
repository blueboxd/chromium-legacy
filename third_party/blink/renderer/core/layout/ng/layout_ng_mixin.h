// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutNGMixin_h
#define LayoutNGMixin_h

#include <type_traits>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"

namespace blink {

// This mixin holds code shared between LayoutNG subclasses of
// LayoutBlock.
template <typename Base>
class LayoutNGMixin : public Base {
 public:
  explicit LayoutNGMixin(Element* element);
  ~LayoutNGMixin() override;

  bool IsLayoutNGObject() const final { return true; }

 protected:
  bool IsOfType(LayoutObject::LayoutObjectType) const override;

  void ComputeIntrinsicLogicalWidths(
      LayoutUnit& min_logical_width,
      LayoutUnit& max_logical_width) const override;

  void UpdateOutOfFlowBlockLayout();
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlock>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutBlockFlow>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutTableCaption>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGMixin<LayoutTableCell>;

}  // namespace blink

#endif  // LayoutNGMixin_h
