// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/blink_test_browser_support.h"

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/unique_name_helper.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

std::string GetFrameNameFromBrowserForWebTests(
    RenderFrameHost* render_frame_host) {
  RenderFrameHostImpl* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  FrameTreeNode* frame_tree_node = render_frame_host_impl->frame_tree_node();
  std::string unique_name = frame_tree_node->unique_name();
  return UniqueNameHelper::ExtractStableNameForTesting(unique_name);
}

}  // namespace content
