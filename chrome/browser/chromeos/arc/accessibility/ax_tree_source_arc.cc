// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

#include <stack>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/chromeos/arc/accessibility/geometry_util.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "components/exo/wm_helper.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {

using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXEventData = mojom::AccessibilityEventData;
using AXEventType = mojom::AccessibilityEventType;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXNodeInfoDataPtr = mojom::AccessibilityNodeInfoDataPtr;
using AXStringProperty = mojom::AccessibilityStringProperty;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;
using AXWindowIntListProperty = mojom::AccessibilityWindowIntListProperty;

namespace {
bool IsDrawerLayout(AXNodeInfoData* node) {
  if (!node || !node->string_properties)
    return false;

  auto it = node->string_properties->find(AXStringProperty::CLASS_NAME);
  if (it == node->string_properties->end())
    return false;

  return it->second == "androidx.drawerlayout.widget.DrawerLayout" ||
         it->second == "android.support.v4.widget.DrawerLayout";
}
}  // namespace

AXTreeSourceArc::AXTreeSourceArc(Delegate* delegate)
    : current_tree_serializer_(new AXTreeArcSerializer(this)),
      is_notification_(false),
      is_input_method_window_(false),
      delegate_(delegate) {}

AXTreeSourceArc::~AXTreeSourceArc() {
  Reset();
}

void AXTreeSourceArc::NotifyAccessibilityEvent(AXEventData* event_data) {
  tree_map_.clear();
  parent_map_.clear();
  cached_computed_bounds_.clear();
  root_id_.reset();

  window_id_ = event_data->window_id;
  is_notification_ = event_data->notification_key.has_value();
  is_input_method_window_ = event_data->is_input_method_window;

  // The following loops perform caching to prepare for AXTreeSerializer.
  // First, we cache the windows.
  // Next, we cache the nodes. To compute IsClickableLeaf, we want a mapping
  // from node id to an index in event_data->node_data is needed to avoid O(N^2)
  // computation, iterate it twice.
  // Finally, we cache each node's computed bounds, based on its descendants.
  CHECK(event_data->window_data);
  root_id_ = event_data->window_data->at(0)->window_id;
  for (size_t i = 0; i < event_data->window_data->size(); ++i) {
    int32_t window_id = event_data->window_data->at(i)->window_id;
    int32_t root_node_id = event_data->window_data->at(i)->root_node_id;
    AXWindowInfoData* window = event_data->window_data->at(i).get();
    if (root_node_id)
      parent_map_[root_node_id] = window_id;

    tree_map_[window_id] =
        std::make_unique<AccessibilityWindowInfoDataWrapper>(this, window);

    std::vector<int32_t> children;
    if (GetProperty(window->int_list_properties,
                    AXWindowIntListProperty::CHILD_WINDOW_IDS, &children)) {
      for (const int32_t child : children) {
        DCHECK(child != root_id_);
        parent_map_[child] = window_id;
      }
    }
  }

  // Mapping from node id to index in event_data->node_data.
  std::map<int32_t, int32_t> node_data_index_map;
  for (size_t i = 0; i < event_data->node_data.size(); ++i) {
    int32_t node_id = event_data->node_data[i]->id;
    node_data_index_map[node_id] = i;
    std::vector<int32_t> children;
    if (GetProperty(event_data->node_data[i].get()->int_list_properties,
                    AXIntListProperty::CHILD_NODE_IDS, &children)) {
      for (const int32_t child : children)
        parent_map_[child] = node_id;
    }
  }
  std::map<int32_t, bool> important_map;
  BuildImportantMap(event_data->node_data, node_data_index_map, important_map);
  for (int i = event_data->node_data.size() - 1; i >= 0; --i) {
    int32_t id = event_data->node_data[i]->id;
    AXNodeInfoData* node = event_data->node_data[i].get();
    bool is_clickable_leaf =
        ComputeIsClickableLeaf(event_data->node_data, i, node_data_index_map);
    tree_map_[id] = std::make_unique<AccessibilityNodeInfoDataWrapper>(
        this, node, is_clickable_leaf, important_map[i]);
  }

  // Assuming |nodeData| is in pre-order, compute cached bounds in post-order to
  // avoid an O(n^2) amount of work as the computed bounds uses descendant
  // bounds.
  for (int i = event_data->node_data.size() - 1; i >= 0; --i) {
    int32_t id = event_data->node_data[i]->id;
    cached_computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }
  for (int i = event_data->window_data->size() - 1; i >= 0; --i) {
    int32_t id = event_data->window_data->at(i)->window_id;
    cached_computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }

  // Calculate the focused ID.
  if (event_data->event_type == AXEventType::VIEW_FOCUSED) {
    AccessibilityInfoDataWrapper* focused_node =
        GetFromId(event_data->source_id);
    if (focused_node) {
      // Sometimes Android sets focus on unfocusable node, e.g. ListView.
      AccessibilityInfoDataWrapper* adjusted_node =
          FindFirstFocusableNode(focused_node);
      android_focused_id_ = IsValid(adjusted_node) ? adjusted_node->GetId()
                                                   : event_data->source_id;
    }
  } else if (event_data->event_type == AXEventType::WINDOW_STATE_CHANGED) {
    // When accessibility window changed, a11y event of WINDOW_CONTENT_CHANGED
    // is fired from Android multiple times.
    // The event of WINDOW_STATE_CHANGED is fired only once for each window
    // change and use it as a trigger to move the a11y focus to the first node.
    AccessibilityInfoDataWrapper* focused_node =
        GetFromId(event_data->source_id);
    AccessibilityInfoDataWrapper* new_focus =
        FindFirstFocusableNode(focused_node);
    if (IsValid(new_focus))
      android_focused_id_ = new_focus->GetId();

    if (event_data->eventText)
      UpdateAXNameCache(focused_node, *event_data->eventText);
  }
  if (!android_focused_id_.has_value()) {
    AccessibilityInfoDataWrapper* root = GetRoot();
    // TODO (sarakato): Add proper fix once cause of invalid node is known.
    if (!IsValid(root)) {
      return;
    } else if (root->IsNode()) {
      android_focused_id_ = root_id_;
    } else {
      std::vector<AccessibilityInfoDataWrapper*> children;
      root->GetChildren(&children);
      if (!children.empty()) {
        for (size_t i = 0; i < children.size(); ++i) {
          if (children[i]->IsNode()) {
            android_focused_id_ = children[i]->GetId();
            break;
          }
        }
      }
    }
  }

  ApplyCachedProperties();

  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = ax_tree_id();

  event_bundle.events.emplace_back();
  ui::AXEvent& event = event_bundle.events.back();

  // When the focused node exists, give it as a hint to decide a Chrome
  // automation event type.
  AXNodeInfoData* focused_node = nullptr;
  if (android_focused_id_.has_value() &&
      tree_map_.find(*android_focused_id_) != tree_map_.end())
    focused_node = tree_map_[*android_focused_id_]->GetNode();
  event.event_type = ToAXEvent(event_data->event_type, focused_node);
  event.id = event_data->source_id;

  if ((event_data->event_type == AXEventType::WINDOW_CONTENT_CHANGED ||
       event_data->event_type == AXEventType::VIEW_SCROLLED) &&
      chrome_focused_id_ && chrome_focused_bounds_) {
    AccessibilityInfoDataWrapper* chrome_focused_node =
        GetFromId(*chrome_focused_id_);
    if (chrome_focused_node &&
        chrome_focused_bounds_ != chrome_focused_node->GetBounds()) {
      // Notify ChromeVox that the accessibility focus location changed.
      event_bundle.events.emplace_back();
      ui::AXEvent& additional_event = event_bundle.events.back();
      additional_event.event_type = ax::mojom::Event::kLocationChanged;
      additional_event.id = *chrome_focused_id_;
      chrome_focused_bounds_ = chrome_focused_node->GetBounds();
    }
  }

  event_bundle.updates.emplace_back();

  // Force the tree, starting at the target of the event, to update, so
  // unignored fields get updated.
  event_bundle.updates[0].node_id_to_clear = event_data->source_id;
  current_tree_serializer_->InvalidateSubtree(GetFromId(event_data->source_id));

  current_tree_serializer_->SerializeChanges(GetFromId(event_data->source_id),
                                             &event_bundle.updates.back());

  GetAutomationEventRouter()->DispatchAccessibilityEvents(event_bundle);
}

void AXTreeSourceArc::NotifyActionResult(const ui::AXActionData& data,
                                         bool result) {
  GetAutomationEventRouter()->DispatchActionResult(data, result);
}

void AXTreeSourceArc::NotifyGetTextLocationDataResult(
    const ui::AXActionData& data,
    const base::Optional<gfx::Rect>& rect) {
  GetAutomationEventRouter()->DispatchGetTextLocationDataResult(data, rect);
}

void AXTreeSourceArc::UpdateAccessibilityFocusLocation(int32_t id) {
  AccessibilityInfoDataWrapper* node = GetFromId(id);
  if (!IsValid(node))
    return;
  chrome_focused_id_ = id;
  chrome_focused_bounds_ = node->GetBounds();
}

const gfx::Rect AXTreeSourceArc::GetBounds(
    AccessibilityInfoDataWrapper* info_data,
    aura::Window* active_window) const {
  DCHECK(root_id_.has_value());

  gfx::Rect info_data_bounds = info_data->GetBounds();

  if (!active_window) {
    gfx::Rect root_bounds = GetRoot()->GetBounds();
    info_data_bounds.Offset(-1 * root_bounds.x(), -1 * root_bounds.y());
    return info_data_bounds;
  }

  exo::WMHelper* wm_helper = exo::WMHelper::GetInstance();
  if (!wm_helper)
    return info_data_bounds;

  views::Widget* widget = views::Widget::GetWidgetForNativeView(active_window);
  DCHECK(widget);

  gfx::RectF info_data_bounds_f =
      arc::ToChromeBounds(info_data_bounds, wm_helper, widget);

  // TODO(katie): offset_container_id should work and we shouldn't have to
  // go into this code path for each node.
  DCHECK(widget->widget_delegate());
  DCHECK(widget->widget_delegate()->GetContentsView());
  const gfx::Rect root_bounds =
      widget->widget_delegate()->GetContentsView()->GetBoundsInScreen();

  // Bounds of root node is relative to its container, i.e. contents view
  // (ShellSurfaceBase).
  info_data_bounds_f.Offset(-1 * root_bounds.x(), -1 * root_bounds.y());

  arc::ScaleDeviceFactor(info_data_bounds_f,
                         active_window->GetToplevelWindow());
  return gfx::ToEnclosingRect(info_data_bounds_f);
}

void AXTreeSourceArc::InvalidateTree() {
  current_tree_serializer_->Reset();
}

bool AXTreeSourceArc::IsRootOfNodeTree(int32_t id) const {
  const auto& node_it = tree_map_.find(id);
  if (node_it == tree_map_.end())
    return false;

  if (!node_it->second->IsNode())
    return false;

  const auto& parent_it = parent_map_.find(id);
  if (parent_it == parent_map_.end())
    return true;

  const auto& parent_tree_it = tree_map_.find(parent_it->second);
  CHECK(parent_tree_it != tree_map_.end());
  return !parent_tree_it->second->IsNode();
}

bool AXTreeSourceArc::GetTreeData(ui::AXTreeData* data) const {
  data->tree_id = ax_tree_id();
  if (android_focused_id_.has_value())
    data->focus_id = *android_focused_id_;
  return true;
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetRoot() const {
  return root_id_.has_value() ? GetFromId(*root_id_) : nullptr;
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetFromId(int32_t id) const {
  auto it = tree_map_.find(id);
  if (it == tree_map_.end())
    return nullptr;
  return it->second.get();
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetParent(
    AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data)
    return nullptr;
  auto it = parent_map_.find(info_data->GetId());
  if (it != parent_map_.end())
    return GetFromId(it->second);
  return nullptr;
}

void AXTreeSourceArc::SerializeNode(AccessibilityInfoDataWrapper* info_data,
                                    ui::AXNodeData* out_data) const {
  if (!info_data)
    return;

  info_data->Serialize(out_data);
}

extensions::AutomationEventRouterInterface*
AXTreeSourceArc::GetAutomationEventRouter() const {
  return extensions::AutomationEventRouter::GetInstance();
}

gfx::Rect AXTreeSourceArc::ComputeEnclosingBounds(
    AccessibilityInfoDataWrapper* info_data) const {
  DCHECK(info_data);
  gfx::Rect computed_bounds;
  // Exit early if the node or window is invisible.
  if (!info_data->IsVisibleToUser())
    return computed_bounds;

  ComputeEnclosingBoundsInternal(info_data, computed_bounds);
  return computed_bounds;
}

void AXTreeSourceArc::ComputeEnclosingBoundsInternal(
    AccessibilityInfoDataWrapper* info_data,
    gfx::Rect& computed_bounds) const {
  auto cached_bounds = cached_computed_bounds_.find(info_data->GetId());
  if (cached_bounds != cached_computed_bounds_.end()) {
    computed_bounds.Union(cached_bounds->second);
    return;
  }

  if (!info_data->IsVisibleToUser())
    return;
  if (info_data->CanBeAccessibilityFocused()) {
    // Only consider nodes that can possibly be accessibility focused.
    computed_bounds.Union(info_data->GetBounds());
    return;
  }
  std::vector<AccessibilityInfoDataWrapper*> children;
  info_data->GetChildren(&children);
  if (children.empty())
    return;
  for (AccessibilityInfoDataWrapper* child : children)
    ComputeEnclosingBoundsInternal(child, computed_bounds);
  return;
}

bool AXTreeSourceArc::ComputeIsClickableLeaf(
    const std::vector<AXNodeInfoDataPtr>& nodes,
    int32_t node_index,
    const std::map<int32_t, int32_t>& node_id_to_nodes_index) const {
  AXNodeInfoData* node = nodes[node_index].get();
  if (!GetBooleanProperty(node, AXBooleanProperty::CLICKABLE))
    return false;

  std::vector<int32_t> children;
  if (!GetProperty(node->int_list_properties, AXIntListProperty::CHILD_NODE_IDS,
                   &children))
    return true;

  std::stack<int32_t, std::vector<int32_t>> stack(children);
  while (!stack.empty()) {
    int32_t parent_id = stack.top();
    stack.pop();
    AXNodeInfoData* parent_node =
        nodes[node_id_to_nodes_index.at(parent_id)].get();
    if (GetBooleanProperty(parent_node, AXBooleanProperty::CLICKABLE))
      return false;
    if (GetProperty(parent_node->int_list_properties,
                    AXIntListProperty::CHILD_NODE_IDS, &children)) {
      for (const int32_t child : children)
        stack.push(child);
    }
  }
  return true;
}

void AXTreeSourceArc::BuildImportantMap(
    const std::vector<AXNodeInfoDataPtr>& nodes,
    const std::map<int32_t, int32_t>& node_id_to_nodes_index,
    std::map<int32_t, bool>& out_map) const {
  // nodes can be empty only in tests.
  if (nodes.size() > 0)
    BuildImportantMapInternal(0, nodes, node_id_to_nodes_index, out_map);
}

bool AXTreeSourceArc::BuildImportantMapInternal(
    int32_t nodes_index,
    const std::vector<AXNodeInfoDataPtr>& nodes,
    const std::map<int32_t, int32_t>& node_id_to_nodes_index,
    std::map<int32_t, bool>& is_important_cache) const {
  AXNodeInfoData* node = nodes[nodes_index].get();

  bool is_important = IsImportantInAndroid(node) &&
                      (HasImportantProperty(node) ||
                       cached_names_.find(node->id) != cached_names_.end());

  // If any child node has importance, this node is also important.
  std::vector<int32_t> children;
  if (GetProperty(node->int_list_properties, AXIntListProperty::CHILD_NODE_IDS,
                  &children)) {
    for (const int32_t child : children) {
      is_important |=
          BuildImportantMapInternal(node_id_to_nodes_index.at(child), nodes,
                                    node_id_to_nodes_index, is_important_cache);
    }
  }

  is_important_cache[nodes_index] = is_important;
  return is_important;
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::FindFirstFocusableNode(
    AccessibilityInfoDataWrapper* info_data) const {
  if (!IsValid(info_data))
    return nullptr;

  if (info_data->IsVisibleToUser() && info_data->CanBeAccessibilityFocused())
    return info_data;

  std::vector<AccessibilityInfoDataWrapper*> children;
  GetChildren(info_data, &children);
  for (AccessibilityInfoDataWrapper* child : children) {
    AccessibilityInfoDataWrapper* candidate = FindFirstFocusableNode(child);
    if (candidate)
      return candidate;
  }

  return nullptr;
}

void AXTreeSourceArc::UpdateAXNameCache(
    AccessibilityInfoDataWrapper* focused_node,
    const std::vector<std::string>& event_text) {
  if (IsDrawerLayout(focused_node->GetNode())) {
    // When drawer menu opened, make the menu title announced.
    // When focus is changed, ChromeVox computes the diff in ancestry between
    // the previously focused and new focused node.
    // As the DrawerLayout is LCA of them, set the new title to be the first
    // visible child node (which is usually drawer menu).
    std::vector<AccessibilityInfoDataWrapper*> children;
    focused_node->GetChildren(&children);
    for (auto* child : children) {
      if (child->IsNode() && child->IsVisibleToUser() &&
          GetBooleanProperty(child->GetNode(), AXBooleanProperty::IMPORTANCE)) {
        cached_roles_[child->GetId()] = ax::mojom::Role::kMenu;
        if (!event_text.empty())
          cached_names_[child->GetId()] = base::JoinString(event_text, " ");
        return;
      }
    }
  }
}

void AXTreeSourceArc::ApplyCachedProperties() {
  for (auto it = cached_names_.begin(); it != cached_names_.end();) {
    AccessibilityInfoDataWrapper* node = GetFromId(it->first);
    if (node) {
      static_cast<AccessibilityNodeInfoDataWrapper*>(node)->set_cached_name(
          it->second);
      it++;
    } else {
      it = cached_names_.erase(it);
    }
  }

  for (auto it = cached_roles_.begin(); it != cached_roles_.end();) {
    AccessibilityInfoDataWrapper* node = GetFromId(it->first);
    if (node) {
      static_cast<AccessibilityNodeInfoDataWrapper*>(node)->set_role(
          it->second);
      it++;
    } else {
      it = cached_roles_.erase(it);
    }
  }
}

void AXTreeSourceArc::Reset() {
  tree_map_.clear();
  parent_map_.clear();
  cached_computed_bounds_.clear();
  current_tree_serializer_.reset(new AXTreeArcSerializer(this));
  root_id_.reset();
  window_id_.reset();
  android_focused_id_.reset();
  extensions::AutomationEventRouterInterface* router =
      GetAutomationEventRouter();
  if (!router)
    return;

  router->DispatchTreeDestroyedEvent(ax_tree_id(), nullptr);
}

int32_t AXTreeSourceArc::GetId(AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data)
    return ui::AXNode::kInvalidAXID;
  return info_data->GetId();
}

void AXTreeSourceArc::GetChildren(
    AccessibilityInfoDataWrapper* info_data,
    std::vector<AccessibilityInfoDataWrapper*>* out_children) const {
  if (!info_data)
    return;

  info_data->GetChildren(out_children);
  if (out_children->empty())
    return;

  std::map<int32_t, size_t> id_to_index;
  for (size_t i = 0; i < out_children->size(); i++)
    id_to_index[out_children->at(i)->GetId()] = i;

  // Sort children based on their enclosing bounding rectangles, based on their
  // descendants.
  std::sort(
      out_children->begin(), out_children->end(),
      [this, &id_to_index](auto left, auto right) {
        auto left_bounds = ComputeEnclosingBounds(left);
        auto right_bounds = ComputeEnclosingBounds(right);

        if (left_bounds.IsEmpty() || right_bounds.IsEmpty()) {
          return id_to_index.at(left->GetId()) < id_to_index.at(right->GetId());
        }

        // Top to bottom sort (non-overlapping).
        if (!left_bounds.Intersects(right_bounds))
          return left_bounds.y() < right_bounds.y();

        // Overlapping
        // Left to right.
        int left_difference = left_bounds.x() - right_bounds.x();
        if (left_difference != 0)
          return left_difference < 0;

        // Top to bottom.
        int top_difference = left_bounds.y() - right_bounds.y();
        if (top_difference != 0)
          return top_difference < 0;

        // Larger to smaller.
        int height_difference = left_bounds.height() - right_bounds.height();
        if (height_difference != 0)
          return height_difference > 0;

        int width_difference = left_bounds.width() - right_bounds.width();
        if (width_difference != 0)
          return width_difference > 0;

        // The rects are equal.
        return id_to_index.at(left->GetId()) < id_to_index.at(right->GetId());
      });
}

bool AXTreeSourceArc::IsIgnored(AccessibilityInfoDataWrapper* info_data) const {
  return false;
}

bool AXTreeSourceArc::IsValid(AccessibilityInfoDataWrapper* info_data) const {
  return info_data;
}

bool AXTreeSourceArc::IsEqual(AccessibilityInfoDataWrapper* info_data1,
                              AccessibilityInfoDataWrapper* info_data2) const {
  if (!info_data1 || !info_data2)
    return false;
  return info_data1->GetId() == info_data2->GetId();
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetNull() const {
  return nullptr;
}

void AXTreeSourceArc::PerformAction(const ui::AXActionData& data) {
  delegate_->OnAction(data);
}

}  // namespace arc
