// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_LIVE_REGION_TRACKER_H_
#define UI_ACCESSIBILITY_AX_LIVE_REGION_TRACKER_H_

#include <map>
#include <set>
#include <vector>

#include "ui/accessibility/ax_tree.h"

namespace ui {

// Class that works with AXEventGenerator to track live regions in
// an AXTree.
class AXLiveRegionTracker {
 public:
  explicit AXLiveRegionTracker(AXTree* tree);

  ~AXLiveRegionTracker();

  void TrackNode(AXNode* node);
  void OnNodeWillBeDeleted(AXNode* node);
  void OnAtomicUpdateFinished();
  AXNode* GetLiveRoot(AXNode* node);
  AXNode* GetLiveRootIfNotBusy(AXNode* node);

  static bool IsLiveRegionRoot(const AXNode* node);

 private:
  void InitializeLiveRegionNodeToRoot(AXNode* node, AXNode* current_root);

  AXTree* tree_;  // Not owned.

  // Map from live region node to its live region root ID.
  std::map<AXNode*, int32_t> live_region_node_to_root_id_;
  std::set<int32_t> deleted_node_ids_;

  DISALLOW_COPY_AND_ASSIGN(AXLiveRegionTracker);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_LIVE_REGION_TRACKER_H_
