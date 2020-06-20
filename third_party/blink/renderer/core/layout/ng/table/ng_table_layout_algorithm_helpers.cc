// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_helpers.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"

namespace blink {

namespace {

// Implements spec distribution algorithm:
// https://www.w3.org/TR/css-tables-3/#width-distribution-algorithm
void DistributeInlineSizeToComputedInlineSizeAuto(
    LayoutUnit target_inline_size,
    LayoutUnit inline_border_spacing,
    NGTableTypes::Column* start_column,
    NGTableTypes::Column* end_column,
    NGTableTypes::Columns* column_constraints) {
  if (column_constraints->size() == 0)
    return;

  unsigned all_columns_count = 0;
  unsigned percent_columns_count = 0;
  unsigned fixed_columns_count = 0;
  unsigned auto_columns_count = 0;

  // What guesses mean is described in table specification.
  // https://www.w3.org/TR/css-tables-3/#width-distribution-algorithm
  enum { kMinGuess, kPercentageGuess, kSpecifiedGuess, kMaxGuess, kAboveMax };
  // sizes are collected for all guesses except kAboveMax
  LayoutUnit guess_sizes[kAboveMax];
  LayoutUnit guess_size_total_increases[kAboveMax];
  float total_percent = 0.0f;
  LayoutUnit total_auto_max_inline_size;
  LayoutUnit total_fixed_max_inline_size;

  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    all_columns_count++;
    if (!column->min_inline_size)
      column->min_inline_size = LayoutUnit();
    if (!column->max_inline_size)
      column->max_inline_size = LayoutUnit();
    if (column->percent) {
      percent_columns_count++;
      total_percent += *column->percent;
      LayoutUnit percent_inline_size =
          column->ResolvePercentInlineSize(target_inline_size);
      guess_sizes[kMinGuess] += *column->min_inline_size;
      guess_sizes[kPercentageGuess] += percent_inline_size;
      guess_sizes[kSpecifiedGuess] += percent_inline_size;
      guess_sizes[kMaxGuess] += percent_inline_size;
      guess_size_total_increases[kPercentageGuess] +=
          percent_inline_size - *column->min_inline_size;
    } else if (column->is_constrained) {  // Fixed column
      fixed_columns_count++;
      total_fixed_max_inline_size += *column->max_inline_size;
      guess_sizes[kMinGuess] += *column->min_inline_size;
      guess_sizes[kPercentageGuess] += *column->min_inline_size;
      guess_sizes[kSpecifiedGuess] += *column->max_inline_size;
      guess_sizes[kMaxGuess] += *column->max_inline_size;
      guess_size_total_increases[kSpecifiedGuess] +=
          *column->max_inline_size - *column->min_inline_size;
    } else {  // Auto column
      auto_columns_count++;
      total_auto_max_inline_size += *column->max_inline_size;
      guess_sizes[kMinGuess] += *column->min_inline_size;
      guess_sizes[kPercentageGuess] += *column->min_inline_size;
      guess_sizes[kSpecifiedGuess] += *column->min_inline_size;
      guess_sizes[kMaxGuess] += *column->max_inline_size;
      guess_size_total_increases[kMaxGuess] +=
          *column->max_inline_size - *column->min_inline_size;
    }
  }
  // Distributing inline sizes can never cause cells to be < min_inline_size.
  // Target inline size must be wider than sum of min inline sizes.
  // This is always true for assignable_table_inline_size, but not for
  // colspan_cells.
  target_inline_size = std::max(target_inline_size, guess_sizes[kMinGuess]);

  unsigned starting_guess = kAboveMax;
  for (unsigned i = kMinGuess; i != kAboveMax; ++i) {
    if (guess_sizes[i] >= target_inline_size) {
      starting_guess = i;
      break;
    }
  }
  switch (starting_guess) {
    case kMinGuess: {
      // All columns are min inline size.
      for (NGTableTypes::Column* column = start_column; column != end_column;
           ++column) {
        column->computed_inline_size =
            column->min_inline_size.value_or(LayoutUnit());
      }
    } break;
    case kPercentageGuess: {
      // Percent columns grow, auto/fixed get min inline size.
      LayoutUnit percent_inline_size_increases =
          guess_size_total_increases[kPercentageGuess];
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kMinGuess];
      LayoutUnit rounding_error_inline_size = distributable_inline_size;
      NGTableTypes::Column* last_column = nullptr;
      for (NGTableTypes::Column* column = start_column; column != end_column;
           ++column) {
        if (column->percent) {
          last_column = column;
          LayoutUnit percent_inline_size =
              column->ResolvePercentInlineSize(target_inline_size);
          LayoutUnit column_inline_size_increase =
              percent_inline_size - *column->min_inline_size;
          LayoutUnit delta;
          if (percent_inline_size_increases != LayoutUnit()) {
            delta = LayoutUnit(distributable_inline_size *
                               column_inline_size_increase.ToFloat() /
                               percent_inline_size_increases);
          } else {
            delta = LayoutUnit(distributable_inline_size.ToFloat() /
                               percent_columns_count);
          }
          rounding_error_inline_size -= delta;
          column->computed_inline_size = *column->min_inline_size + delta;
        } else {
          // Auto/Fixed columns get min inline size.
          column->computed_inline_size = *column->min_inline_size;
        }
      }
      if (rounding_error_inline_size != LayoutUnit()) {
        DCHECK(last_column);
        last_column->computed_inline_size += rounding_error_inline_size;
      }
    } break;
    case kSpecifiedGuess: {
      // Fixed columns grow, auto gets min, percent gets %max
      LayoutUnit fixed_inline_size_increase =
          guess_size_total_increases[kSpecifiedGuess];
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kPercentageGuess];
      LayoutUnit rounding_error_inline_size = distributable_inline_size;
      NGTableTypes::Column* last_column = nullptr;
      for (NGTableTypes::Column* column = start_column; column != end_column;
           ++column) {
        if (column->percent) {
          column->computed_inline_size =
              column->ResolvePercentInlineSize(target_inline_size);
        } else if (column->is_constrained) {
          last_column = column;
          LayoutUnit column_inline_size_increase =
              *column->max_inline_size - *column->min_inline_size;
          LayoutUnit delta;
          if (fixed_inline_size_increase != LayoutUnit()) {
            delta = LayoutUnit(distributable_inline_size *
                               column_inline_size_increase.ToFloat() /
                               fixed_inline_size_increase);
          } else {
            delta = LayoutUnit(distributable_inline_size.ToFloat() /
                               fixed_columns_count);
          }
          rounding_error_inline_size -= delta;
          column->computed_inline_size = *column->min_inline_size + delta;
        } else {
          column->computed_inline_size = *column->min_inline_size;
        }
      }
      if (rounding_error_inline_size != LayoutUnit()) {
        DCHECK(last_column);
        last_column->computed_inline_size += rounding_error_inline_size;
      }
    } break;
    case kMaxGuess: {
      // Auto columns grow, fixed gets max, percent gets %max
      LayoutUnit auto_inline_size_increase =
          guess_size_total_increases[kMaxGuess];
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kSpecifiedGuess];
      LayoutUnit rounding_error_inline_size = distributable_inline_size;
      NGTableTypes::Column* last_column = nullptr;
      for (NGTableTypes::Column* column = start_column; column != end_column;
           ++column) {
        if (column->percent) {
          column->computed_inline_size =
              column->ResolvePercentInlineSize(target_inline_size);
        } else if (column->is_constrained) {
          column->computed_inline_size = *column->max_inline_size;
        } else {
          last_column = column;
          LayoutUnit column_inline_size_increase =
              *column->max_inline_size - *column->min_inline_size;
          LayoutUnit delta;
          if (auto_inline_size_increase != LayoutUnit()) {
            delta = LayoutUnit(distributable_inline_size *
                               column_inline_size_increase.ToFloat() /
                               auto_inline_size_increase);
          } else {
            delta = LayoutUnit(distributable_inline_size.ToFloat() /
                               auto_columns_count);
          }
          rounding_error_inline_size -= delta;
          column->computed_inline_size = *column->min_inline_size + delta;
        }
      }
      if (rounding_error_inline_size != LayoutUnit()) {
        DCHECK(last_column);
        last_column->computed_inline_size += rounding_error_inline_size;
      }
    } break;
    case kAboveMax: {
      LayoutUnit distributable_inline_size =
          target_inline_size - guess_sizes[kMaxGuess];
      if (auto_columns_count > 0) {
        // Grow auto columns if available
        LayoutUnit rounding_error_inline_size = distributable_inline_size;
        NGTableTypes::Column* last_column = nullptr;
        for (NGTableTypes::Column* column = start_column; column != end_column;
             ++column) {
          if (column->percent) {
            column->computed_inline_size =
                column->ResolvePercentInlineSize(target_inline_size);
          } else if (column->is_constrained) {
            column->computed_inline_size = *column->max_inline_size;
          } else {
            last_column = column;
            LayoutUnit delta;
            if (total_auto_max_inline_size > LayoutUnit()) {
              delta = LayoutUnit(distributable_inline_size *
                                 (*column->max_inline_size).ToFloat() /
                                 total_auto_max_inline_size);
            } else {
              delta = distributable_inline_size / auto_columns_count;
            }
            rounding_error_inline_size -= delta;
            column->computed_inline_size = *column->max_inline_size + delta;
          }
        }
        if (rounding_error_inline_size != LayoutUnit()) {
          DCHECK(last_column);
          last_column->computed_inline_size += rounding_error_inline_size;
        }
      } else if (fixed_columns_count > 0) {
        // Grow fixed columns if available.
        LayoutUnit rounding_error_inline_size = distributable_inline_size;
        NGTableTypes::Column* last_column = nullptr;
        for (NGTableTypes::Column* column = start_column; column != end_column;
             ++column) {
          if (column->percent) {
            column->computed_inline_size =
                column->ResolvePercentInlineSize(target_inline_size);
          } else if (column->is_constrained) {
            last_column = column;
            LayoutUnit delta;
            if (total_fixed_max_inline_size > LayoutUnit()) {
              delta = LayoutUnit(distributable_inline_size *
                                 (*column->max_inline_size).ToFloat() /
                                 total_fixed_max_inline_size);
            } else {
              delta = distributable_inline_size / fixed_columns_count;
            }
            rounding_error_inline_size -= delta;
            column->computed_inline_size = *column->max_inline_size + delta;
          } else {
            DCHECK(false);
          }
        }
        if (rounding_error_inline_size != LayoutUnit()) {
          DCHECK(last_column);
          last_column->computed_inline_size += rounding_error_inline_size;
        }
      } else if (percent_columns_count > 0) {
        // Grow percent columns.
        for (NGTableTypes::Column* column = start_column; column != end_column;
             ++column) {
          if (column->percent) {
            if (total_percent > 0.0f) {
              column->computed_inline_size = LayoutUnit(
                  *column->percent / total_percent * target_inline_size);
            } else {
              column->computed_inline_size =
                  distributable_inline_size / percent_columns_count;
            }
          } else {
            DCHECK(false);
          }
        }
      }
    }
  }

#if DCHECK_IS_ON()
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    DCHECK_NE(column->computed_inline_size, kIndefiniteSize);
  }
#endif
}

void SynchronizeAssignableTableInlineSizeAndColumnsFixed(
    LayoutUnit target_inline_size,
    LayoutUnit inline_border_spacing,
    NGTableTypes::Column* start_column,
    NGTableTypes::Column* end_column) {
  DCHECK_NE(start_column, end_column);
  unsigned all_columns_count = 0;
  unsigned percent_columns_count = 0;
  unsigned auto_columns_count = 0;
  unsigned auto_empty_columns_count = 0;
  unsigned fixed_columns_count = 0;

  float total_percent = 0.0f;
  LayoutUnit total_percent_inline_size;
  LayoutUnit total_auto_max_inline_size;
  LayoutUnit total_fixed_inline_size;
  LayoutUnit assigned_inline_size;

  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    all_columns_count++;
    if (!column->min_inline_size)
      column->min_inline_size = LayoutUnit();
    if (!column->max_inline_size)
      column->max_inline_size = LayoutUnit();
    if (column->percent) {
      percent_columns_count++;
      total_percent += *column->percent;
      total_percent_inline_size +=
          LayoutUnit(*column->percent / 100 * target_inline_size);
    } else if (column->is_constrained) {  // Fixed column
      fixed_columns_count++;
      total_fixed_inline_size += *column->max_inline_size;
    } else {
      auto_columns_count++;
      if (*column->max_inline_size == LayoutUnit())
        auto_empty_columns_count++;
      total_auto_max_inline_size += *column->max_inline_size;
    }
  }

  NGTableTypes::Column* last_distributed_column = nullptr;
  // Distribute to fixed columns.
  if (fixed_columns_count > 0) {
    float scale = 1.0f;
    bool scale_available = true;
    LayoutUnit target_fixed_size =
        (target_inline_size - total_percent_inline_size).ClampNegativeToZero();
    bool scale_up =
        total_fixed_inline_size < target_fixed_size && auto_columns_count == 0;
    // Fixed columns grow if there are no auto columns. They fill up space not
    // taken up by percentage columns.
    bool scale_down = total_fixed_inline_size > target_inline_size;
    if (scale_up || scale_down) {
      if (total_fixed_inline_size != LayoutUnit()) {
        scale = target_fixed_size.ToFloat() / total_fixed_inline_size;
      } else {
        scale_available = false;
      }
    }
    for (NGTableTypes::Column* column = start_column; column != end_column;
         ++column) {
      if (!column->IsFixed())
        continue;
      last_distributed_column = column;
      if (scale_available) {
        column->computed_inline_size =
            LayoutUnit(scale * *column->max_inline_size);
      } else {
        DCHECK_EQ(fixed_columns_count, all_columns_count);
        column->computed_inline_size =
            LayoutUnit(target_inline_size.ToFloat() / fixed_columns_count);
      }
      assigned_inline_size += column->computed_inline_size;
    }
  }
  if (assigned_inline_size >= target_inline_size)
    return;
  // Distribute to percent columns.
  if (percent_columns_count > 0) {
    float scale = 1.0f;
    bool scale_available = true;
    // Percent columns only grow if there are no auto columns.
    bool scale_up = total_percent_inline_size <
                        (target_inline_size - assigned_inline_size) &&
                    auto_columns_count == 0;
    bool scale_down =
        total_percent_inline_size > (target_inline_size - assigned_inline_size);
    if (scale_up || scale_down) {
      if (total_percent_inline_size != LayoutUnit()) {
        scale = (target_inline_size - assigned_inline_size).ToFloat() /
                total_percent_inline_size;
      } else {
        scale_available = false;
      }
    }
    for (NGTableTypes::Column* column = start_column; column != end_column;
         ++column) {
      if (!column->percent)
        continue;
      last_distributed_column = column;
      if (scale_available) {
        column->computed_inline_size =
            LayoutUnit(scale * *column->percent / 100 * target_inline_size);
      } else {
        column->computed_inline_size =
            LayoutUnit((target_inline_size - assigned_inline_size).ToFloat() /
                       percent_columns_count);
      }
      assigned_inline_size += column->computed_inline_size;
    }
  }
  // Distribute to auto columns.
  LayoutUnit distributing_inline_size =
      target_inline_size - assigned_inline_size;
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    if (column->percent || column->is_constrained)
      continue;
    last_distributed_column = column;
    column->computed_inline_size =
        LayoutUnit(distributing_inline_size / float(auto_columns_count));
    assigned_inline_size += column->computed_inline_size;
  }
  LayoutUnit delta = target_inline_size - assigned_inline_size;
  last_distributed_column->computed_inline_size += delta;
}

void DistributeColspanCellToColumnsFixed(
    const NGTableTypes::ColspanCell& colspan_cell,
    LayoutUnit inline_border_spacing,
    NGTableTypes::Columns* column_constraints) {
  // Fixed layout does not merge columns.
  DCHECK_LE(colspan_cell.span,
            column_constraints->size() - colspan_cell.start_column);
  NGTableTypes::Column* start_column =
      &(*column_constraints)[colspan_cell.start_column];
  NGTableTypes::Column* end_column = start_column + colspan_cell.span;
  DCHECK_NE(start_column, end_column);

  LayoutUnit colspan_cell_min_inline_size;
  LayoutUnit colspan_cell_max_inline_size;
  if (colspan_cell.cell_inline_constraint.is_constrained) {
    colspan_cell_min_inline_size =
        (colspan_cell.cell_inline_constraint.min_inline_size -
         (colspan_cell.span - 1) * inline_border_spacing)
            .ClampNegativeToZero();
    colspan_cell_max_inline_size =
        (colspan_cell.cell_inline_constraint.max_inline_size -
         (colspan_cell.span - 1) * inline_border_spacing)
            .ClampNegativeToZero();
  }

  // Distribute min/max/percentage evenly between all cells.
  // Colspanned cells only distribute min inline size if constrained.
  LayoutUnit rounding_error_min_inline_size = colspan_cell_min_inline_size;
  LayoutUnit rounding_error_max_inline_size = colspan_cell_max_inline_size;
  float rounding_error_percent =
      colspan_cell.cell_inline_constraint.percent.value_or(0.0f);

  LayoutUnit new_min_size = LayoutUnit(colspan_cell_min_inline_size /
                                       static_cast<float>(colspan_cell.span));
  LayoutUnit new_max_size = LayoutUnit(colspan_cell_max_inline_size /
                                       static_cast<float>(colspan_cell.span));
  base::Optional<float> new_percent;
  if (colspan_cell.cell_inline_constraint.percent) {
    new_percent =
        *colspan_cell.cell_inline_constraint.percent / colspan_cell.span;
  }

  NGTableTypes::Column* last_column;
  for (NGTableTypes::Column* column = start_column; column < end_column;
       ++column) {
    last_column = column;
    rounding_error_min_inline_size -= new_min_size;
    rounding_error_max_inline_size -= new_max_size;
    if (new_percent)
      rounding_error_percent -= *new_percent;

    if (!column->min_inline_size) {
      column->is_constrained |=
          colspan_cell.cell_inline_constraint.is_constrained;
      column->min_inline_size = new_min_size;
    }
    if (!column->max_inline_size) {
      column->is_constrained |=
          colspan_cell.cell_inline_constraint.is_constrained;
      column->max_inline_size = new_max_size;
    }
    if (!column->percent && new_percent)
      column->percent = new_percent;
  }
  last_column->min_inline_size =
      *last_column->min_inline_size + rounding_error_min_inline_size;
  last_column->max_inline_size =
      *last_column->max_inline_size + rounding_error_max_inline_size;
  if (new_percent)
    last_column->percent = *last_column->percent + rounding_error_percent;
}

void DistributeColspanCellToColumnsAuto(
    const NGTableTypes::ColspanCell& colspan_cell,
    LayoutUnit inline_border_spacing,
    NGTableTypes::Columns* column_constraints) {
  unsigned effective_span =
      std::min(colspan_cell.span,
               column_constraints->size() - colspan_cell.start_column);
  NGTableTypes::Column* start_column =
      &(*column_constraints)[colspan_cell.start_column];
  NGTableTypes::Column* end_column = start_column + effective_span;

  // Inline sizes for redistribution exclude border spacing.
  LayoutUnit colspan_cell_min_inline_size =
      (colspan_cell.cell_inline_constraint.min_inline_size -
       (effective_span - 1) * inline_border_spacing)
          .ClampNegativeToZero();
  LayoutUnit colspan_cell_max_inline_size =
      (colspan_cell.cell_inline_constraint.max_inline_size -
       (effective_span - 1) * inline_border_spacing)
          .ClampNegativeToZero();
  base::Optional<float> colspan_cell_percent =
      colspan_cell.cell_inline_constraint.percent;

  if (colspan_cell_percent.has_value()) {
    float columns_percent = 0.0f;
    unsigned all_columns_count = 0;
    unsigned percent_columns_count = 0;
    unsigned nonpercent_columns_count = 0;
    LayoutUnit nonpercent_columns_max_inline_size;
    for (NGTableTypes::Column* column = start_column; column != end_column;
         ++column) {
      if (!column->max_inline_size)
        column->max_inline_size = LayoutUnit();
      if (!column->min_inline_size)
        column->min_inline_size = LayoutUnit();
      all_columns_count++;
      if (column->percent) {
        percent_columns_count++;
        columns_percent += *column->percent;
      } else {
        nonpercent_columns_count++;
        nonpercent_columns_max_inline_size += *column->max_inline_size;
      }
    }
    float surplus_percent = *colspan_cell_percent - columns_percent;
    if (surplus_percent > 0.0f && all_columns_count > percent_columns_count) {
      // Distribute surplus percent to non-percent columns in proportion to
      // max_inline_size.
      for (NGTableTypes::Column* column = start_column; column != end_column;
           ++column) {
        if (column->percent)
          continue;
        float column_percent;
        if (nonpercent_columns_max_inline_size != LayoutUnit()) {
          // Column percentage is proportional to its max_inline_size.
          column_percent = surplus_percent *
                           column->max_inline_size.value_or(LayoutUnit()) /
                           nonpercent_columns_max_inline_size;
        } else {
          // Distribute evenly instead.
          // Legacy difference: Legacy forces max_inline_size to be at least
          // 1px.
          column_percent = surplus_percent / nonpercent_columns_count;
        }
        column->percent = column_percent;
      }
    }
  }

  // TODO(atotic) See crbug.com/531752 for discussion about differences
  // between FF/Chrome.
  // Minimum inline size gets distributed with standard distribution algorithm.
  DistributeInlineSizeToComputedInlineSizeAuto(
      colspan_cell_min_inline_size, inline_border_spacing, start_column,
      end_column, column_constraints);
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    column->min_inline_size =
        std::max(*column->min_inline_size, column->computed_inline_size);
  }
  DistributeInlineSizeToComputedInlineSizeAuto(
      colspan_cell_max_inline_size, inline_border_spacing, start_column,
      end_column, column_constraints);
  for (NGTableTypes::Column* column = start_column; column != end_column;
       ++column) {
    column->max_inline_size =
        std::max(*column->max_inline_size, column->computed_inline_size);
  }
}

}  // namespace

void NGTableAlgorithmHelpers::DistributeColspanCellToColumns(
    const NGTableTypes::ColspanCell& colspan_cell,
    LayoutUnit inline_border_spacing,
    bool is_fixed_layout,
    NGTableTypes::Columns* column_constraints) {
  // Clipped colspanned cells can end up having a span of 1 (which is not wide).
  DCHECK_GT(colspan_cell.span, 1u);

  if (is_fixed_layout) {
    DistributeColspanCellToColumnsFixed(colspan_cell, inline_border_spacing,
                                        column_constraints);
  } else {
    DistributeColspanCellToColumnsAuto(colspan_cell, inline_border_spacing,
                                       column_constraints);
  }
}

// Standard: https://www.w3.org/TR/css-tables-3/#width-distribution-algorithm
// After synchroniziation, assignable table inline size and sum of column
// final inline sizes will be equal.
void NGTableAlgorithmHelpers::SynchronizeAssignableTableInlineSizeAndColumns(
    LayoutUnit assignable_table_inline_size,
    LayoutUnit inline_border_spacing,
    bool is_fixed_layout,
    NGTableTypes::Columns* column_constraints) {
  if (column_constraints->size() == 0)
    return;
  NGTableTypes::Column* start_column = &(*column_constraints)[0];
  NGTableTypes::Column* end_column = start_column + column_constraints->size();
  if (is_fixed_layout) {
    SynchronizeAssignableTableInlineSizeAndColumnsFixed(
        assignable_table_inline_size, inline_border_spacing, start_column,
        end_column);
  } else {
    DistributeInlineSizeToComputedInlineSizeAuto(
        assignable_table_inline_size, inline_border_spacing, start_column,
        end_column, column_constraints);
  }
}

}  // namespace blink
