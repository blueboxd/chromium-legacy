// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/test_ax_node_wrapper.h"

#include <unordered_map>
#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

namespace {

// A global map from AXNodes to TestAXNodeWrappers.
std::unordered_map<AXNode*, TestAXNodeWrapper*> g_node_to_wrapper_map;

// A global coordinate offset.
gfx::Vector2d g_offset;

// A global map that stores which node is focused on a determined tree.
//   - If a tree has no node being focused, there shouldn't be any entry on the
//     map associated with such tree, i.e. a pair {tree, nullptr} is invalid.
//   - For testing purposes, assume there is a single node being focused in the
//     entire tree and if such node is deleted, focus is completely lost.
std::unordered_map<AXTree*, AXNode*> g_focused_node_in_tree;

// A global indicating the last node which ShowContextMenu was called from.
AXNode* g_node_from_last_show_context_menu;

// A global indicating the last node which accessibility perform action
// default action was called from.
AXNode* g_node_from_last_default_action;

// A simple implementation of AXTreeObserver to catch when AXNodes are
// deleted so we can delete their wrappers.
class TestAXTreeObserver : public AXTreeObserver {
 private:
  void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) override {
    auto iter = g_node_to_wrapper_map.find(node);
    if (iter != g_node_to_wrapper_map.end()) {
      TestAXNodeWrapper* wrapper = iter->second;
      delete wrapper;
      g_node_to_wrapper_map.erase(iter->first);
    }
  }
};

TestAXTreeObserver g_ax_tree_observer;

}  // namespace

// static
TestAXNodeWrapper* TestAXNodeWrapper::GetOrCreate(AXTree* tree, AXNode* node) {
  if (!tree || !node)
    return nullptr;

  if (!tree->HasObserver(&g_ax_tree_observer))
    tree->AddObserver(&g_ax_tree_observer);
  auto iter = g_node_to_wrapper_map.find(node);
  if (iter != g_node_to_wrapper_map.end())
    return iter->second;
  TestAXNodeWrapper* wrapper = new TestAXNodeWrapper(tree, node);
  g_node_to_wrapper_map[node] = wrapper;
  return wrapper;
}

// static
void TestAXNodeWrapper::SetGlobalCoordinateOffset(const gfx::Vector2d& offset) {
  g_offset = offset;
}

// static
const AXNode* TestAXNodeWrapper::GetNodeFromLastShowContextMenu() {
  return g_node_from_last_show_context_menu;
}

// static
const AXNode* TestAXNodeWrapper::GetNodeFromLastDefaultAction() {
  return g_node_from_last_default_action;
}

TestAXNodeWrapper::~TestAXNodeWrapper() {
  platform_node_->Destroy();
}

const AXNodeData& TestAXNodeWrapper::GetData() const {
  return node_->data();
}

const AXTreeData& TestAXNodeWrapper::GetTreeData() const {
  return tree_->data();
}

AXNodePosition::AXPositionInstance TestAXNodeWrapper::CreateTextPositionAt(
    int offset,
    ax::mojom::TextAffinity affinity) const {
  return ui::AXNodePosition::CreateTextPosition(GetTreeData().tree_id,
                                                node_->id(), offset, affinity);
}

gfx::NativeViewAccessible TestAXNodeWrapper::GetParent() {
  TestAXNodeWrapper* parent_wrapper = GetOrCreate(tree_, node_->parent());
  return parent_wrapper ?
      parent_wrapper->ax_platform_node()->GetNativeViewAccessible() :
      nullptr;
}

int TestAXNodeWrapper::GetChildCount() {
  return InternalChildCount();
}

gfx::NativeViewAccessible TestAXNodeWrapper::ChildAtIndex(int index) {
  TestAXNodeWrapper* child_wrapper = InternalGetChild(index);
  return child_wrapper ?
      child_wrapper->ax_platform_node()->GetNativeViewAccessible() :
      nullptr;
}

gfx::Rect TestAXNodeWrapper::GetBoundsRect(
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  switch (coordinate_system) {
    case AXCoordinateSystem::kScreen: {
      // We could optionally add clipping here if ever needed.
      gfx::RectF bounds = GetLocation();
      bounds.Offset(g_offset);
      return gfx::ToEnclosingRect(bounds);
    }
    case AXCoordinateSystem::kRootFrame:
    case AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

gfx::Rect TestAXNodeWrapper::GetInnerTextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  switch (coordinate_system) {
    case AXCoordinateSystem::kScreen: {
      gfx::RectF bounds = GetLocation();
      // This implementation currently only deals with text node that has role
      // kInlineTextBox and kStaticText.
      // For test purposes, assume node with kStaticText always has a single
      // child with role kInlineTextBox.
      if (GetData().role == ax::mojom::Role::kInlineTextBox) {
        bounds = GetInlineTextRect(start_offset, end_offset);
      } else if (GetData().role == ax::mojom::Role::kStaticText &&
                 InternalChildCount() > 0) {
        TestAXNodeWrapper* child = InternalGetChild(0);
        if (child != nullptr &&
            child->GetData().role == ax::mojom::Role::kInlineTextBox) {
          bounds = child->GetInlineTextRect(start_offset, end_offset);
        }
      }

      bounds.Offset(g_offset);
      return gfx::ToEnclosingRect(bounds);
    }
    case AXCoordinateSystem::kRootFrame:
    case AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

gfx::Rect TestAXNodeWrapper::GetHypertextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  switch (coordinate_system) {
    case AXCoordinateSystem::kScreen: {
      // Ignoring start, len, and clipped, as there's no clean way to map these
      // via unit tests.
      gfx::RectF bounds = GetLocation();
      bounds.Offset(g_offset);
      return gfx::ToEnclosingRect(bounds);
    }
    case AXCoordinateSystem::kRootFrame:
    case AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

TestAXNodeWrapper* TestAXNodeWrapper::HitTestSyncInternal(int x, int y) {
  // Here we find the deepest child whose bounding box contains the given point.
  // The assuptions are that there are no overlapping bounding rects and that
  // all children have smaller bounding rects than their parents.
  if (!GetClippedScreenBoundsRect().Contains(gfx::Rect(x, y, 0, 0)))
    return nullptr;

  for (int i = 0; i < GetChildCount(); i++) {
    TestAXNodeWrapper* child = GetOrCreate(tree_, node_->children()[i]);
    if (!child)
      return nullptr;

    TestAXNodeWrapper* result = child->HitTestSyncInternal(x, y);
    if (result) {
      return result;
    }
  }
  return this;
}

gfx::NativeViewAccessible TestAXNodeWrapper::HitTestSync(int x, int y) {
  TestAXNodeWrapper* wrapper = HitTestSyncInternal(x, y);
  return wrapper ? wrapper->ax_platform_node()->GetNativeViewAccessible()
                 : nullptr;
}

gfx::NativeViewAccessible TestAXNodeWrapper::GetFocus() {
  auto focused = g_focused_node_in_tree.find(tree_);
  if (focused != g_focused_node_in_tree.end() &&
      focused->second->IsDescendantOf(node_)) {
    return GetOrCreate(tree_, focused->second)
        ->ax_platform_node()
        ->GetNativeViewAccessible();
  }
  return nullptr;
}

bool TestAXNodeWrapper::IsMinimized() const {
  return minimized_;
}

// Walk the AXTree and ensure that all wrappers are created
void TestAXNodeWrapper::BuildAllWrappers(AXTree* tree, AXNode* node) {
  for (auto* child : node->children()) {
    TestAXNodeWrapper::GetOrCreate(tree, child);
    BuildAllWrappers(tree, child);
  }
}

void TestAXNodeWrapper::ResetNativeEventTarget() {
  native_event_target_ = gfx::kNullAcceleratedWidget;
}

AXPlatformNode* TestAXNodeWrapper::GetFromNodeID(int32_t id) {
  // Force creating all of the wrappers for this tree.
  BuildAllWrappers(tree_, node_);

  for (auto it = g_node_to_wrapper_map.begin();
       it != g_node_to_wrapper_map.end(); ++it) {
    AXNode* node = it->first;
    if (node->id() == id) {
      TestAXNodeWrapper* wrapper = it->second;
      return wrapper->ax_platform_node();
    }
  }
  return nullptr;
}

int TestAXNodeWrapper::GetIndexInParent() {
  return node_ ? int{node_->index_in_parent()} : -1;
}

void TestAXNodeWrapper::ReplaceIntAttribute(int32_t node_id,
                                            ax::mojom::IntAttribute attribute,
                                            int32_t value) {
  if (!tree_)
    return;

  AXNode* node = tree_->GetFromId(node_id);
  if (!node)
    return;

  AXNodeData new_data = node->data();
  std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>& attributes =
      new_data.int_attributes;

  base::EraseIf(attributes, [attribute](auto& pair) {
    return pair.first == attribute;
  });

  new_data.AddIntAttribute(attribute, value);
  node->SetData(new_data);
}

void TestAXNodeWrapper::ReplaceFloatAttribute(
    ax::mojom::FloatAttribute attribute,
    float value) {
  AXNodeData new_data = GetData();
  std::vector<std::pair<ax::mojom::FloatAttribute, float>>& attributes =
      new_data.float_attributes;

  base::EraseIf(attributes,
                [attribute](auto& pair) { return pair.first == attribute; });

  new_data.AddFloatAttribute(attribute, value);
  node_->SetData(new_data);
}

void TestAXNodeWrapper::ReplaceBoolAttribute(ax::mojom::BoolAttribute attribute,
                                             bool value) {
  AXNodeData new_data = GetData();
  std::vector<std::pair<ax::mojom::BoolAttribute, bool>>& attributes =
      new_data.bool_attributes;

  base::EraseIf(attributes,
                [attribute](auto& pair) { return pair.first == attribute; });

  new_data.AddBoolAttribute(attribute, value);
  node_->SetData(new_data);
}

void TestAXNodeWrapper::ReplaceStringAttribute(
    ax::mojom::StringAttribute attribute,
    std::string value) {
  AXNodeData new_data = GetData();
  std::vector<std::pair<ax::mojom::StringAttribute, std::string>>& attributes =
      new_data.string_attributes;

  base::EraseIf(attributes,
                [attribute](auto& pair) { return pair.first == attribute; });

  new_data.AddStringAttribute(attribute, value);
  node_->SetData(new_data);
}

void TestAXNodeWrapper::ReplaceTreeDataTextSelection(int32_t anchor_node_id,
                                                     int32_t anchor_offset,
                                                     int32_t focus_node_id,
                                                     int32_t focus_offset) {
  if (!tree_)
    return;

  AXTreeData new_tree_data = GetTreeData();
  new_tree_data.sel_anchor_object_id = anchor_node_id;
  new_tree_data.sel_anchor_offset = anchor_offset;
  new_tree_data.sel_focus_object_id = focus_node_id;
  new_tree_data.sel_focus_offset = focus_offset;

  tree_->UpdateData(new_tree_data);
}

bool TestAXNodeWrapper::IsTable() const {
  return node_->IsTable();
}

base::Optional<int> TestAXNodeWrapper::GetTableRowCount() const {
  return node_->GetTableRowCount();
}

base::Optional<int> TestAXNodeWrapper::GetTableColCount() const {
  return node_->GetTableColCount();
}

base::Optional<int> TestAXNodeWrapper::GetTableAriaRowCount() const {
  return node_->GetTableAriaRowCount();
}

base::Optional<int> TestAXNodeWrapper::GetTableAriaColCount() const {
  return node_->GetTableAriaColCount();
}

base::Optional<int> TestAXNodeWrapper::GetTableCellCount() const {
  return node_->GetTableCellCount();
}

std::vector<int32_t> TestAXNodeWrapper::GetColHeaderNodeIds() const {
  std::vector<int32_t> header_ids;
  node_->GetTableCellColHeaderNodeIds(&header_ids);
  return header_ids;
}

std::vector<int32_t> TestAXNodeWrapper::GetColHeaderNodeIds(
    int col_index) const {
  std::vector<int32_t> header_ids;
  node_->GetTableColHeaderNodeIds(col_index, &header_ids);
  return header_ids;
}

std::vector<int32_t> TestAXNodeWrapper::GetRowHeaderNodeIds() const {
  std::vector<int32_t> header_ids;
  node_->GetTableCellRowHeaderNodeIds(&header_ids);
  return header_ids;
}

std::vector<int32_t> TestAXNodeWrapper::GetRowHeaderNodeIds(
    int row_index) const {
  std::vector<int32_t> header_ids;
  node_->GetTableRowHeaderNodeIds(row_index, &header_ids);
  return header_ids;
}

bool TestAXNodeWrapper::IsTableRow() const {
  return node_->IsTableRow();
}

base::Optional<int> TestAXNodeWrapper::GetTableRowRowIndex() const {
  return node_->GetTableRowRowIndex();
}

bool TestAXNodeWrapper::IsTableCellOrHeader() const {
  return node_->IsTableCellOrHeader();
}

base::Optional<int> TestAXNodeWrapper::GetTableCellIndex() const {
  return node_->GetTableCellIndex();
}

base::Optional<int> TestAXNodeWrapper::GetTableCellColIndex() const {
  return node_->GetTableCellColIndex();
}

base::Optional<int> TestAXNodeWrapper::GetTableCellRowIndex() const {
  return node_->GetTableCellRowIndex();
}

base::Optional<int> TestAXNodeWrapper::GetTableCellColSpan() const {
  return node_->GetTableCellColSpan();
}

base::Optional<int> TestAXNodeWrapper::GetTableCellRowSpan() const {
  return node_->GetTableCellRowSpan();
}

base::Optional<int> TestAXNodeWrapper::GetTableCellAriaColIndex() const {
  return node_->GetTableCellAriaColIndex();
}

base::Optional<int> TestAXNodeWrapper::GetTableCellAriaRowIndex() const {
  return node_->GetTableCellAriaRowIndex();
}

base::Optional<int32_t> TestAXNodeWrapper::GetCellId(int row_index,
                                                     int col_index) const {
  AXNode* cell = node_->GetTableCellFromCoords(row_index, col_index);
  if (!cell)
    return base::nullopt;
  return cell->id();
}

gfx::AcceleratedWidget
TestAXNodeWrapper::GetTargetForNativeAccessibilityEvent() {
  return native_event_target_;
}

base::Optional<int32_t> TestAXNodeWrapper::CellIndexToId(int cell_index) const {
  AXNode* cell = node_->GetTableCellFromIndex(cell_index);
  if (!cell)
    return base::nullopt;
  return cell->id();
}

bool TestAXNodeWrapper::IsCellOrHeaderOfARIATable() const {
  return node_->IsCellOrHeaderOfARIATable();
}

bool TestAXNodeWrapper::IsCellOrHeaderOfARIAGrid() const {
  return node_->IsCellOrHeaderOfARIAGrid();
}

bool TestAXNodeWrapper::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  switch (data.action) {
    case ax::mojom::Action::kScrollToPoint:
      g_offset = gfx::Vector2d(data.target_point.x(), data.target_point.y());
      return true;

    case ax::mojom::Action::kScrollToMakeVisible: {
      auto offset = node_->data().relative_bounds.bounds.OffsetFromOrigin();
      g_offset = gfx::Vector2d(-offset.x(), -offset.y());
      return true;
    }

    case ax::mojom::Action::kDoDefault:
      if (GetData().role == ax::mojom::Role::kListBoxOption ||
          GetData().role == ax::mojom::Role::kCell) {
        bool current_value =
            GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
        ReplaceBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                             !current_value);
      }
      g_node_from_last_default_action = node_;
      return true;

    case ax::mojom::Action::kSetValue:
      if (IsRangeValueSupported(GetData())) {
        ReplaceFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                              std::stof(data.value));
      } else if (GetData().role == ax::mojom::Role::kTextField) {
        ReplaceStringAttribute(ax::mojom::StringAttribute::kValue, data.value);
      }
      return true;

    case ax::mojom::Action::kSetSelection: {
      ReplaceIntAttribute(data.anchor_node_id,
                          ax::mojom::IntAttribute::kTextSelStart,
                          data.anchor_offset);
      ReplaceIntAttribute(data.focus_node_id,
                          ax::mojom::IntAttribute::kTextSelEnd,
                          data.focus_offset);
      ReplaceTreeDataTextSelection(data.anchor_node_id, data.anchor_offset,
                                   data.focus_node_id, data.focus_offset);
      return true;
    }

    case ax::mojom::Action::kFocus:
      g_focused_node_in_tree[tree_] = node_;
      return true;

    case ax::mojom::Action::kShowContextMenu:
      g_node_from_last_show_context_menu = node_;
      return true;

    default:
      return true;
  }
}

base::string16 TestAXNodeWrapper::GetLocalizedRoleDescriptionForUnlabeledImage()
    const {
  return base::ASCIIToUTF16("Unlabeled image");
}

base::string16 TestAXNodeWrapper::GetLocalizedStringForLandmarkType() const {
  const AXNodeData& data = GetData();
  switch (data.role) {
    case ax::mojom::Role::kBanner:
      return base::ASCIIToUTF16("banner");

    case ax::mojom::Role::kComplementary:
      return base::ASCIIToUTF16("complementary");

    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return base::ASCIIToUTF16("content information");

    case ax::mojom::Role::kRegion:
      if (data.HasStringAttribute(ax::mojom::StringAttribute::kName))
        return base::ASCIIToUTF16("region");
      FALLTHROUGH;

    default:
      return {};
  }
}

base::string16 TestAXNodeWrapper::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      return base::ASCIIToUTF16(
          "To get missing image descriptions, open the context menu.");
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      return base::ASCIIToUTF16("Getting description...");
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      return base::ASCIIToUTF16(
          "Appears to contain adult content. No description available.");
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
      return base::ASCIIToUTF16("No description available.");
    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      return base::string16();
  }

  NOTREACHED();
  return base::string16();
}

base::string16 TestAXNodeWrapper::GetStyleNameAttributeAsLocalizedString()
    const {
  AXNode* current_node = node_;
  while (current_node) {
    if (current_node->data().role == ax::mojom::Role::kMark)
      return base::ASCIIToUTF16("mark");
    current_node = current_node->parent();
  }
  return base::string16();
}

bool TestAXNodeWrapper::ShouldIgnoreHoveredStateForTesting() {
  return true;
}

std::set<AXPlatformNode*> TestAXNodeWrapper::GetReverseRelations(
    ax::mojom::IntAttribute attr) {
  DCHECK(IsNodeIdIntAttribute(attr));
  return GetNodesForNodeIds(tree_->GetReverseRelations(attr, GetData().id));
}

std::set<AXPlatformNode*> TestAXNodeWrapper::GetReverseRelations(
    ax::mojom::IntListAttribute attr) {
  DCHECK(IsNodeIdIntListAttribute(attr));
  return GetNodesForNodeIds(tree_->GetReverseRelations(attr, GetData().id));
}

const ui::AXUniqueId& TestAXNodeWrapper::GetUniqueId() const {
  return unique_id_;
}

TestAXNodeWrapper::TestAXNodeWrapper(AXTree* tree, AXNode* node)
    : tree_(tree),
      node_(node),
      platform_node_(AXPlatformNode::Create(this)) {
#if defined(OS_WIN)
  native_event_target_ = gfx::kMockAcceleratedWidget;
#else
  native_event_target_ = gfx::kNullAcceleratedWidget;
#endif
}

bool TestAXNodeWrapper::IsOrderedSetItem() const {
  return node_->IsOrderedSetItem();
}

bool TestAXNodeWrapper::IsOrderedSet() const {
  return node_->IsOrderedSet();
}

base::Optional<int> TestAXNodeWrapper::GetPosInSet() const {
  return node_->GetPosInSet();
}

base::Optional<int> TestAXNodeWrapper::GetSetSize() const {
  return node_->GetSetSize();
}

gfx::RectF TestAXNodeWrapper::GetLocation() const {
  return GetData().relative_bounds.bounds;
}

int TestAXNodeWrapper::InternalChildCount() const {
  return int{node_->children().size()};
}

TestAXNodeWrapper* TestAXNodeWrapper::InternalGetChild(int index) const {
  CHECK_GE(index, 0);
  CHECK_LT(index, InternalChildCount());
  return GetOrCreate(tree_, node_->children()[size_t{index}]);
}

// Recursive helper function for GetDescendants. Aggregates all of the
// descendants for a given node within the descendants vector.
void TestAXNodeWrapper::Descendants(
    const AXNode* node,
    std::vector<gfx::NativeViewAccessible>* descendants) const {
  std::vector<AXNode*> child_nodes = node->children();
  for (AXNode* child : child_nodes) {
    descendants->emplace_back(ax_platform_node()
                                  ->GetDelegate()
                                  ->GetFromNodeID(child->id())
                                  ->GetNativeViewAccessible());
    Descendants(child, descendants);
  }
}

const std::vector<gfx::NativeViewAccessible> TestAXNodeWrapper::GetDescendants()
    const {
  std::vector<gfx::NativeViewAccessible> descendants;
  Descendants(node_, &descendants);
  return descendants;
}

gfx::RectF TestAXNodeWrapper::GetInlineTextRect(const int start_offset,
                                                const int end_offset) const {
  DCHECK(start_offset >= 0 && end_offset >= 0 && start_offset <= end_offset);
  const std::vector<int32_t>& character_offsets = GetData().GetIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets);
  gfx::RectF location = GetLocation();
  gfx::RectF bounds;

  switch (static_cast<ax::mojom::TextDirection>(
      GetData().GetIntAttribute(ax::mojom::IntAttribute::kTextDirection))) {
    // Currently only kNone and kLtr are supported text direction.
    case ax::mojom::TextDirection::kNone:
    case ax::mojom::TextDirection::kLtr: {
      int start_pixel_offset =
          start_offset > 0 ? character_offsets[start_offset - 1] : location.x();
      int end_pixel_offset =
          end_offset > 0 ? character_offsets[end_offset - 1] : location.x();
      bounds =
          gfx::RectF(start_pixel_offset, location.y(),
                     end_pixel_offset - start_pixel_offset, location.height());
      break;
    }
    default:
      NOTIMPLEMENTED();
  }
  return bounds;
}

}  // namespace ui
