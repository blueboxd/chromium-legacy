// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_WEB_TEST_SUPPORT_RENDERER_H_
#define CONTENT_PUBLIC_TEST_WEB_TEST_SUPPORT_RENDERER_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"

namespace content {
class RenderFrame;

// Turn a renderer into web test mode.
void EnableRendererWebTestMode();

// Enable injecting of a WebViewTestProxy between WebViews and RenderViews,
// WebWidgetTestProxy between WebWidgets and RenderWidgets and WebFrameTestProxy
// between WebFrames and RenderFrames.
void EnableWebTestProxyCreation();

// Run all pending idle tasks immediately, and then invoke callback.
void SchedulerRunIdleTasks(base::OnceClosure callback);

// Causes the RenderWidget corresponding to |render_frame| to update its
// TextInputState.
void ForceTextInputStateUpdateForRenderFrame(RenderFrame* render_frame);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_WEB_TEST_SUPPORT_RENDERER_H_
