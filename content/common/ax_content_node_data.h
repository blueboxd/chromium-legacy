// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_AX_CONTENT_NODE_DATA_H_
#define CONTENT_COMMON_AX_CONTENT_NODE_DATA_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "ui/accessibility/ax_node_data.h"

namespace content {

// A subclass of AXNodeData that contains extra fields for
// content-layer-specific AX attributes.
struct CONTENT_EXPORT AXContentNodeData : public ui::AXNodeData {
  AXContentNodeData() = default;
  AXContentNodeData(const AXContentNodeData& other) = default;
  ~AXContentNodeData() override = default;
  AXContentNodeData& operator=(const AXNodeData& other);

  // Return a string representation of this data, for debugging.
  std::string ToString() const override;

  // The routing ID of this node's child tree.
  int32_t child_routing_id = MSG_ROUTING_NONE;
};

}  // namespace content

#endif  // CONTENT_COMMON_AX_CONTENT_NODE_DATA_H_
