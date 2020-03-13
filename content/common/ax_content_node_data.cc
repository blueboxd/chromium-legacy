// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ax_content_node_data.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "content/common/ax_content_node_data.mojom.h"

using base::NumberToString;

namespace content {

AXContentNodeData& AXContentNodeData::operator=(const AXNodeData& other) {
  *static_cast<AXNodeData*>(this) = other;
  return *this;
}

std::string AXContentNodeData::ToString() const {
  std::string result = AXNodeData::ToString();
  if (child_routing_id != MSG_ROUTING_NONE)
    result += " child_routing_id=" + NumberToString(child_routing_id);
  return result;
}

std::string AXContentTreeData::ToString() const {
  std::string result = AXTreeData::ToString();

  if (routing_id != MSG_ROUTING_NONE)
    result += " routing_id=" + NumberToString(routing_id);
  if (parent_routing_id != MSG_ROUTING_NONE)
    result += " parent_routing_id=" + NumberToString(parent_routing_id);

  return result;
}

}  // namespace ui
