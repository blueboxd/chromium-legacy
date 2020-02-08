// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_
#define FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <fuchsia/math/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/engine/web_engine_export.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_observer.h"

namespace content {
class WebContents;
}  // namespace content

// This class is the intermediate for accessibility between Chrome and Fuchsia.
// It handles registration to the Fuchsia Semantics Manager, translating events
// and data structures between the two services, and forwarding actions and
// events.
// The lifetime of an instance of AccessibilityBridge is the same as that of a
// View created by FrameImpl. This class refers to the View via the
// caller-supplied ViewRef.
class WEB_ENGINE_EXPORT AccessibilityBridge
    : public content::WebContentsObserver,
      public fuchsia::accessibility::semantics::SemanticListener,
      public ui::AXTreeObserver {
 public:
  // |web_contents| is required to exist for the duration of |this|.
  AccessibilityBridge(
      fuchsia::accessibility::semantics::SemanticsManagerPtr semantics_manager,
      fuchsia::ui::views::ViewRef view_ref,
      content::WebContents* web_contents);
  ~AccessibilityBridge() final;

  void set_handle_actions_for_test(bool handle) {
    handle_actions_for_test_ = handle;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(AccessibilityBridgeTest, OnSemanticsModeChanged);

  // A struct used for caching semantic information. This allows for updates and
  // deletes to be stored in the same vector to preserve all ordering
  // information.
  struct SemanticUpdateOrDelete {
    enum Type { UPDATE, DELETE };

    SemanticUpdateOrDelete(SemanticUpdateOrDelete&& m);
    SemanticUpdateOrDelete(Type type,
                           fuchsia::accessibility::semantics::Node node,
                           uint32_t id_to_delete);
    ~SemanticUpdateOrDelete() = default;

    Type type;
    fuchsia::accessibility::semantics::Node update_node;
    uint32_t id_to_delete;
  };

  // Processes pending data and commits it to the Semantic Tree.
  void TryCommit();

  // Helper function for TryCommit() that sends the contents of |to_send_| to
  // the Semantic Tree, starting at |start|.
  void DispatchSemanticsMessages(size_t start, size_t size);

  // Callback for SemanticTree::CommitUpdates.
  void OnCommitComplete();

  // Converts AXNode ids to Semantic Node ids, and handles special casing of the
  // root.
  uint32_t ConvertToFuchsiaNodeId(int32_t ax_node_id);

  // Deletes all nodes in subtree rooted at and including |node|, unless |node|
  // is the root of the tree.
  // |tree| and |node| are owned by the accessibility bridge.
  void DeleteSubtree(ui::AXTree* tree, ui::AXNode* node);

  // content::WebContentsObserver implementation.
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& details) override;

  // fuchsia::accessibility::semantics::SemanticListener implementation.
  void OnAccessibilityActionRequested(
      uint32_t node_id,
      fuchsia::accessibility::semantics::Action action,
      OnAccessibilityActionRequestedCallback callback) final;
  void HitTest(fuchsia::math::PointF local_point,
               HitTestCallback callback) final;
  void OnSemanticsModeChanged(bool updates_enabled,
                              OnSemanticsModeChangedCallback callback) final;

  // ui::AXTreeObserver implementation.
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeObserver::Change>& changes) override;

  fuchsia::accessibility::semantics::SemanticTreePtr tree_ptr_;
  fidl::Binding<fuchsia::accessibility::semantics::SemanticListener> binding_;
  content::WebContents* web_contents_;
  ui::AXSerializableTree tree_;

  // Cache for pending data to be sent to the Semantic Tree between commits.
  std::vector<SemanticUpdateOrDelete> to_send_;
  bool commit_inflight_ = false;

  // Maintain a map of callbacks as multiple hit test events can happen at once.
  // These are keyed by the request_id field of ui::AXActionData.
  base::flat_map<int, HitTestCallback> pending_hit_test_callbacks_;

  // Maintain a map of callbacks for accessibility actions. Entries are keyed by
  // node id the action is performed on.
  base::flat_map<int, OnAccessibilityActionRequestedCallback>
      pending_accessibility_action_callbacks_;

  // The root id of |tree_|.
  int32_t root_id_ = 0;

  bool handle_actions_for_test_ = true;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityBridge);
};

#endif  // FUCHSIA_ENGINE_BROWSER_ACCESSIBILITY_BRIDGE_H_
