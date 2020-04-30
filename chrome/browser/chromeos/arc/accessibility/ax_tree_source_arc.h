// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/arc/accessibility/accessibility_info_data_wrapper.h"
#include "components/arc/mojom/accessibility_helper.mojom-forward.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/views/view.h"

namespace ui {
struct AXEvent;
}

namespace arc {
class AXTreeSourceArcTest;

using AXTreeArcSerializer = ui::AXTreeSerializer<AccessibilityInfoDataWrapper*,
                                                 ui::AXNodeData,
                                                 ui::AXTreeData>;

// This class represents the accessibility tree from the focused ARC window.
class AXTreeSourceArc : public ui::AXTreeSource<AccessibilityInfoDataWrapper*,
                                                ui::AXNodeData,
                                                ui::AXTreeData>,
                        public ui::AXActionHandler {
 public:
  class Delegate {
   public:
    virtual void OnAction(const ui::AXActionData& data) const = 0;
  };

  AXTreeSourceArc(Delegate* delegate, float device_scale_factor);
  ~AXTreeSourceArc() override;

  // Notify automation of an accessibility event.
  void NotifyAccessibilityEvent(mojom::AccessibilityEventData* event_data);

  // Notify automation of a result to an action.
  void NotifyActionResult(const ui::AXActionData& data, bool result);

  // Notify automation of result to getTextLocation.
  void NotifyGetTextLocationDataResult(const ui::AXActionData& data,
                                       const base::Optional<gfx::Rect>& rect);

  // Invalidates the tree serializer.
  void InvalidateTree();

  // Returns true if the node id is the root of the node tree (which can have a
  // parent window).
  bool IsRootOfNodeTree(int32_t id) const;

  // AXTreeSource:
  bool GetTreeData(ui::AXTreeData* data) const override;
  AccessibilityInfoDataWrapper* GetRoot() const override;
  AccessibilityInfoDataWrapper* GetFromId(int32_t id) const override;
  AccessibilityInfoDataWrapper* GetParent(
      AccessibilityInfoDataWrapper* info_data) const override;
  void SerializeNode(AccessibilityInfoDataWrapper* info_data,
                     ui::AXNodeData* out_data) const override;

  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float dsf) { device_scale_factor_ = dsf; }

  bool is_notification() { return is_notification_; }

  bool is_input_method_window() { return is_input_method_window_; }

  // The window id of this tree.
  base::Optional<int32_t> window_id() const { return window_id_; }

 private:
  friend class arc::AXTreeSourceArcTest;

  // virtual for testing.
  virtual extensions::AutomationEventRouterInterface* GetAutomationEventRouter()
      const;

  // Computes the smallest rect that encloses all of the descendants of
  // |info_data|.
  gfx::Rect ComputeEnclosingBounds(
      AccessibilityInfoDataWrapper* info_data) const;

  // Helper to recursively compute bounds for |info_data|. Returns true if
  // non-empty bounds were encountered.
  void ComputeEnclosingBoundsInternal(AccessibilityInfoDataWrapper* info_data,
                                      gfx::Rect& computed_bounds) const;

  // Computes if the node is clickable and has no clickable descendants.
  bool ComputeIsClickableLeaf(
      const std::vector<mojom::AccessibilityNodeInfoDataPtr>& nodes,
      int32_t node_index,
      const std::map<int32_t, int32_t>& node_id_to_nodes_index) const;

  // Builds a mapping from index in |nodes| to whether ignored state should be
  // applied to the node in chrome accessibility.
  void BuildImportanceTable(
      mojom::AccessibilityEventData* event_data,
      const std::map<int32_t, int32_t>& node_id_to_nodes_index,
      std::vector<bool>& out_node) const;

  bool BuildHasImportantProperty(
      int32_t nodes_index,
      const std::vector<mojom::AccessibilityNodeInfoDataPtr>& nodes,
      const std::map<int32_t, int32_t>& node_id_to_nodes_index,
      std::vector<bool>& has_important_prop_cache) const;

  // Find the most top-left focusable node under the given node.
  AccessibilityInfoDataWrapper* FindFirstFocusableNode(
      AccessibilityInfoDataWrapper* info_data) const;

  AccessibilityInfoDataWrapper* GetSelectedNodeInfoFromAdapterView(
      mojom::AccessibilityEventData* event_data) const;

  void UpdateAXNameCache(AccessibilityInfoDataWrapper* focused_node,
                         const std::vector<std::string>& event_text);

  void ApplyCachedProperties();

  // Compare previous live region and current live region, and add event to the
  // given vector if there is any difference.
  void HandleLiveRegions(std::vector<ui::AXEvent>* events);

  // Resets tree state.
  void Reset();

  // AXTreeSource:
  int32_t GetId(AccessibilityInfoDataWrapper* info_data) const override;
  void GetChildren(
      AccessibilityInfoDataWrapper* info_data,
      std::vector<AccessibilityInfoDataWrapper*>* out_children) const override;
  bool IsIgnored(AccessibilityInfoDataWrapper* info_data) const override;
  bool IsValid(AccessibilityInfoDataWrapper* info_data) const override;
  bool IsEqual(AccessibilityInfoDataWrapper* info_data1,
               AccessibilityInfoDataWrapper* info_data2) const override;
  AccessibilityInfoDataWrapper* GetNull() const override;

  // AXActionHandler:
  void PerformAction(const ui::AXActionData& data) override;

  // Maps an AccessibilityInfoDataWrapper ID to its tree data.
  std::map<int32_t, std::unique_ptr<AccessibilityInfoDataWrapper>> tree_map_;

  // The device scale factor of the display which the window is on.
  float device_scale_factor_;

  // Maps an AccessibilityInfoDataWrapper ID to its parent.
  std::map<int32_t, int32_t> parent_map_;

  std::unique_ptr<AXTreeArcSerializer> current_tree_serializer_;
  base::Optional<int32_t> root_id_;
  base::Optional<int32_t> window_id_;
  base::Optional<int32_t> android_focused_id_;

  bool is_notification_;
  bool is_input_method_window_;

  std::map<int32_t, std::string> cached_names_;
  std::map<int32_t, ax::mojom::Role> cached_roles_;

  // Mapping from Chrome node ID to its cached computed bounds.
  // This simplifies bounds calculations.
  std::map<int32_t, gfx::Rect> computed_bounds_;

  // Mapping from Chrome node ID to the previous computed name for live region.
  std::map<int32_t, std::string> previous_live_region_name_;

  // A delegate that handles accessibility actions on behalf of this tree. The
  // delegate is valid during the lifetime of this tree.
  const Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceArc);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_
