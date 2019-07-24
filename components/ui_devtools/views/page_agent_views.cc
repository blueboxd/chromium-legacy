// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/page_agent_views.h"

#include "base/command_line.h"
#include "components/ui_devtools/ui_element.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/views_switches.h"

namespace ui_devtools {

PageAgentViews::PageAgentViews(DOMAgent* dom_agent) : PageAgent(dom_agent) {}

PageAgentViews::~PageAgentViews() {}

void PaintRectVector(std::vector<UIElement*> child_elements) {
  for (auto* element : child_elements) {
    if (element->type() == UIElementType::VIEW) {
      element->PaintRect();
    }
    PaintRectVector(element->children());
  }
}

protocol::Response PageAgentViews::disable() {
  // Set bubble lock flag back to false.
  views::BubbleDialogDelegateView::devtools_dismiss_override_ = false;

  // Remove debug bounds rects if enabled.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          views::switches::kDrawViewBoundsRects)) {
    base::CommandLine::ForCurrentProcess()->InitFromArgv(
        base::CommandLine::ForCurrentProcess()->argv());
    PaintRectVector(dom_agent_->element_root()->children());
  }
  return protocol::Response::OK();
}

protocol::Response PageAgentViews::reload(protocol::Maybe<bool> bypass_cache) {
  if (!bypass_cache.isJust())
    return protocol::Response::OK();

  bool shift_pressed = bypass_cache.fromMaybe(false);

  // Ctrl+Shift+R called to toggle bubble lock.
  if (shift_pressed) {
    views::BubbleDialogDelegateView::devtools_dismiss_override_ =
        !views::BubbleDialogDelegateView::devtools_dismiss_override_;
  } else {
    // Ctrl+R called to toggle debug bounds rectangles.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            views::switches::kDrawViewBoundsRects)) {
      base::CommandLine::ForCurrentProcess()->InitFromArgv(
          base::CommandLine::ForCurrentProcess()->argv());
    } else {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          views::switches::kDrawViewBoundsRects);
    }
    PaintRectVector(dom_agent_->element_root()->children());
  }
  return protocol::Response::OK();
}

}  // namespace ui_devtools
