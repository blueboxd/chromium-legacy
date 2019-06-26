// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_

#include <map>
#include <memory>
#include <vector>

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_info_data.h"
#include "components/arc/common/accessibility_helper.mojom.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace arc {
class AXTreeSourceArcTest;

using AXTreeArcSerializer = ui::
    AXTreeSerializer<ArcAccessibilityInfoData*, ui::AXNodeData, ui::AXTreeData>;

// This class represents the accessibility tree from the focused ARC window.
class AXTreeSourceArc : public ui::AXTreeSource<ArcAccessibilityInfoData*,
                                                ui::AXNodeData,
                                                ui::AXTreeData>,
                        public ui::AXActionHandler {
 public:
  class Delegate {
   public:
    virtual void OnAction(const ui::AXActionData& data) const = 0;
  };

  explicit AXTreeSourceArc(Delegate* delegate);
  ~AXTreeSourceArc() override;

  // AXTreeSource overrides.
  bool GetTreeData(ui::AXTreeData* data) const override;

  // Notify automation of an accessibility event.
  void NotifyAccessibilityEvent(mojom::AccessibilityEventData* event_data);

  // Notify automation of a result to an action.
  void NotifyActionResult(const ui::AXActionData& data, bool result);

  // Notify automation of result to getTextLocation.
  void NotifyGetTextLocationDataResult(const ui::AXActionData& data,
                                       const base::Optional<gfx::Rect>& rect);

  // Attaches tree to an aura window and gives it system focus.
  void Focus(aura::Window* window);

  // Gets the window id of this tree.
  int32_t window_id() const { return window_id_; }

  // AXTreeSource overrides used by ArcAccessibilityInfoData subclasses.
  // TODO(katie): should these be "friended" or "protected" instead?
  ArcAccessibilityInfoData* GetRoot() const override;
  ArcAccessibilityInfoData* GetFromId(int32_t id) const override;
  void SerializeNode(ArcAccessibilityInfoData* info_data,
                     ui::AXNodeData* out_data) const override;
  ArcAccessibilityInfoData* GetParent(
      ArcAccessibilityInfoData* info_data) const override;

  // Returns bounds of a node which can be passed to AXNodeData.location. Bounds
  // are returned in the following coordinates depending on whether it's root or
  // not.
  // - Root node is relative to its container, i.e. focused window.
  // - Non-root node is relative to the root node of this tree.
  //
  // focused_window is nullptr for notification.
  const gfx::Rect GetBounds(ArcAccessibilityInfoData* info_data,
                            aura::Window* focused_window) const;

  // Invalidates the tree serializer.
  void InvalidateTree();

  bool is_notification() { return is_notification_; }

  bool is_input_method_window() { return is_input_method_window_; }

 private:
  friend class arc::AXTreeSourceArcTest;
  class FocusStealer;

  // AXTreeSource overrides.
  int32_t GetId(ArcAccessibilityInfoData* info_data) const override;
  void GetChildren(
      ArcAccessibilityInfoData* info_data,
      std::vector<ArcAccessibilityInfoData*>* out_children) const override;
  bool IsIgnored(ArcAccessibilityInfoData* info_data) const override;
  bool IsValid(ArcAccessibilityInfoData* info_data) const override;
  bool IsEqual(ArcAccessibilityInfoData* info_data1,
               ArcAccessibilityInfoData* info_data2) const override;
  ArcAccessibilityInfoData* GetNull() const override;

  // Computes the smallest rect that encloses all of the descendants of
  // |info_data|.
  gfx::Rect ComputeEnclosingBounds(ArcAccessibilityInfoData* info_data) const;

  // Helper to recursively compute bounds for |info_data|. Returns true if
  // non-empty bounds were encountered.
  void ComputeEnclosingBoundsInternal(ArcAccessibilityInfoData* info_data,
                                      gfx::Rect& computed_bounds) const;

  // AXActionHandler overrides.
  void PerformAction(const ui::AXActionData& data) override;

  // Resets tree state.
  void Reset();

  // Maps an ArcAccessibilityInfoData ID to its tree data.
  std::map<int32_t, std::unique_ptr<ArcAccessibilityInfoData>> tree_map_;

  // Maps an ArcAccessibilityInfoData ID to its parent.
  std::map<int32_t, int32_t> parent_map_;
  std::unique_ptr<AXTreeArcSerializer> current_tree_serializer_;
  int32_t root_id_;
  int32_t window_id_;
  int32_t focused_id_;
  bool is_notification_;
  bool is_input_method_window_;

  // A delegate that handles accessibility actions on behalf of this tree. The
  // delegate is valid during the lifetime of this tree.
  const Delegate* const delegate_;
  std::string package_name_;

  // Mapping from ArcAccessibilityInfoData ID to its cached computed bounds.
  // This simplifies bounds calculations.
  std::map<int32_t, gfx::Rect> cached_computed_bounds_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceArc);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_
