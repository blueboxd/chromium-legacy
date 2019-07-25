// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_ACTION_TARGET_FACTORY_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_ACTION_TARGET_FACTORY_H_

#include <memory>

#include "content/common/content_export.h"

namespace blink {
class WebDocument;
}

namespace ui {
class AXActionTarget;
}

namespace content {

class CONTENT_EXPORT AXActionTargetFactory {
 public:
  // Given a node id, obtain a node from the appropriate tree source and wrap
  // it in an abstraction for dispatching accessibility actions.
  static std::unique_ptr<ui::AXActionTarget> CreateFromNodeId(
      const blink::WebDocument& document,
      int node_id);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_ACTION_TARGET_FACTORY_H_
