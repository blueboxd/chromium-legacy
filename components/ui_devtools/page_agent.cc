// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/page_agent.h"

#include "base/command_line.h"
#include "components/ui_devtools/ui_element.h"

namespace ui_devtools {

PageAgent::PageAgent(DOMAgent* dom_agent) : dom_agent_(dom_agent) {}

PageAgent::~PageAgent() {}

void PaintRectVector(std::vector<UIElement*> child_elements) {
  for (auto* element : child_elements) {
    if (element->type() == UIElementType::VIEW) {
      element->PaintRect();
    }
    PaintRectVector(element->children());
  }
}

protocol::Response PageAgent::reload() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          "draw-view-bounds-rects")) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        "draw-view-bounds-rects");
  } else {
    base::CommandLine::ForCurrentProcess()->InitFromArgv(
        base::CommandLine::ForCurrentProcess()->argv());
  }
  PaintRectVector(dom_agent_->element_root()->children());
  return protocol::Response::OK();
}

}  // namespace ui_devtools
