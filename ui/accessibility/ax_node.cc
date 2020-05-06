// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"

#include <algorithm>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/gfx/transform.h"

namespace ui {

constexpr AXNode::AXID AXNode::kInvalidAXID;

AXNode::AXNode(AXNode::OwnerTree* tree,
               AXNode* parent,
               int32_t id,
               size_t index_in_parent,
               size_t unignored_index_in_parent)
    : tree_(tree),
      index_in_parent_(index_in_parent),
      unignored_index_in_parent_(unignored_index_in_parent),
      parent_(parent) {
  data_.id = id;
}

AXNode::~AXNode() = default;

size_t AXNode::GetUnignoredChildCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return unignored_child_count_;
}

AXNodeData&& AXNode::TakeData() {
  return std::move(data_);
}

AXNode* AXNode::GetUnignoredChildAtIndex(size_t index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  size_t count = 0;
  for (auto it = UnignoredChildrenBegin(); it != UnignoredChildrenEnd(); ++it) {
    if (count == index)
      return it.get();
    ++count;
  }
  return nullptr;
}

AXNode* AXNode::GetUnignoredParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  AXNode* result = parent();
  while (result && result->IsIgnored())
    result = result->parent();
  return result;
}

size_t AXNode::GetUnignoredIndexInParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return unignored_index_in_parent_;
}

size_t AXNode::GetIndexInParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return index_in_parent_;
}

AXNode* AXNode::GetFirstUnignoredChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return ComputeFirstUnignoredChildRecursive();
}

AXNode* AXNode::GetLastUnignoredChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return ComputeLastUnignoredChildRecursive();
}

AXNode* AXNode::GetDeepestFirstUnignoredChild() const {
  if (!GetUnignoredChildCount())
    return nullptr;

  AXNode* deepest_child = GetFirstUnignoredChild();
  while (deepest_child->GetUnignoredChildCount()) {
    deepest_child = deepest_child->GetFirstUnignoredChild();
  }

  return deepest_child;
}

AXNode* AXNode::GetDeepestLastUnignoredChild() const {
  if (!GetUnignoredChildCount())
    return nullptr;

  AXNode* deepest_child = GetLastUnignoredChild();
  while (deepest_child->GetUnignoredChildCount()) {
    deepest_child = deepest_child->GetLastUnignoredChild();
  }

  return deepest_child;
}

// Search for the next sibling of this node, skipping over any ignored nodes
// encountered.
//
// In our search:
//   If we find an ignored sibling, we consider its children as our siblings.
//   If we run out of siblings, we consider an ignored parent's siblings as our
//     own siblings.
//
// Note: this behaviour of 'skipping over' an ignored node makes this subtly
// different to finding the next (direct) sibling which is unignored.
//
// Consider a tree, where (i) marks a node as ignored:
//
//   1
//   ├── 2
//   ├── 3(i)
//   │   └── 5
//   └── 4
//
// The next sibling of node 2 is node 3, which is ignored.
// The next unignored sibling of node 2 could be either:
//  1) node 4 - next unignored sibling in the literal tree, or
//  2) node 5 - next unignored sibling in the logical document.
//
// There is no next sibling of node 5.
// The next unignored sibling of node 5 could be either:
//  1) null   - no next sibling in the literal tree, or
//  2) node 4 - next unignored sibling in the logical document.
//
// In both cases, this method implements approach (2).
//
// TODO(chrishall): Can we remove this non-reflexive case by forbidding
//   GetNextUnignoredSibling calls on an ignored started node?
// Note: this means that Next/Previous-UnignoredSibling are not reflexive if
// either of the nodes in question are ignored. From above we get an example:
//   NextUnignoredSibling(3)     is 4, but
//   PreviousUnignoredSibling(4) is 5.
//
// The view of unignored siblings for node 3 includes both node 2 and node 4:
//    2 <-- [3(i)] --> 4
//
// Whereas nodes 2, 5, and 4 do not consider node 3 to be an unignored sibling:
// null <-- [2] --> 5
//    2 <-- [5] --> 4
//    5 <-- [4] --> null
AXNode* AXNode::GetNextUnignoredSibling() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXNode* current = this;

  // If there are children of the |current| node still to consider.
  bool considerChildren = false;

  while (current) {
    // A |candidate| sibling to consider.
    // If it is unignored then we have found our result.
    // Otherwise promote it to |current| and consider its children.
    AXNode* candidate;

    if (considerChildren && (candidate = current->GetFirstChild())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;

    } else if ((candidate = current->GetNextSibling())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;
      // Look through the ignored candidate node to consider their children as
      // though they were siblings.
      considerChildren = true;

    } else {
      // Continue our search through a parent iff they are ignored.
      //
      // If |current| has an ignored parent, then we consider the parent's
      // siblings as though they were siblings of |current|.
      //
      // Given a tree:
      //   1
      //   ├── 2(?)
      //   │   └── [4]
      //   └── 3
      //
      // Node 4's view of siblings:
      //   literal tree:   null <-- [4] --> null
      //
      // If node 2 is not ignored, then node 4's view doesn't change, and we
      // have no more nodes to consider:
      //   unignored tree: null <-- [4] --> null
      //
      // If instead node 2 is ignored, then node 4's view of siblings grows to
      // include node 3, and we have more nodes to consider:
      //   unignored tree: null <-- [4] --> 3
      current = current->parent();
      if (!current || !current->IsIgnored())
        return nullptr;

      // We have already considered all relevant descendants of |current|.
      considerChildren = false;
    }
  }

  return nullptr;
}

// Search for the previous sibling of this node, skipping over any ignored nodes
// encountered.
//
// In our search for a sibling:
//   If we find an ignored sibling, we may consider its children as siblings.
//   If we run out of siblings, we may consider an ignored parent's siblings as
//     our own.
//
// See the documentation for |GetNextUnignoredSibling| for more details.
AXNode* AXNode::GetPreviousUnignoredSibling() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXNode* current = this;

  // If there are children of the |current| node still to consider.
  bool considerChildren = false;

  while (current) {
    // A |candidate| sibling to consider.
    // If it is unignored then we have found our result.
    // Otherwise promote it to |current| and consider its children.
    AXNode* candidate;

    if (considerChildren && (candidate = current->GetLastChild())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;

    } else if ((candidate = current->GetPreviousSibling())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;
      // Look through the ignored candidate node to consider their children as
      // though they were siblings.
      considerChildren = true;

    } else {
      // Continue our search through a parent iff they are ignored.
      //
      // If |current| has an ignored parent, then we consider the parent's
      // siblings as though they were siblings of |current|.
      //
      // Given a tree:
      //   1
      //   ├── 2
      //   └── 3(?)
      //       └── [4]
      //
      // Node 4's view of siblings:
      //   literal tree:   null <-- [4] --> null
      //
      // If node 3 is not ignored, then node 4's view doesn't change, and we
      // have no more nodes to consider:
      //   unignored tree: null <-- [4] --> null
      //
      // If instead node 3 is ignored, then node 4's view of siblings grows to
      // include node 2, and we have more nodes to consider:
      //   unignored tree:    2 <-- [4] --> null
      current = current->parent();
      if (!current || !current->IsIgnored())
        return nullptr;

      // We have already considered all relevant descendants of |current|.
      considerChildren = false;
    }
  }

  return nullptr;
}

AXNode* AXNode::GetNextUnignoredInTreeOrder() const {
  if (GetUnignoredChildCount())
    return GetFirstUnignoredChild();

  const AXNode* node = this;
  while (node) {
    AXNode* sibling = node->GetNextUnignoredSibling();
    if (sibling)
      return sibling;

    node = node->GetUnignoredParent();
  }

  return nullptr;
}

AXNode* AXNode::GetPreviousUnignoredInTreeOrder() const {
  AXNode* sibling = GetPreviousUnignoredSibling();
  if (!sibling)
    return GetUnignoredParent();

  if (sibling->GetUnignoredChildCount())
    return sibling->GetDeepestLastUnignoredChild();

  return sibling;
}

AXNode::UnignoredChildIterator AXNode::UnignoredChildrenBegin() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return UnignoredChildIterator(this, GetFirstUnignoredChild());
}

AXNode::UnignoredChildIterator AXNode::UnignoredChildrenEnd() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return UnignoredChildIterator(this, nullptr);
}

// The first (direct) child, ignored or unignored.
AXNode* AXNode::GetFirstChild() const {
  if (children().size() == 0)
    return nullptr;
  return children()[0];
}

// The last (direct) child, ignored or unignored.
AXNode* AXNode::GetLastChild() const {
  size_t n = children().size();
  if (n == 0)
    return nullptr;
  return children()[n - 1];
}

// The previous (direct) sibling, ignored or unignored.
AXNode* AXNode::GetPreviousSibling() const {
  // Root nodes lack a parent, their index_in_parent should be 0.
  DCHECK(!parent() ? index_in_parent() == 0 : true);
  size_t index = index_in_parent();
  if (index == 0)
    return nullptr;
  return parent()->children()[index - 1];
}

// The next (direct) sibling, ignored or unignored.
AXNode* AXNode::GetNextSibling() const {
  if (!parent())
    return nullptr;
  size_t nextIndex = index_in_parent() + 1;
  if (nextIndex >= parent()->children().size())
    return nullptr;
  return parent()->children()[nextIndex];
}

bool AXNode::IsText() const {
  return data().role == ax::mojom::Role::kStaticText ||
         data().role == ax::mojom::Role::kLineBreak ||
         data().role == ax::mojom::Role::kInlineTextBox;
}

bool AXNode::IsLineBreak() const {
  return data().role == ax::mojom::Role::kLineBreak ||
         (data().role == ax::mojom::Role::kInlineTextBox &&
          data().GetBoolAttribute(
              ax::mojom::BoolAttribute::kIsLineBreakingObject));
}

void AXNode::SetData(const AXNodeData& src) {
  data_ = src;
}

void AXNode::SetLocation(int32_t offset_container_id,
                         const gfx::RectF& location,
                         gfx::Transform* transform) {
  data_.relative_bounds.offset_container_id = offset_container_id;
  data_.relative_bounds.bounds = location;
  if (transform) {
    data_.relative_bounds.transform =
        std::make_unique<gfx::Transform>(*transform);
  } else {
    data_.relative_bounds.transform.reset();
  }
}

void AXNode::SetIndexInParent(size_t index_in_parent) {
  index_in_parent_ = index_in_parent;
}

void AXNode::UpdateUnignoredCachedValues() {
  if (!IsIgnored())
    UpdateUnignoredCachedValuesRecursive(0);
}

void AXNode::SwapChildren(std::vector<AXNode*>* children) {
  children->swap(children_);
}

void AXNode::Destroy() {
  delete this;
}

bool AXNode::IsDescendantOf(const AXNode* ancestor) const {
  if (this == ancestor)
    return true;
  if (parent())
    return parent()->IsDescendantOf(ancestor);

  return false;
}

std::vector<int> AXNode::GetOrComputeLineStartOffsets() {
  std::vector<int> line_offsets;
  if (data().GetIntListAttribute(ax::mojom::IntListAttribute::kCachedLineStarts,
                                 &line_offsets)) {
    return line_offsets;
  }

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

AXLanguageInfo* AXNode::GetLanguageInfo() const {
  return language_info_.get();
}

void AXNode::SetLanguageInfo(std::unique_ptr<AXLanguageInfo> lang_info) {
  language_info_ = std::move(lang_info);
}

void AXNode::ClearLanguageInfo() {
  language_info_.reset();
}

std::string AXNode::GetLanguage() const {
  // Walk up tree considering both detected and author declared languages.
  for (const AXNode* cur = this; cur; cur = cur->parent()) {
    // If language detection has assigned a language then we prefer that.
    const AXLanguageInfo* lang_info = cur->GetLanguageInfo();
    if (lang_info && !lang_info->language.empty()) {
      return lang_info->language;
    }

    // If the page author has declared a language attribute we fallback to that.
    const AXNodeData& data = cur->data();
    if (data.HasStringAttribute(ax::mojom::StringAttribute::kLanguage)) {
      return data.GetStringAttribute(ax::mojom::StringAttribute::kLanguage);
    }
  }

  return std::string();
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
  return base::make_optional(table_info->aria_col_count);
}

base::Optional<int> AXNode::GetTableAriaRowCount() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;
  return base::make_optional(table_info->aria_row_count);
}

base::Optional<int> AXNode::GetTableCellCount() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;

  return static_cast<int>(table_info->unique_cell_ids.size());
}

base::Optional<bool> AXNode::GetTableHasColumnOrRowHeaderNode() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return base::nullopt;

  return !table_info->all_headers.empty();
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
  if (iter == table_info->row_id_to_index.end())
    return base::nullopt;
  return int{iter->second};
}

std::vector<AXNode::AXID> AXNode::GetTableRowNodeIds() const {
  std::vector<AXNode::AXID> row_node_ids;
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return row_node_ids;

  for (AXNode* node : table_info->row_nodes)
    row_node_ids.push_back(node->data().id);

  return row_node_ids;
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

  // If this node is not a cell, then return the headers for the first column.
  int col_index = GetTableCellColIndex().value_or(0);
  const auto& col = table_info->col_headers[col_index];
  for (int header : col)
    col_header_ids->push_back(header);
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

  // If this node is not a cell, then return the headers for the first row.
  int row_index = GetTableCellRowIndex().value_or(0);
  const auto& row = table_info->row_headers[row_index];
  for (int header : row)
    row_header_ids->push_back(header);
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

void AXNode::IdVectorToNodeVector(const std::vector<int32_t>& ids,
                                  std::vector<AXNode*>* nodes) const {
  for (int32_t id : ids) {
    AXNode* node = tree_->GetFromId(id);
    if (node)
      nodes->push_back(node);
  }
}

base::Optional<int> AXNode::GetHierarchicalLevel() const {
  int hierarchical_level =
      GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);

  // According to the WAI_ARIA spec, a defined hierarchical level value is
  // greater than 0.
  // https://www.w3.org/TR/wai-aria-1.1/#aria-level
  if (hierarchical_level > 0)
    return hierarchical_level;

  return base::nullopt;
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
  if (!IsOrderedSetItem() || IsIgnored())
    return base::nullopt;

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
  if ((!IsOrderedSetItem() && !IsOrderedSet()) || IsIgnored())
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
  int32_t set_size = tree_->GetSetSize(*this, ordered_set);
  if (set_size < 0)
    return base::nullopt;
  return set_size;
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
      return item_role == ax::mojom::Role::kComment ||
             item_role == ax::mojom::Role::kListItem ||
             item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kListBoxOption ||
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
      return item_role == ax::mojom::Role::kMenuListOption ||
             item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;
    case ax::mojom::Role::kRadioGroup:
      return item_role == ax::mojom::Role::kRadioButton;
    case ax::mojom::Role::kDescriptionList:
      // Only the term for each description list entry should receive posinset
      // and setsize.
      return item_role == ax::mojom::Role::kDescriptionListTerm ||
             item_role == ax::mojom::Role::kTerm;
    case ax::mojom::Role::kPopUpButton:
      // kPopUpButtons can wrap a kMenuListPopUp.
      return item_role == ax::mojom::Role::kMenuListPopup;
    default:
      return false;
  }
}

bool AXNode::IsIgnoredContainerForOrderedSet() const {
  return IsIgnored() || data().role == ax::mojom::Role::kListItem ||
         data().role == ax::mojom::Role::kGenericContainer ||
         data().role == ax::mojom::Role::kUnknown;
}

int AXNode::UpdateUnignoredCachedValuesRecursive(int startIndex) {
  int count = 0;
  for (AXNode* child : children_) {
    if (child->IsIgnored()) {
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
  // Continue walking up while parent is invalid, ignored, a generic container,
  // or unknown.
  while (result && (result->IsIgnored() ||
                    result->data().role == ax::mojom::Role::kGenericContainer ||
                    result->data().role == ax::mojom::Role::kUnknown)) {
    result = result->parent();
  }
  return result;
}

AXNode* AXNode::ComputeLastUnignoredChildRecursive() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (children().empty())
    return nullptr;

  for (int i = static_cast<int>(children().size()) - 1; i >= 0; --i) {
    AXNode* child = children_[i];
    if (!child->IsIgnored())
      return child;

    AXNode* descendant = child->ComputeLastUnignoredChildRecursive();
    if (descendant)
      return descendant;
  }
  return nullptr;
}

AXNode* AXNode::ComputeFirstUnignoredChildRecursive() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  for (size_t i = 0; i < children().size(); i++) {
    AXNode* child = children_[i];
    if (!child->IsIgnored())
      return child;

    AXNode* descendant = child->ComputeFirstUnignoredChildRecursive();
    if (descendant)
      return descendant;
  }
  return nullptr;
}

bool AXNode::IsIgnored() const {
  return data().IsIgnored();
}

bool AXNode::IsInListMarker() const {
  if (data().role == ax::mojom::Role::kListMarker)
    return true;

  // List marker node's children can only be text elements.
  if (!IsText())
    return false;

  // There is no need to iterate over all the ancestors of the current anchor
  // since a list marker node only has children on 2 levels.
  // i.e.:
  // AXLayoutObject role=kListMarker
  // ++StaticText
  // ++++InlineTextBox
  AXNode* parent_node = GetUnignoredParent();
  if (parent_node && parent_node->data().role == ax::mojom::Role::kListMarker)
    return true;

  AXNode* grandparent_node = parent_node->GetUnignoredParent();
  return grandparent_node &&
         grandparent_node->data().role == ax::mojom::Role::kListMarker;
}

bool AXNode::IsCollapsedMenuListPopUpButton() const {
  if (data().role != ax::mojom::Role::kPopUpButton ||
      !data().HasState(ax::mojom::State::kCollapsed)) {
    return false;
  }

  // When a popup button contains a menu list popup, its only child is unignored
  // and is a menu list popup.
  AXNode* node = GetFirstUnignoredChild();
  if (!node)
    return false;

  return node->data().role == ax::mojom::Role::kMenuListPopup;
}

AXNode* AXNode::GetCollapsedMenuListPopUpButtonAncestor() const {
  AXNode* node = GetOrderedSet();

  if (!node)
    return nullptr;

  // The ordered set returned is either the popup element child of the popup
  // button (e.g., the AXMenuListPopup) or the popup button itself. We need
  // |node| to point to the popup button itself.
  if (node->data().role != ax::mojom::Role::kPopUpButton) {
    node = node->parent();
    if (!node)
      return nullptr;
  }

  return node->IsCollapsedMenuListPopUpButton() ? node : nullptr;
}

}  // namespace ui
