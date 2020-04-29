// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/secondary_test_window_observer.h"

#include "content/public/browser/render_frame_host.h"
#include "content/shell/browser/web_test/web_test_control_host.h"

namespace content {

SecondaryTestWindowObserver::SecondaryTestWindowObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  WebTestControlHost* web_test_control_host = WebTestControlHost::Get();
  if (!web_test_control_host)
    return;
  DCHECK(!web_test_control_host->IsMainWindow(web_contents));

  // Ensure that any preexisting frames (likely just the main frame) are handled
  // as well.
  for (RenderFrameHost* frame : web_contents->GetAllFrames()) {
    if (frame->IsRenderFrameLive())
      web_test_control_host->HandleNewRenderFrameHost(frame);
  }
}

SecondaryTestWindowObserver::~SecondaryTestWindowObserver() {}

void SecondaryTestWindowObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  WebTestControlHost* web_test_control_host = WebTestControlHost::Get();
  if (!web_test_control_host)
    return;
  DCHECK(!web_test_control_host->IsMainWindow(
      WebContents::FromRenderFrameHost(render_frame_host)));
  web_test_control_host->HandleNewRenderFrameHost(render_frame_host);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SecondaryTestWindowObserver)

}  // namespace content
