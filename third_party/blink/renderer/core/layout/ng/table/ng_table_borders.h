// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_BORDERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_BORDERS_H_

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;
struct NGBoxStrut;

// When table has collapsed borders, computing borders for table parts is
// complex, and costly.
// NGTableBorders precomputes collapsed borders. It exposes the API for
// border access. If borders are not collapsed, the API returns regular
// borders.
//
// NGTableBorders methods often take rowspan/colspan arguments.
// Rowspan must never be taller than the section.
// Colspan must never be wider than the table.
// To enforce this, NGTableBorders keeps track of section dimensions,
// and table's last column.
//
// Collapsed borders are stored as edges.
// Edges are stored in a 1D array. The array does not grow if borders are
// not set.
// Each edge represents a cell border.
// Mapping between edges and cells is best understood like this:
// - each cell stores only two edges, left edge, and a top edge.
// - cell's right edge is the left edge of the next cell.
// - cell's bottom edge is the top edge of the cell in the next row.
//
// To store all last row/col edges, an extra imaginary cell is used.
//
// A grid with R rows and C columns has |2 * (R+1) * (C+1)| edges.
// Example; R=1, C=3, 2*(1+1)*(3+1) = 16 edges.
// Edges 7, 9, 11, 13, 14, 15 are unused.
//
//     1    3   5   7
//   ------------------    <= edges for 3 cols X 1 row
//   |0  |2  |4   |6
//   |   |   |    |
//   ------------------
//   | 8 | 10| 12 | 14
//   |   |   |    |
//   |9  |11 |13  |15

class NGTableBorders {
  STACK_ALLOCATED();

 public:
  // |table_border_padding| as computed from css values.
  NGTableBorders(const ComputedStyle& table_style,
                 const NGBoxStrut& table_border_padding);

#if DCHECK_IS_ON()
  String DumpEdges();
  void ShowEdges();
#endif

  // Side of the style the collapsed border belongs to.
  enum class LogicalBoxSide {
    kInlineStart,
    kInlineEnd,
    kBlockStart,
    kBlockEnd
  };

  enum class EdgeSource { kNone, kCell, kRow, kSection, kColumn, kTable };

  // Specifies which side of CSS style defines the edge.
  // |kDoNotFill| means edge is empty, but should not be filled.
  enum class EdgeSide { kTop, kRight, kBottom, kLeft, kDoNotFill };

  // Edge is defined by a style, and box side. Box side specifies which
  // style border defines the edge.
  struct Edge {
    const ComputedStyle* style;
    EdgeSide edge_side;
  };

  static inline BoxSide ToBoxSide(EdgeSide edge_side) {
    DCHECK_NE(edge_side, EdgeSide::kDoNotFill);
    return static_cast<BoxSide>(edge_side);
  }

  static LayoutUnit BorderWidth(const ComputedStyle* style, BoxSide box_side) {
    switch (box_side) {
      case BoxSide::kLeft:
        return LayoutUnit(style->BorderLeftWidth());
      case BoxSide::kRight:
        return LayoutUnit(style->BorderRightWidth());
      case BoxSide::kTop:
        return LayoutUnit(style->BorderTopWidth());
      case BoxSide::kBottom:
        return LayoutUnit(style->BorderBottomWidth());
    }
  }

  static EBorderStyle BorderStyle(const ComputedStyle* style,
                                  BoxSide box_side) {
    switch (box_side) {
      case BoxSide::kLeft:
        return style->BorderLeftStyle();
      case BoxSide::kRight:
        return style->BorderRightStyle();
      case BoxSide::kTop:
        return style->BorderTopStyle();
      case BoxSide::kBottom:
        return style->BorderBottomStyle();
    }
  }

  static Color BorderColor(const ComputedStyle* style, BoxSide box_side) {
    switch (box_side) {
      case BoxSide::kLeft:
        return style->VisitedDependentColor(GetCSSPropertyBorderLeftColor());
      case BoxSide::kRight:
        return style->VisitedDependentColor(GetCSSPropertyBorderRightColor());
      case BoxSide::kTop:
        return style->VisitedDependentColor(GetCSSPropertyBorderTopColor());
      case BoxSide::kBottom:
        return style->VisitedDependentColor(GetCSSPropertyBorderBottomColor());
    }
  }

  using Edges = Vector<Edge>;

  struct Section {
    wtf_size_t start_row;
    wtf_size_t row_count;
  };

  bool IsEmpty() const { return edges_.IsEmpty(); }

  bool IsCollapsed() const { return is_collapsed_; }

  wtf_size_t EdgesPerRow() const { return edges_per_row_; }

  LayoutUnit BorderWidth(wtf_size_t edge_index) const {
    if (edge_index < edges_.size() && edges_[edge_index].style &&
        edges_[edge_index].edge_side != EdgeSide::kDoNotFill) {
      return BorderWidth(edges_[edge_index].style,
                         ToBoxSide(edges_[edge_index].edge_side));
    }
    return LayoutUnit();
  }

  NGBoxStrut TableBorderPadding() const {
    DCHECK(cached_table_border_padding_);
    return *cached_table_border_padding_;
  }

  // This method is necessary because collapsed table's border rect and
  // visual overflow rect use different borders.
  // Border rect uses inline start/end of the first row.
  // Visual rect uses largest inline start/end of the entire table.
  std::pair<LayoutUnit, LayoutUnit> GetCollapsedBorderVisualInlineStrut()
      const {
    return std::make_pair(collapsed_visual_inline_start_,
                          collapsed_visual_inline_end_);
  }

  NGBoxStrut CellBorder(wtf_size_t row,
                        wtf_size_t column,
                        wtf_size_t rowspan,
                        wtf_size_t colspan,
                        wtf_size_t section_index,
                        const ComputedStyle& cell_style) const;

  NGBoxStrut CellPadding(wtf_size_t row,
                         wtf_size_t column,
                         const ComputedStyle& cell_style) const;

  NGBoxStrut CellBorderPadding(wtf_size_t row,
                               wtf_size_t column,
                               wtf_size_t rowspan,
                               wtf_size_t colspan,
                               wtf_size_t section_index,
                               const ComputedStyle& cell_style) const {
    return CellBorder(row, column, rowspan, colspan, section_index,
                      cell_style) +
           CellPadding(row, column, cell_style);
  }

  void ComputeCollapsedTableBorderPadding(wtf_size_t table_row_count,
                                          wtf_size_t table_column_count);

  // |section_index| is only used to clamp rowspan. Only needed for
  // cells with rowspan > 1.
  void MergeBorders(wtf_size_t start_row,
                    wtf_size_t start_column,
                    wtf_size_t rowspan,
                    wtf_size_t colspan,
                    const ComputedStyle* source_style,
                    EdgeSource source,
                    wtf_size_t section_index = kNotFound);

  void AddSection(wtf_size_t start_row, wtf_size_t row_count) {
    sections_.push_back(Section{start_row, row_count});
  }

  Section GetSection(wtf_size_t section_index) {
    return sections_[section_index];
  }

  void SetLastColumnIndex(wtf_size_t last_column_index) {
    last_column_index_ = last_column_index;
  }

  Edges::const_iterator begin() const { return edges_.begin(); }

  Edges::const_iterator end() const { return edges_.end(); }

 private:
  wtf_size_t ClampColspan(wtf_size_t column, wtf_size_t colspan) const {
    DCHECK_GE(last_column_index_, column);
    return std::min(colspan, last_column_index_ - column);
  }

  // Clamps rowspan by section size.
  wtf_size_t ClampRowspan(wtf_size_t section_index,
                          wtf_size_t table_row_index,
                          wtf_size_t rowspan) const {
    if (rowspan <= 1)
      return rowspan;
    DCHECK_LT(section_index, sections_.size());
    return std::min(rowspan,
                    sections_[section_index].row_count -
                        (table_row_index - sections_[section_index].start_row));
  }

  NGBoxStrut GetCellBorders(wtf_size_t row,
                            wtf_size_t column,
                            wtf_size_t rowspan,
                            wtf_size_t colspan) const;

  void EnsureCellColumnFits(wtf_size_t cell_column);

  void EnsureCellRowFits(wtf_size_t cell_row);

  void MergeRowAxisBorder(wtf_size_t start_row,
                          wtf_size_t start_column,
                          wtf_size_t colspan,
                          const ComputedStyle* source_style,
                          LogicalBoxSide side);

  void MergeColumnAxisBorder(wtf_size_t start_row,
                             wtf_size_t start_column,
                             wtf_size_t rowspan,
                             const ComputedStyle* source_style,
                             LogicalBoxSide side);

  void MarkInnerBordersAsDoNotFill(wtf_size_t start_row,
                                   wtf_size_t start_column,
                                   wtf_size_t rowspan,
                                   wtf_size_t colspan);

  BoxSide LogicalToPhysical(LogicalBoxSide logical_side) const {
    // https://www.w3.org/TR/css-writing-modes-4/#logical-to-physical
    switch (logical_side) {
      case LogicalBoxSide::kInlineStart:
        switch (writing_direction_.GetWritingMode()) {
          case WritingMode::kHorizontalTb:
            return writing_direction_.Direction() == TextDirection::kLtr
                       ? BoxSide::kLeft
                       : BoxSide::kRight;
          case WritingMode::kVerticalLr:
          case WritingMode::kVerticalRl:
          case WritingMode::kSidewaysRl:
            return writing_direction_.Direction() == TextDirection::kLtr
                       ? BoxSide::kTop
                       : BoxSide::kBottom;
          case WritingMode::kSidewaysLr:
            return writing_direction_.Direction() == TextDirection::kLtr
                       ? BoxSide::kBottom
                       : BoxSide::kTop;
        }
      case LogicalBoxSide::kInlineEnd:
        switch (writing_direction_.GetWritingMode()) {
          case WritingMode::kHorizontalTb:
            return writing_direction_.Direction() == TextDirection::kLtr
                       ? BoxSide::kRight
                       : BoxSide::kLeft;
          case WritingMode::kVerticalLr:
          case WritingMode::kVerticalRl:
          case WritingMode::kSidewaysRl:
            return writing_direction_.Direction() == TextDirection::kLtr
                       ? BoxSide::kBottom
                       : BoxSide::kTop;
          case WritingMode::kSidewaysLr:
            return writing_direction_.Direction() == TextDirection::kLtr
                       ? BoxSide::kTop
                       : BoxSide::kBottom;
        }
      case LogicalBoxSide::kBlockStart:
        switch (writing_direction_.GetWritingMode()) {
          case WritingMode::kHorizontalTb:
            return BoxSide::kTop;
          case WritingMode::kVerticalLr:
            return BoxSide::kLeft;
          case WritingMode::kVerticalRl:
          case WritingMode::kSidewaysRl:
            return BoxSide::kRight;
          case WritingMode::kSidewaysLr:
            return BoxSide::kLeft;
        }
      case LogicalBoxSide::kBlockEnd:
        switch (writing_direction_.GetWritingMode()) {
          case WritingMode::kHorizontalTb:
            return BoxSide::kBottom;
          case WritingMode::kVerticalLr:
            return BoxSide::kRight;
          case WritingMode::kVerticalRl:
          case WritingMode::kSidewaysRl:
            return BoxSide::kLeft;
          case WritingMode::kSidewaysLr:
            return BoxSide::kRight;
        }
    }
  }

  Edges edges_;
  Vector<Section> sections_;
  wtf_size_t edges_per_row_ = 0;
  // Table border/padding are expensive to compute for collapsed tables.
  // We compute them once, and cache them.
  base::Optional<NGBoxStrut> cached_table_border_padding_;
  // Collapsed tables use first border to compute inline start/end.
  // Visual overflow use enclosing rectangle of all borders
  // to compute inline start/end.
  // |collapsed_visual_inline_start/end| are the visual rectangle values.
  LayoutUnit collapsed_visual_inline_start_;
  LayoutUnit collapsed_visual_inline_end_;
  WritingDirectionMode writing_direction_;
  // Cells cannot extrude beyond table grid size.
  // Rowspan and colspan sizes must be clamped to enforce this.
  wtf_size_t last_column_index_ = UINT_MAX;
  bool is_collapsed_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_BORDERS_H_
