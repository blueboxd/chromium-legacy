// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"

#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"

namespace blink {

LayoutNGTable::LayoutNGTable(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

wtf_size_t LayoutNGTable::ColumnCount() const {
  // TODO(atotic) land implementation.
  NOTIMPLEMENTED();
  return 0;
}

void LayoutNGTable::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }
  UpdateInFlowBlockLayout();
}

LayoutBox* LayoutNGTable::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  return LayoutObjectFactory::CreateAnonymousTableWithParent(*parent);
}

bool LayoutNGTable::IsFirstCell(const LayoutNGTableCellInterface& cell) const {
  const LayoutNGTableRowInterface* row = cell.RowInterface();
  if (row->FirstCellInterface() != &cell)
    return false;
  const LayoutNGTableSectionInterface* section = row->SectionInterface();
  if (section->FirstRowInterface() != row)
    return false;
  // TODO(atotic) Should be TopNonEmptyInterface?
  if (TopSectionInterface() != section)
    return false;
  return true;
}

// Only called from AXLayoutObject::IsDataTable()
LayoutNGTableSectionInterface* LayoutNGTable::FirstBodyInterface() const {
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (child->StyleRef().Display() == EDisplay::kTableRowGroup)
      return ToInterface<LayoutNGTableSectionInterface>(child);
  }
  return nullptr;
}

// Called from many AXLayoutObject methods.
LayoutNGTableSectionInterface* LayoutNGTable::TopSectionInterface() const {
  // TODO(atotic) implement.
  return nullptr;
}

// Called from many AXLayoutObject methods.
LayoutNGTableSectionInterface* LayoutNGTable::SectionBelowInterface(
    const LayoutNGTableSectionInterface* target,
    SkipEmptySectionsValue skip) const {
  // TODO(atotic) implement.
  return nullptr;
}

}  // namespace blink
