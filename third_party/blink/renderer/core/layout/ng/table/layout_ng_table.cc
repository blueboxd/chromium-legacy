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

namespace {

inline bool NeedsTableSection(const LayoutObject& object) {
  // Return true if 'object' can't exist in an anonymous table without being
  // wrapped in a table section box.
  EDisplay display = object.StyleRef().Display();
  return display != EDisplay::kTableCaption &&
         display != EDisplay::kTableColumnGroup &&
         display != EDisplay::kTableColumn;
}

}  // namespace

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

void LayoutNGTable::AddChild(LayoutObject* child, LayoutObject* before_child) {
  bool wrap_in_anonymous_section = !child->IsTableCaption() &&
                                   !child->IsLayoutTableCol() &&
                                   !child->IsTableSection();

  if (!wrap_in_anonymous_section) {
    if (before_child && before_child->Parent() != this)
      before_child = SplitAnonymousBoxesAroundChild(before_child);
    LayoutBox::AddChild(child, before_child);
    return;
  }

  if (!before_child && LastChild() && LastChild()->IsTableSection() &&
      LastChild()->IsAnonymous() && !LastChild()->IsBeforeContent()) {
    LastChild()->AddChild(child);
    return;
  }

  if (before_child && !before_child->IsAnonymous() &&
      before_child->Parent() == this) {
    LayoutNGTableSection* section =
        DynamicTo<LayoutNGTableSection>(before_child->PreviousSibling());
    if (section && section->IsAnonymous()) {
      section->AddChild(child);
      return;
    }
  }

  LayoutObject* last_box = before_child;
  while (last_box && last_box->Parent()->IsAnonymous() &&
         !last_box->IsTableSection() && NeedsTableSection(*last_box))
    last_box = last_box->Parent();
  if (last_box && last_box->IsAnonymous() && last_box->IsTablePart() &&
      !IsAfterContent(last_box)) {
    if (before_child == last_box)
      before_child = last_box->SlowFirstChild();
    last_box->AddChild(child, before_child);
    return;
  }

  if (before_child && !before_child->IsTableSection() &&
      NeedsTableSection(*before_child))
    before_child = nullptr;

  LayoutBox* section =
      LayoutObjectFactory::CreateAnonymousTableSectionWithParent(*this);
  AddChild(section, before_child);
  section->AddChild(child);
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
