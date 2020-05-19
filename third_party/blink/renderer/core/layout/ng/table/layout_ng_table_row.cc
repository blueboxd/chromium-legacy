// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"

#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row_interface.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section_interface.h"

namespace blink {

LayoutNGTableRow::LayoutNGTableRow(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

bool LayoutNGTableRow::IsEmpty() const {
  return !FirstChild();
}

LayoutBox* LayoutNGTableRow::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  return LayoutObjectFactory::CreateAnonymousTableRowWithParent(*parent);
}

unsigned LayoutNGTableRow::RowIndex() const {
  unsigned index = 0;
  for (LayoutObject* child = Parent()->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child == this)
      return index;
    ++index;
  }
  NOTREACHED();
  return 0;
}

LayoutNGTableCell* LayoutNGTableRow::LastCell() const {
  return To<LayoutNGTableCell>(LastChild());
}

LayoutNGTableSectionInterface* LayoutNGTableRow::SectionInterface() const {
  return To<LayoutNGTableSection>(Parent());
}

LayoutNGTableRowInterface* LayoutNGTableRow::PreviousRowInterface() const {
  return ToInterface<LayoutNGTableRowInterface>(PreviousSibling());
}

LayoutNGTableRowInterface* LayoutNGTableRow::NextRowInterface() const {
  return ToInterface<LayoutNGTableRowInterface>(NextSibling());
}

LayoutNGTableCellInterface* LayoutNGTableRow::FirstCellInterface() const {
  return ToInterface<LayoutNGTableCellInterface>(FirstChild());
}

LayoutNGTableCellInterface* LayoutNGTableRow::LastCellInterface() const {
  return ToInterface<LayoutNGTableCellInterface>(LastChild());
}

}  // namespace blink
