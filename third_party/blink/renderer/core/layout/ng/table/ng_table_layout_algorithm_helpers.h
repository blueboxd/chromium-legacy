// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_HELPERS_H_

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

// Table size distribution algorithms.
class NGTableAlgorithmHelpers {
 public:
  // Compute maximum number of table columns that can deduced from
  // single cell and its colspan.
  static wtf_size_t ComputeMaxColumn(wtf_size_t current_column,
                                     wtf_size_t colspan,
                                     bool is_fixed_table_layout) {
    // In fixed mode, every column is preserved.
    if (is_fixed_table_layout)
      return current_column + colspan;
    return current_column + 1;
  }

  static void DistributeColspanCellToColumns(
      const NGTableTypes::ColspanCell& colspan_cell,
      LayoutUnit inline_border_spacing,
      bool is_fixed_layout,
      NGTableTypes::Columns* column_constraints);

  static void SynchronizeAssignableTableInlineSizeAndColumns(
      LayoutUnit assignable_table_inline_size,
      LayoutUnit inline_border_spacing,
      bool is_fixed_layout,
      NGTableTypes::Columns* column_constraints);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_LAYOUT_ALGORITHM_HELPERS_H_
