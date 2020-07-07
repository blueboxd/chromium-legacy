// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

// https://www.w3.org/TR/css-tables-3/#conflict-resolution-for-collapsed-borders
bool IsSourceMoreSpecificThanEdge(EBorderStyle source_style,
                                  LayoutUnit source_width,
                                  const NGTableBorders::Edge& edge) {
  if (edge.edge_side == NGTableBorders::EdgeSide::kDoNotFill)
    return false;

  if (!edge.style || source_style == EBorderStyle::kHidden)
    return true;

  EBorderStyle edge_border_style = NGTableBorders::BorderStyle(
      edge.style, NGTableBorders::ToBoxSide(edge.edge_side));
  if (edge_border_style == EBorderStyle::kHidden)
    return false;

  LayoutUnit edge_width = NGTableBorders::BorderWidth(
      edge.style, NGTableBorders::ToBoxSide(edge.edge_side));
  if (source_width < edge_width)
    return false;
  if (source_width > edge_width)
    return true;
  return source_style > edge_border_style;
}

NGTableBorders::EdgeSide ToEdgeSide(BoxSide box_side) {
  return static_cast<NGTableBorders::EdgeSide>(box_side);
}

}  // namespace

NGTableBorders::NGTableBorders(const ComputedStyle& table_style,
                               const NGBoxStrut& table_border_padding)
    : writing_direction_(table_style.GetWritingDirection()),
      is_collapsed_(table_style.BorderCollapse() ==
                    EBorderCollapse::kCollapse) {
  if (!is_collapsed_)
    cached_table_border_padding_ = table_border_padding;
}

#if DCHECK_IS_ON()
String NGTableBorders::DumpEdges() {
  if (edges_per_row_ == 0)
    return "No edges";

  StringBuilder edge_string;
  wtf_size_t row_count = edges_.size() / edges_per_row_;
  for (wtf_size_t row = 0; row < row_count; ++row) {
    for (wtf_size_t i = 0; i < edges_per_row_; ++i) {
      const auto& edge = edges_[edges_per_row_ * row + i];
      if (edge.style) {
        switch (edge.edge_side) {
          case EdgeSide::kTop:
            edge_string.Append('-');
            break;
          case EdgeSide::kBottom:
            edge_string.Append('_');
            break;
          case EdgeSide::kLeft:
            edge_string.Append('[');
            break;
          case EdgeSide::kRight:
            edge_string.Append(']');
            break;
          case EdgeSide::kDoNotFill:
            edge_string.Append('?');
            break;
        }
      } else {  // no style.
        if (edge.edge_side == EdgeSide::kDoNotFill)
          edge_string.Append('X');
        else
          edge_string.Append('.');
      }
      if (i & 1)  // i is odd.
        edge_string.Append(' ');
    }
    edge_string.Append('\n');
  }
  return edge_string.ToString();
}

void NGTableBorders::ShowEdges() {
  LOG(INFO) << "\n" << DumpEdges().Utf8();
}

#endif

NGBoxStrut NGTableBorders::GetCellBorders(wtf_size_t row,
                                          wtf_size_t column,
                                          wtf_size_t rowspan,
                                          wtf_size_t colspan) const {
  NGBoxStrut border_strut;
  if (edges_per_row_ == 0)
    return border_strut;
  DCHECK_EQ(edges_.size() % edges_per_row_, 0u);
  if (column * 2 >= edges_per_row_ || row >= edges_.size() / edges_per_row_)
    return border_strut;

  // Compute inline border widths.
  wtf_size_t first_inline_start_edge = row * edges_per_row_ + column * 2;
  wtf_size_t first_inline_end_edge = first_inline_start_edge + colspan * 2;
  for (wtf_size_t i = 0; i < rowspan; ++i) {
    wtf_size_t start_edge_index = first_inline_start_edge + i * edges_per_row_;
    border_strut.inline_start =
        std::max(border_strut.inline_start, BorderWidth(start_edge_index));
    if (start_edge_index >= edges_.size())
      break;
    wtf_size_t end_edge_index = first_inline_end_edge + i * edges_per_row_;
    border_strut.inline_end =
        std::max(border_strut.inline_end, BorderWidth(end_edge_index));
  }
  // Compute block border widths.
  wtf_size_t start_edge_column_index = column * 2 + 1;
  for (wtf_size_t i = 0; i < colspan; ++i) {
    wtf_size_t current_column_index = start_edge_column_index + i * 2;
    if (current_column_index >= edges_per_row_)
      break;
    wtf_size_t start_edge_index = row * edges_per_row_ + current_column_index;
    border_strut.block_start =
        std::max(border_strut.block_start, BorderWidth(start_edge_index));
    wtf_size_t end_edge_index = start_edge_index + rowspan * edges_per_row_;
    border_strut.block_end =
        std::max(border_strut.block_end, BorderWidth(end_edge_index));
  }
  DCHECK(is_collapsed_);
  // If borders are not divisible by 2, two half borders will not add up
  // to original border size (off by 1/64px). This is ok, because
  // pixel snapping will round to physical pixels.
  border_strut.block_start /= 2;
  border_strut.block_end /= 2;
  border_strut.inline_start /= 2;
  border_strut.inline_end /= 2;
  return border_strut;
}

void NGTableBorders::ComputeCollapsedTableBorderPadding(
    wtf_size_t table_row_count,
    wtf_size_t table_column_count) {
  DCHECK(is_collapsed_);
  // https://www.w3.org/TR/CSS2/tables.html#collapsing-borders
  // block[start|end] borders are computed by traversing all the edges.
  // inline[start|end] borders are computed by looking at first/last edge.
  if (edges_per_row_ == 0) {
    cached_table_border_padding_ = NGBoxStrut();
    return;
  }
  DCHECK_GE((table_column_count + 1) * 2, edges_per_row_);
  // We still need visual border rect.
  NGBoxStrut borders =
      GetCellBorders(0, 0, table_row_count, table_column_count);
  collapsed_visual_inline_start_ = borders.inline_start;
  collapsed_visual_inline_end_ = borders.inline_end;
  wtf_size_t inline_start_edge = 0;
  wtf_size_t inline_end_edge = 2 * table_column_count;
  borders.inline_start = BorderWidth(inline_start_edge) / 2;
  borders.inline_end = inline_end_edge < edges_.size()
                           ? BorderWidth(inline_end_edge) / 2
                           : LayoutUnit();
  cached_table_border_padding_ = borders;
}

NGBoxStrut NGTableBorders::CellBorder(wtf_size_t row,
                                      wtf_size_t column,
                                      wtf_size_t rowspan,
                                      wtf_size_t colspan,
                                      wtf_size_t section_index,
                                      const ComputedStyle& cell_style) const {
  if (is_collapsed_) {
    return GetCellBorders(row, column,
                          ClampRowspan(section_index, row, rowspan),
                          ClampColspan(column, colspan));
  }
  return ComputeBorders(
      NGConstraintSpaceBuilder(writing_direction_.GetWritingMode(),
                               writing_direction_.GetWritingMode(),
                               /* is_new_fc */ false)
          .ToConstraintSpace(),
      cell_style);
}

NGBoxStrut NGTableBorders::CellPadding(wtf_size_t row,
                                       wtf_size_t column,
                                       const ComputedStyle& cell_style) const {
  if (!cell_style.MayHavePadding())
    return NGBoxStrut();
  // TODO(atotic) Need to add PercentageResolutionSize to resolve padding
  // correctly.
  return ComputePadding(
      NGConstraintSpaceBuilder(writing_direction_.GetWritingMode(),
                               writing_direction_.GetWritingMode(),
                               /* is_new_fc */ false)
          .ToConstraintSpace(),
      cell_style);
}

void NGTableBorders::MergeBorders(wtf_size_t cell_start_row,
                                  wtf_size_t cell_start_column,
                                  wtf_size_t rowspan,
                                  wtf_size_t colspan,
                                  const ComputedStyle* source_style,
                                  EdgeSource source,
                                  wtf_size_t section_index) {
  DCHECK(is_collapsed_);
  // Can be 0 in empty table parts.
  if (rowspan == 0 || colspan == 0)
    return;

  wtf_size_t clamped_colspan = ClampColspan(cell_start_column, colspan);
  wtf_size_t clamped_rowspan =
      source == EdgeSource::kCell
          ? ClampRowspan(section_index, cell_start_row, rowspan)
          : rowspan;
  bool mark_inner_borders = source == EdgeSource::kCell &&
                            (clamped_rowspan > 1 || clamped_colspan > 1);

  if (mark_inner_borders) {
    EnsureCellColumnFits(cell_start_column + clamped_colspan - 1);
    EnsureCellRowFits(cell_start_row + clamped_rowspan - 1);
  } else {
    PhysicalToLogical<EBorderStyle> border_style(
        writing_direction_.GetWritingMode(), writing_direction_.Direction(),
        source_style->BorderTopStyle(), source_style->BorderRightStyle(),
        source_style->BorderBottomStyle(), source_style->BorderLeftStyle());
    if (border_style.InlineStart() == EBorderStyle::kNone &&
        border_style.InlineEnd() == EBorderStyle::kNone &&
        border_style.BlockStart() == EBorderStyle::kNone &&
        border_style.BlockEnd() == EBorderStyle::kNone) {
      return;
    }
    // Only need to ensure edges that will be assigned exist.
    if (border_style.InlineEnd() == EBorderStyle::kNone &&
        border_style.BlockStart() == EBorderStyle::kNone &&
        border_style.BlockEnd() == EBorderStyle::kNone) {
      EnsureCellColumnFits(cell_start_column);
    } else {
      EnsureCellColumnFits(cell_start_column + clamped_colspan - 1);
    }
    if (border_style.InlineStart() == EBorderStyle::kNone &&
        border_style.InlineEnd() == EBorderStyle::kNone &&
        border_style.BlockEnd() == EBorderStyle::kNone) {
      EnsureCellRowFits(cell_start_row);
    } else {
      EnsureCellRowFits(cell_start_row + clamped_rowspan - 1);
    }
  }
  MergeRowAxisBorder(cell_start_row, cell_start_column, clamped_colspan,
                     source_style, LogicalBoxSide::kBlockStart);
  MergeRowAxisBorder(cell_start_row + clamped_rowspan, cell_start_column,
                     clamped_colspan, source_style, LogicalBoxSide::kBlockEnd);
  MergeColumnAxisBorder(cell_start_row, cell_start_column, clamped_rowspan,
                        source_style, LogicalBoxSide::kInlineStart);
  MergeColumnAxisBorder(cell_start_row, cell_start_column + clamped_colspan,
                        clamped_rowspan, source_style,
                        LogicalBoxSide::kInlineEnd);
  if (mark_inner_borders) {
    MarkInnerBordersAsDoNotFill(cell_start_row, cell_start_column,
                                clamped_rowspan, clamped_colspan);
  }
}

void NGTableBorders::MergeRowAxisBorder(wtf_size_t start_row,
                                        wtf_size_t start_column,
                                        wtf_size_t colspan,
                                        const ComputedStyle* source_style,
                                        LogicalBoxSide logical_side) {
  BoxSide physical_side = LogicalToPhysical(logical_side);
  EBorderStyle source_border_style = BorderStyle(source_style, physical_side);
  if (source_border_style == EBorderStyle::kNone)
    return;
  LayoutUnit source_border_width = BorderWidth(source_style, physical_side);
  wtf_size_t start_edge = edges_per_row_ * start_row + start_column * 2 + 1;
  wtf_size_t end_edge = start_edge + colspan * 2;
  for (wtf_size_t current_edge = start_edge; current_edge < end_edge;
       current_edge += 2) {
    // https://www.w3.org/TR/css-tables-3/#border-specificity
    if (IsSourceMoreSpecificThanEdge(source_border_style, source_border_width,
                                     edges_[current_edge])) {
      edges_[current_edge].style = source_style;
      edges_[current_edge].edge_side = ToEdgeSide(physical_side);
    }
  }
}

void NGTableBorders::MergeColumnAxisBorder(wtf_size_t start_row,
                                           wtf_size_t start_column,
                                           wtf_size_t rowspan,
                                           const ComputedStyle* source_style,
                                           LogicalBoxSide logical_side) {
  BoxSide physical_side = LogicalToPhysical(logical_side);
  EBorderStyle source_border_style = BorderStyle(source_style, physical_side);
  if (source_border_style == EBorderStyle::kNone)
    return;
  LayoutUnit source_border_width = BorderWidth(source_style, physical_side);
  wtf_size_t start_edge = edges_per_row_ * start_row + start_column * 2;
  wtf_size_t end_edge = start_edge + (rowspan * edges_per_row_);
  for (wtf_size_t current_edge = start_edge; current_edge < end_edge;
       current_edge += edges_per_row_) {
    // https://www.w3.org/TR/css-tables-3/#border-specificity
    if (IsSourceMoreSpecificThanEdge(source_border_style, source_border_width,
                                     edges_[current_edge])) {
      edges_[current_edge].style = source_style;
      edges_[current_edge].edge_side = ToEdgeSide(physical_side);
    }
  }
}

// Rowspanned/colspanned cells need to mark inner edges as do-not-fill to
// prevent tables parts from drawing into them.
void NGTableBorders::MarkInnerBordersAsDoNotFill(wtf_size_t start_row,
                                                 wtf_size_t start_column,
                                                 wtf_size_t rowspan,
                                                 wtf_size_t colspan) {
  // Mark block axis edges.
  wtf_size_t start_edge = (start_column * 2) + 2;
  wtf_size_t end_edge = start_edge + (colspan - 1) * 2;
  for (wtf_size_t row = start_row;
       row < start_row + rowspan && start_edge != end_edge; ++row) {
    wtf_size_t row_offset = row * edges_per_row_;
    for (wtf_size_t edge = row_offset + start_edge;
         edge < row_offset + end_edge; edge += 2) {
      // DCHECK(!edges_[edge].style) is true in most tables. But,
      // when two cells overlap each other, (really an error)
      // style might already be assigned.
      if (!edges_[edge].style)
        edges_[edge].edge_side = EdgeSide::kDoNotFill;
    }
  }
  // Mark inline axis edges.
  start_edge = start_column * 2 + 1;
  end_edge = start_edge + colspan * 2;
  for (wtf_size_t row = start_row + 1; row < start_row + rowspan; ++row) {
    wtf_size_t row_offset = row * edges_per_row_;
    for (wtf_size_t edge = row_offset + start_edge;
         edge < row_offset + end_edge; edge += 2) {
      if (!edges_[edge].style)
        edges_[edge].edge_side = EdgeSide::kDoNotFill;
    }
  }
}

// Inline edges are edges between columns.
void NGTableBorders::EnsureCellColumnFits(wtf_size_t cell_column) {
  wtf_size_t desired_edges_per_row = (cell_column + 2) * 2;
  if (desired_edges_per_row <= edges_per_row_)
    return;

  // When number of columns changes, all rows have to be resized.
  // Edges must be copied to new positions. This can be expensive.
  // Most tables do not change number of columns after the 1st row.
  wtf_size_t row_count =
      edges_per_row_ == 0 ? 1 : edges_.size() / edges_per_row_;
  edges_.resize(row_count * desired_edges_per_row);
  for (wtf_size_t row_index = row_count - 1; row_index > 0; --row_index) {
    wtf_size_t new_edge = desired_edges_per_row - 1;
    bool done = false;
    // while loop is necessary to count down with unsigned.
    do {
      wtf_size_t new_edge_index = row_index * desired_edges_per_row + new_edge;
      if (new_edge < edges_per_row_) {
        wtf_size_t old_edge_index = row_index * edges_per_row_ + new_edge;
        DCHECK_LT(row_index * edges_per_row_ + new_edge, edges_.size());
        edges_[new_edge_index] = edges_[old_edge_index];
      } else {
        edges_[new_edge_index].style = nullptr;
        edges_[new_edge_index].edge_side = EdgeSide::kTop;
      }
      done = new_edge-- == 0;
    } while (!done);
  }
  // Previous loop does not clear out new cells in the first row.
  for (wtf_size_t edge_index = edges_per_row_;
       edge_index < desired_edges_per_row; ++edge_index) {
    edges_[edge_index].style = nullptr;
    edges_[edge_index].edge_side = EdgeSide::kTop;
  }
  edges_per_row_ = desired_edges_per_row;
}

// Block edges are edges between rows.
void NGTableBorders::EnsureCellRowFits(wtf_size_t cell_row) {
  DCHECK_NE(edges_per_row_, 0u);
  wtf_size_t current_block_edges = edges_.size() / edges_per_row_;
  wtf_size_t desired_block_edges = cell_row + 2;
  if (desired_block_edges <= current_block_edges)
    return;
  edges_.resize(desired_block_edges * edges_per_row_);
}

}  // namespace blink
