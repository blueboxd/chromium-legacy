// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_auralinux.h"

#include <atk/atk.h>
#include <vector>

#include "content/browser/accessibility/browser_accessibility_auralinux.h"
#include "content/common/accessibility_messages.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerAuraLinux(initial_tree, delegate,
                                                  factory);
}

BrowserAccessibilityManagerAuraLinux*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerAuraLinux() {
  return static_cast<BrowserAccessibilityManagerAuraLinux*>(this);
}

BrowserAccessibilityManagerAuraLinux::BrowserAccessibilityManagerAuraLinux(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(delegate, factory) {
  Initialize(initial_tree);
}

BrowserAccessibilityManagerAuraLinux::~BrowserAccessibilityManagerAuraLinux() {}

// static
ui::AXTreeUpdate BrowserAccessibilityManagerAuraLinux::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerAuraLinux::FireFocusEvent(
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireFocusEvent(node);
  FireEvent(node, ax::mojom::Event::kFocus);
}

void BrowserAccessibilityManagerAuraLinux::FireSelectedEvent(
    BrowserAccessibility* node) {
  // Some browser UI widgets, such as the omnibox popup, only send notifications
  // when they become selected. In contrast elements in a page, such as options
  // in the select element, also send notifications when they become unselected.
  // Since AXPlatformNodeAuraLinux must handle firing a platform event for the
  // unselected case, we can safely ignore the unselected case for rendered
  // elements.
  if (!node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    return;

  FireEvent(node, ax::mojom::Event::kSelection);
}

void BrowserAccessibilityManagerAuraLinux::FireLoadingEvent(
    BrowserAccessibility* node,
    bool is_loading) {
  if (!node->IsNative())
    return;

  gfx::NativeViewAccessible obj = node->GetNativeViewAccessible();
  if (!ATK_IS_OBJECT(obj))
    return;

  atk_object_notify_state_change(obj, ATK_STATE_BUSY, is_loading);
  if (!is_loading)
    g_signal_emit_by_name(obj, "load_complete");
}

void BrowserAccessibilityManagerAuraLinux::FireExpandedEvent(
    BrowserAccessibility* node,
    bool is_expanded) {
  if (!node->IsNative())
    return;

  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnExpandedStateChanged(
      is_expanded);
}

void BrowserAccessibilityManagerAuraLinux::FireEvent(BrowserAccessibility* node,
                                                     ax::mojom::Event event) {
  if (!node->IsNative())
    return;

  ToBrowserAccessibilityAuraLinux(node)->GetNode()->NotifyAccessibilityEvent(
      event);
}

void BrowserAccessibilityManagerAuraLinux::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node);
  // Need to implement.
}

void BrowserAccessibilityManagerAuraLinux::FireNameChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnNameChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireDescriptionChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnDescriptionChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireSubtreeCreatedEvent(
    BrowserAccessibility* node) {
  // Sending events during a load would create a lot of spam, don't do that.
  if (GetTreeData().loaded)
    ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnSubtreeCreated();
}

void BrowserAccessibilityManagerAuraLinux::FireGeneratedEvent(
    ui::AXEventGenerator::Event event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);

  switch (event_type) {
    case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED: {
      int32_t focus_id = GetTreeData().sel_focus_object_id;
      BrowserAccessibility* focus_object = GetFromID(focus_id);
      if (focus_object)
        FireEvent(focus_object, ax::mojom::Event::kTextSelectionChanged);
      break;
    }
    case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      FireEvent(node, ax::mojom::Event::kActiveDescendantChanged);
      break;
    case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      FireEvent(node, ax::mojom::Event::kCheckedStateChanged);
      break;
    case ui::AXEventGenerator::Event::COLLAPSED:
      FireExpandedEvent(node, false);
      break;
    case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
      FireEvent(node, ax::mojom::Event::kDocumentTitleChanged);
      break;
    case ui::AXEventGenerator::Event::EXPANDED:
      FireExpandedEvent(node, true);
      break;
    case ui::AXEventGenerator::Event::LOAD_COMPLETE:
      FireLoadingEvent(node, false);
      FireEvent(node, ax::mojom::Event::kLoadComplete);
      break;
    case ui::AXEventGenerator::Event::LOAD_START:
      FireLoadingEvent(node, true);
      break;
    case ui::AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      FireEvent(node, ax::mojom::Event::kSelectedChildrenChanged);
      break;
    case ui::AXEventGenerator::Event::MENU_ITEM_SELECTED:
    case ui::AXEventGenerator::Event::SELECTED_CHANGED:
      FireSelectedEvent(node);
      break;
    case ui::AXEventGenerator::Event::SUBTREE_CREATED:
      FireSubtreeCreatedEvent(node);
      break;
    case ui::AXEventGenerator::Event::VALUE_CHANGED:
      FireEvent(node, ax::mojom::Event::kValueChanged);
      break;
    case ui::AXEventGenerator::Event::NAME_CHANGED:
      FireNameChangedEvent(node);
      break;
    case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
      FireDescriptionChangedEvent(node);
      break;
    case ui::AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      FireEvent(node, ax::mojom::Event::kInvalidStatusChanged);
      break;
    default:
      // Need to implement.
      break;
  }
}

static AtkObject* GetParentFrameIfToplevelDocument(AtkObject* object) {
  while (object) {
    if (atk_object_get_role(object) == ATK_ROLE_DOCUMENT_WEB)
      return nullptr;
    if (atk_object_get_role(object) == ATK_ROLE_FRAME)
      return object;
    object = atk_object_get_parent(object);
  }
  return nullptr;
}

static void EstablishEmbeddedRelationship(AtkObject* document_object) {
  if (!document_object)
    return;

  AtkObject* window =
      GetParentFrameIfToplevelDocument(atk_object_get_parent(document_object));
  if (!window)
    return;

  ui::AXPlatformNodeAuraLinux* window_platform_node =
      static_cast<ui::AXPlatformNodeAuraLinux*>(
          ui::AXPlatformNode::FromNativeViewAccessible(window));
  ui::AXPlatformNodeAuraLinux* document_platform_node =
      static_cast<ui::AXPlatformNodeAuraLinux*>(
          ui::AXPlatformNode::FromNativeViewAccessible(document_object));
  if (!window_platform_node || !document_platform_node)
    return;

  window_platform_node->SetEmbeddedDocument(document_object);
  document_platform_node->SetEmbeddingWindow(window);
}

void BrowserAccessibilityManagerAuraLinux::OnSubtreeWillBeDeleted(
    ui::AXTree* tree,
    ui::AXNode* node) {
  BrowserAccessibilityManager::OnSubtreeWillBeDeleted(tree, node);
  // Sending events on load/destruction would create a lot of spam, avoid that.
  if (!GetTreeData().loaded)
    return;

  BrowserAccessibility* obj = GetFromAXNode(node);
  if (obj && obj->IsNative())
    ToBrowserAccessibilityAuraLinux(obj)->GetNode()->OnSubtreeWillBeDeleted();
}

void BrowserAccessibilityManagerAuraLinux::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);

  // Ideally we would like to do this only when `root_changed` is true, but it
  // seems that our parent ATK frame can switch between multiple
  // BrowserAccessibilityManagers with no way to detect that here. Instead
  // whenever an update happens we reestablish the relationship to our parent
  // frame.
  if (GetRoot() && GetRoot()->IsNative() && IsRootTree())
    EstablishEmbeddedRelationship(GetRoot()->GetNativeViewAccessible());

  // This is the second step in what will be a three step process mirroring that
  // used in BrowserAccessibilityManagerWin.
  for (const auto& change : changes) {
    const ui::AXNode* changed_node = change.node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->IsNative())
      ToBrowserAccessibilityAuraLinux(obj)->GetNode()->UpdateHypertext();
  }
}

}  // namespace content
