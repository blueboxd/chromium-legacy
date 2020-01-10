// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/stub_render_widget_host_owner_delegate.h"

#include "content/public/common/web_preferences.h"

namespace content {

bool StubRenderWidgetHostOwnerDelegate::MayRenderWidgetForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  return true;
}

bool StubRenderWidgetHostOwnerDelegate::ShouldContributePriorityToProcess() {
  return false;
}

bool StubRenderWidgetHostOwnerDelegate::IsMainFrameActive() {
  return true;
}

bool StubRenderWidgetHostOwnerDelegate::IsNeverComposited() {
  return false;
}

WebPreferences
StubRenderWidgetHostOwnerDelegate::GetWebkitPreferencesForWidget() {
  return {};
}

FrameTreeNode* StubRenderWidgetHostOwnerDelegate::GetFocusedFrame() {
  return nullptr;
}

}  // namespace content
