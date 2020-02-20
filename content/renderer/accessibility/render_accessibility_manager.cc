// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_manager.h"

#include <utility>

#include "base/bind.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/render_frame_impl.h"

namespace content {

RenderAccessibilityManager::RenderAccessibilityManager(
    RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {}

RenderAccessibilityManager::~RenderAccessibilityManager() = default;

void RenderAccessibilityManager::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::RenderAccessibility> receiver) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
}

RenderAccessibilityImpl*
RenderAccessibilityManager::GetRenderAccessibilityImpl() {
  return render_accessibility_.get();
}

ui::AXMode RenderAccessibilityManager::GetAccessibilityMode() const {
  if (!render_accessibility_)
    return ui::AXMode();
  return render_accessibility_->GetAccessibilityMode();
}

void RenderAccessibilityManager::SetMode(uint32_t ax_mode) {
  ui::AXMode old_mode = GetAccessibilityMode();
  ui::AXMode new_mode(ax_mode);
  if (old_mode == new_mode)
    return;

  if (new_mode.has_mode(ui::AXMode::kWebContents) &&
      !old_mode.has_mode(ui::AXMode::kWebContents)) {
    render_accessibility_ =
        std::make_unique<RenderAccessibilityImpl>(render_frame_, new_mode);
  } else if (!new_mode.has_mode(ui::AXMode::kWebContents) &&
             old_mode.has_mode(ui::AXMode::kWebContents)) {
    render_accessibility_.reset();
  }

  // Notify the RenderFrame when the accessibility mode is changes to ensure it
  // notifies the relevant observers (subclasses of RenderFrameObserver). This
  // includes the RenderAccessibilityImpl instance owned by |this|, which will
  // make update Blink and emit the relevant events back to the browser process
  // according to change in the accessibility mode being made.
  render_frame_->NotifyAccessibilityModeChange(new_mode);
}

}  // namespace content
