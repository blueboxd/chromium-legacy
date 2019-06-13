// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"

#include <algorithm>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_language_info.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/gfx/transform.h"

namespace ui {

AXNode::AXNode(AXNode::OwnerTree* tree,
               AXNode* parent,
               int32_t id,
               size_t index_in_parent)
    : tree_(tree),
      index_in_parent_(index_in_parent),
      unignored_child_count_(0),
      parent_(parent),
      language_info_(nullptr) {
  data_.id = id;
  // If this node is the root, use the given index_in_parent to provide
  // consistency.
  if (!parent)
    unignored_index_in_parent_ = index_in_parent_;
  else
    unignored_index_in_parent_ = 0;
}

AXNode::~AXNode() = default;

size_t AXNode::GetUnignoredChildCount() const {
  return unignored_child_count_;
}

AXNodeData&& AXNode::TakeData() {
  return std::move(data_);
}

AXNode* AXNode::GetUnignoredChildAtIndex(size_t index) const {
  size_t count = 0;
  for (AXNode* child : children_) {
    DCHECK_LE(count, index);
    size_t child_count = 1;
    if (child->data().HasState(ax::mojom::State::kIgnored)) {
      child_count = child->GetUnignoredChildCount();
      if (index < count + child_count)
        return child->GetUnignoredChildAtIndex(index - count);
    } else if (count == index) {
      return child;
    }
    count += child_count;
  }

  return nullptr;
}

AXNode* AXNode::GetUnignoredParent() const {
  AXNode* result = parent();
  while (result && result->data().HasState(ax::mojom::State::kIgnored))
    result = result->parent();
  return result;
}

size_t AXNode::GetUnignoredIndexInParent() const {
  DCHECK(!data().HasState(ax::mojom::State::kIgnored));
  return unignored_index_in_parent_;
}

bool AXNode::IsText() const {
  return data().role == ax::mojom::Role::kStaticText ||
         data().role == ax::mojom::Role::kLineBreak ||
         data().role == ax::mojom::Role::kInlineTextBox;
}

bool AXNode::IsLineBreak() const {
  return data().role == ax::mojom::Role::kLineBreak ||
         (IsText() && parent() &&
          parent()->data().role == ax::mojom::Role::kLineBreak);
}

void AXNode::SetData(const AXNodeData& src) {
  data_ = src;
}

void AXNode::SetLocation(int32_t offset_container_id,
                         const gfx::RectF& location,
                         gfx::Transform* transform) {
  data_.relative_bounds.offset_container_id = offset_container_id;
  data_.relative_bounds.bounds = location;
  if (transform)
    data_.relative_bounds.transform.reset(new gfx::Transform(*transform));
  else
    data_.relative_bounds.transform.reset(nullptr);
}

void AXNode::SetIndexInParent(size_t index_in_parent) {
  index_in_parent_ = index_in_parent;
}

void AXNode::UpdateUnignoredCachedValues() {
  if (!data().HasState(ax::mojom::State::kIgnored))
    UpdateUnignoredCachedValuesRecursive(0);
}

void AXNode::SwapChildren(std::vector<AXNode*>& children) {
  children.swap(children_);
}

void AXNode::Destroy() {
  delete this;
}

bool AXNode::IsDescendantOf(AXNode* ancestor) {
  if (this == ancestor)
    return true;
  else if (parent())
    return parent()->IsDescendantOf(ancestor);

  return false;
}

std::vector<int> AXNode::GetOrComputeLineStartOffsets() {
  std::vector<int> line_offsets;
  if (data().GetIntListAttribute(ax::mojom::IntListAttribute::kCachedLineStarts,
                                 &line_offsets))
    return line_offsets;

  int start_offset = 0;
  ComputeLineStartOffsets(&line_offsets, &start_offset);
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kCachedLineStarts,
                            line_offsets);
  return line_offsets;
}

void AXNode::ComputeLineStartOffsets(std::vector<int>* line_offsets,
                                     int* start_offset) const {
  DCHECK(line_offsets);
  DCHECK(start_offset);
  for (const AXNode* child : children()) {
    DCHECK(child);
    if (!child->children().empty()) {
      child->ComputeLineStartOffsets(line_offsets, start_offset);
      continue;
    }

    // Don't report if the first piece of text starts a new line or not.
    if (*start_offset && !child->data().HasIntAttribute(
                             ax::mojom::IntAttribute::kPreviousOnLineId)) {
      // If there are multiple objects with an empty accessible label at the
      // start of a line, only include a single line start offset.
      if (line_offsets->empty() || line_offsets->back() != *start_offset)
        line_offsets->push_back(*start_offset);
    }

    base::string16 text =
        child->data().GetString16Attribute(ax::mojom::StringAttribute::kName);
    *start_offset += static_cast<int>(text.length());
  }
}

const std::string& AXNode::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  const AXNode* current_node = this;
  do {
    if (current_node->data().HasStringAttribute(attribute))
      return current_node->data().GetStringAttribute(attribute);
    current_node = current_node->parent();
  } while (current_node);
  return base::EmptyString();
}

base::string16 AXNode::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return base::UTF8ToUTF16(GetInheritedStringAttribute(attribute));
}

AXLanguageInfo* AXNode::GetLanguageInfo() {
  return language_info_.get();
}

void AXNode::SetLanguageInfo(std::unique_ptr<AXLanguageInfo> lang_info) {
  language_info_ = std::move(lang_info);
}

std::string AXNode::GetLanguage() {
  // If we have been labelled with language info then rely on that.
  const AXLanguageInfo* lang_info = GetLanguageInfo();
  if (lang_info && !lang_info->language.empty())
    return lang_info->language;

  // Otherwise fallback to kLanguage attribute.
  const auto& lang_attr =
      GetInheritedStringAttribute(ax::mojom::StringAttribute::kLanguage);
  return lang_attr;
}

std::ostream& operator<<(std::ostream& stream, const AXNode& node) {
  return stream << node.data().ToString();
}

bool AXNode::IsTable() const {
  return IsTableLike(data().role);
}

base::Optional<int> AXNode::GetTableColCount() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;
  return int{table_info->col_count};
}

base::Optional<int> AXNode::GetTableRowCount() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;
  return int{table_info->row_count};
}

base::Optional<int> AXNode::GetTableAriaColCount() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;
  return table_info->aria_col_count;
}

base::Optional<int> AXNode::GetTableAriaRowCount() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;
  return table_info->aria_row_count;
}

base::Optional<int> AXNode::GetTableCellCount() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;

  return static_cast<int>(table_info->unique_cell_ids.size());
}

AXNode* AXNode::GetTableCellFromIndex(int index) const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return nullptr;

  // There is a table but there is no cell with the given index.
  if (index < 0 || size_t{index} >= table_info->unique_cell_ids.size()) {
    return nullptr;
  }

  return tree_->GetFromId(table_info->unique_cell_ids[size_t{index}]);
}

AXNode* AXNode::GetTableCaption() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return nullptr;

  return tree_->GetFromId(table_info->caption_id);
}

AXNode* AXNode::GetTableCellFromCoords(int row_index, int col_index) const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return nullptr;

  // There is a table but the given coordinates are outside the table.
  if (row_index < 0 || size_t{row_index} >= table_info->row_count ||
      col_index < 0 || size_t{col_index} >= table_info->col_count) {
    return nullptr;
  }

  return tree_->GetFromId(
      table_info->cell_ids[size_t{row_index}][size_t{col_index}]);
}

void AXNode::GetTableColHeaderNodeIds(
    int col_index,
    std::vector<int32_t>* col_header_ids) const {
  DCHECK(col_header_ids);
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return;

  if (col_index < 0 || size_t{col_index} >= table_info->col_count)
    return;

  for (size_t i = 0; i < table_info->col_headers[size_t{col_index}].size(); i++)
    col_header_ids->push_back(table_info->col_headers[size_t{col_index}][i]);
}

void AXNode::GetTableRowHeaderNodeIds(
    int row_index,
    std::vector<int32_t>* row_header_ids) const {
  DCHECK(row_header_ids);
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return;

  if (row_index < 0 || size_t{row_index} >= table_info->row_count)
    return;

  for (size_t i = 0; i < table_info->row_headers[size_t{row_index}].size(); i++)
    row_header_ids->push_back(table_info->row_headers[size_t{row_index}][i]);
}

void AXNode::GetTableUniqueCellIds(std::vector<int32_t>* cell_ids) const {
  DCHECK(cell_ids);
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return;

  cell_ids->assign(table_info->unique_cell_ids.begin(),
                   table_info->unique_cell_ids.end());
}

const std::vector<AXNode*>* AXNode::GetExtraMacNodes() const {
  // Should only be available on the table node itself, not any of its children.
  const AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return nullptr;

  return &table_info->extra_mac_nodes;
}

//
// Table row-like nodes.
//

bool AXNode::IsTableRow() const {
  return ui::IsTableRow(data().role);
}

base::Optional<int> AXNode::GetTableRowRowIndex() const {
  if (!IsTableRow())
    return base::nullopt;

  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;

  const auto& iter = table_info->row_id_to_index.find(id());
  if (iter != table_info->row_id_to_index.end())
    return int{iter->second};
  return base::nullopt;
}

#if defined(OS_MACOSX)

//
// Table column-like nodes. These nodes are only present on macOS.
//

bool AXNode::IsTableColumn() const {
  return ui::IsTableColumn(data().role);
}

base::Optional<int> AXNode::GetTableColColIndex() const {
  if (!IsTableColumn())
    return base::nullopt;

  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;

  int index = 0;
  for (const AXNode* node : table_info->extra_mac_nodes) {
    if (node == this)
      break;
    index++;
  }
  return index;
}

#endif  // defined(OS_MACOSX)

//
// Table cell-like nodes.
//

bool AXNode::IsTableCellOrHeader() const {
  return IsCellOrTableHeader(data().role);
}

base::Optional<int> AXNode::GetTableCellIndex() const {
  if (!IsTableCellOrHeader())
    return base::nullopt;

  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;

  const auto& iter = table_info->cell_id_to_index.find(id());
  if (iter != table_info->cell_id_to_index.end())
    return int{iter->second};
  return base::nullopt;
}

base::Optional<int> AXNode::GetTableCellColIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;

  base::Optional<int> index = GetTableCellIndex();
  if (!index)
    return base::nullopt;

  return int{table_info->cell_data_vector[*index].col_index};
}

base::Optional<int> AXNode::GetTableCellRowIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;

  base::Optional<int> index = GetTableCellIndex();
  if (!index)
    return base::nullopt;

  return int{table_info->cell_data_vector[*index].row_index};
}

base::Optional<int> AXNode::GetTableCellColSpan() const {
  // If it's not a table cell, don't return a col span.
  if (!IsTableCellOrHeader())
    return base::nullopt;

  // Otherwise, try to return a colspan, with 1 as the default if it's not
  // specified.
  int col_span;
  if (GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan, &col_span))
    return col_span;
  return 1;
}

base::Optional<int> AXNode::GetTableCellRowSpan() const {
  // If it's not a table cell, don't return a row span.
  if (!IsTableCellOrHeader())
    return base::nullopt;

  // Otherwise, try to return a row span, with 1 as the default if it's not
  // specified.
  int row_span;
  if (GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, &row_span))
    return row_span;
  return 1;
}

base::Optional<int> AXNode::GetTableCellAriaColIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;

  base::Optional<int> index = GetTableCellIndex();
  if (!index)
    return base::nullopt;

  return int{table_info->cell_data_vector[*index].aria_col_index};
}

base::Optional<int> AXNode::GetTableCellAriaRowIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;

  base::Optional<int> index = GetTableCellIndex();
  if (!index)
    return base::nullopt;

  return int{table_info->cell_data_vector[*index].aria_row_index};
}

void AXNode::GetTableCellColHeaderNodeIds(
    std::vector<int32_t>* col_header_ids) const {
  DCHECK(col_header_ids);
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info || table_info->col_count <= 0)
    return;

  base::Optional<int> col_index = GetTableCellColIndex();
  // If this node is not a cell, then return the headers for the first column.
  for (size_t i = 0; i < table_info->col_headers[col_index.value_or(0)].size();
       i++) {
    col_header_ids->push_back(
        table_info->col_headers[col_index.value_or(0)][i]);
  }
}

void AXNode::GetTableCellColHeaders(std::vector<AXNode*>* col_headers) const {
  DCHECK(col_headers);

  std::vector<int32_t> col_header_ids;
  GetTableCellColHeaderNodeIds(&col_header_ids);
  IdVectorToNodeVector(col_header_ids, col_headers);
}

void AXNode::GetTableCellRowHeaderNodeIds(
    std::vector<int32_t>* row_header_ids) const {
  DCHECK(row_header_ids);
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info || table_info->row_count <= 0)
    return;

  base::Optional<int> row_index = GetTableCellRowIndex();
  // If this node is not a cell, then return the headers for the first row.
  for (size_t i = 0; i < table_info->row_headers[row_index.value_or(0)].size();
       i++) {
    row_header_ids->push_back(
        table_info->row_headers[row_index.value_or(0)][i]);
  }
}

void AXNode::GetTableCellRowHeaders(std::vector<AXNode*>* row_headers) const {
  DCHECK(row_headers);

  std::vector<int32_t> row_header_ids;
  GetTableCellRowHeaderNodeIds(&row_header_ids);
  IdVectorToNodeVector(row_header_ids, row_headers);
}

bool AXNode::IsCellOrHeaderOfARIATable() const {
  if (!IsTableCellOrHeader())
    return false;

  const AXNode* node = this;
  while (node && !node->IsTable())
    node = node->parent();
  if (!node)
    return false;

  return node->data().role == ax::mojom::Role::kTable;
}

bool AXNode::IsCellOrHeaderOfARIAGrid() const {
  if (!IsTableCellOrHeader())
    return false;

  const AXNode* node = this;
  while (node && !node->IsTable())
    node = node->parent();
  if (!node)
    return false;

  return node->data().role == ax::mojom::Role::kGrid ||
         node->data().role == ax::mojom::Role::kTreeGrid;
}

AXTableInfo* AXNode::GetAncestorTableInfo() const {
  const AXNode* node = this;
  while (node && !node->IsTable())
    node = node->parent();
  if (node)
    return tree_->GetTableInfo(node);
  return nullptr;
}

void AXNode::IdVectorToNodeVector(std::vector<int32_t>& ids,
                                  std::vector<AXNode*>* nodes) const {
  for (int32_t id : ids) {
    AXNode* node = tree_->GetFromId(id);
    if (node)
      nodes->push_back(node);
  }
}

bool AXNode::IsOrderedSetItem() const {
  return ui::IsItemLike(data().role);
}

bool AXNode::IsOrderedSet() const {
  return ui::IsSetLike(data().role);
}

// pos_in_set and set_size related functions.
// Uses AXTree's cache to calculate node's pos_in_set.
base::Optional<int> AXNode::GetPosInSet() {
  // Only allow this to be called on nodes that can hold pos_in_set values,
  // which are defined in the ARIA spec.
  if (!IsOrderedSetItem()) {
    return base::nullopt;
  }

  const AXNode* ordered_set = GetOrderedSet();
  if (!ordered_set) {
    return base::nullopt;
  }

  // If tree is being updated, return no value.
  if (tree()->GetTreeUpdateInProgressState())
    return base::nullopt;

  // See AXTree::GetPosInSet
  return tree_->GetPosInSet(*this, ordered_set);
}

// Uses AXTree's cache to calculate node's set_size.
base::Optional<int> AXNode::GetSetSize() {
  // Only allow this to be called on nodes that can hold set_size values, which
  // are defined in the ARIA spec.
  if (!(IsOrderedSetItem() || IsOrderedSet()))
    return base::nullopt;

  // If node is item-like, find its outerlying ordered set. Otherwise,
  // this node is the ordered set.
  const AXNode* ordered_set = this;
  if (IsItemLike(data().role))
    ordered_set = GetOrderedSet();
  if (!ordered_set)
    return base::nullopt;

  // If tree is being updated, return no value.
  if (tree()->GetTreeUpdateInProgressState())
    return base::nullopt;

  // See AXTree::GetSetSize
  return tree_->GetSetSize(*this, ordered_set);
}

// Returns true if the role of ordered set matches the role of item.
// Returns false otherwise.
bool AXNode::SetRoleMatchesItemRole(const AXNode* ordered_set) const {
  ax::mojom::Role item_role = data().role;

  // Switch on role of ordered set
  switch (ordered_set->data().role) {
    case ax::mojom::Role::kFeed:
      return item_role == ax::mojom::Role::kArticle;

    case ax::mojom::Role::kList:
      return item_role == ax::mojom::Role::kListItem;

    case ax::mojom::Role::kGroup:
      return item_role == ax::mojom::Role::kListItem ||
             item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kTreeItem;

    case ax::mojom::Role::kMenu:
      return item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;

    case ax::mojom::Role::kMenuBar:
      return item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;

    case ax::mojom::Role::kTabList:
      return item_role == ax::mojom::Role::kTab;

    case ax::mojom::Role::kTree:
      return item_role == ax::mojom::Role::kTreeItem;

    case ax::mojom::Role::kListBox:
      return item_role == ax::mojom::Role::kListBoxOption;

    case ax::mojom::Role::kMenuListPopup:
      return item_role == ax::mojom::Role::kMenuListOption;

    case ax::mojom::Role::kRadioGroup:
      return item_role == ax::mojom::Role::kRadioButton;

    case ax::mojom::Role::kDescriptionList:
      // Only the term for each description list entry should receive posinset
      // and setsize.
      return item_role == ax::mojom::Role::kDescriptionListTerm ||
             item_role == ax::mojom::Role::kTerm;

    default:
      return false;
  }
}

int AXNode::UpdateUnignoredCachedValuesRecursive(int startIndex) {
  int count = 0;
  for (AXNode* child : children_) {
    if (child->data().HasState(ax::mojom::State::kIgnored)) {
      child->unignored_index_in_parent_ = 0;
      count += child->UpdateUnignoredCachedValuesRecursive(startIndex + count);
    } else {
      child->unignored_index_in_parent_ = startIndex + count++;
    }
  }
  unignored_child_count_ = count;
  return count;
}

// Finds ordered set that immediately contains node.
// Is not required for set's role to match node's role.
AXNode* AXNode::GetOrderedSet() const {
  AXNode* result = parent();

  // Continue walking up while parent is invalid, ignored, or is a generic
  // container.
  while (result && (result->data().HasState(ax::mojom::State::kIgnored) ||
                    result->data().role == ax::mojom::Role::kGenericContainer ||
                    result->data().role == ax::mojom::Role::kIgnored)) {
    result = result->parent();
  }
  return result;
}

}  // namespace ui
