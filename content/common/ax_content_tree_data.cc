// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ax_content_tree_data.h"

#include "base/strings/string_number_conversions.h"

using base::NumberToString;

namespace content {

std::string AXContentTreeData::ToString() const {
  std::string result = AXTreeData::ToString();

  if (routing_id != MSG_ROUTING_NONE)
    result += " routing_id=" + NumberToString(routing_id);
  if (parent_routing_id != MSG_ROUTING_NONE)
    result += " parent_routing_id=" + NumberToString(parent_routing_id);

  return result;
}

}  // namespace content
