// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/back_forward_cache.h"

#include "base/strings/string_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

void BackForwardCache::DisableForRenderFrameHost(
    RenderFrameHost* render_frame_host,
    base::StringPiece reason) {
  content::WebContents::FromRenderFrameHost(render_frame_host)
      ->GetController()
      .GetBackForwardCache()
      .DisableForRenderFrameHost(content::GlobalFrameRoutingId(
                                     render_frame_host->GetProcess()->GetID(),
                                     render_frame_host->GetRoutingID()),
                                 reason);
}
}  // namespace content
