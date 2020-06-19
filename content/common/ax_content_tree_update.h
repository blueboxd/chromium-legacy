// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_AX_CONTENT_TREE_UPDATE_H_
#define CONTENT_COMMON_AX_CONTENT_TREE_UPDATE_H_

#include "content/common/ax_content_node_data.h"
#include "content/common/ax_content_tree_data.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_tree_update.h"

namespace content {

typedef ui::AXTreeUpdateBase<content::AXContentNodeData,
                             content::AXContentTreeData>
    AXContentTreeUpdate;

}  // namespace content

#endif  // CONTENT_COMMON_AX_CONTENT_TREE_UPDATE_H_
