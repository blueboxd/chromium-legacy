// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_AX_CONTENT_TREE_DATA_H_
#define CONTENT_COMMON_AX_CONTENT_TREE_DATA_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "ui/accessibility/ax_tree_data.h"

namespace content {

// A subclass of AXTreeData that contains extra fields for
// content-layer-specific AX attributes.
struct CONTENT_EXPORT AXContentTreeData : public ui::AXTreeData {
  AXContentTreeData() = default;
  ~AXContentTreeData() override = default;

  // Return a string representation of this data, for debugging.
  std::string ToString() const override;

  // The routing ID of this frame.
  int routing_id = MSG_ROUTING_NONE;

  // The routing ID of the parent frame.
  int parent_routing_id = MSG_ROUTING_NONE;
};

}  // namespace content

#endif  // CONTENT_COMMON_AX_CONTENT_TREE_DATA_H_
