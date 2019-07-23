// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_action_target_factory.h"

#include "content/renderer/accessibility/blink_ax_action_target.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "ui/accessibility/null_ax_action_target.h"

namespace content {

// static
std::unique_ptr<ui::AXActionTarget> AXActionTargetFactory::CreateFromNodeId(
    const blink::WebDocument& document,
    int node_id) {
  blink::WebAXObject blink_target =
      blink::WebAXObject::FromWebDocumentByID(document, node_id);
  if (!blink_target.IsNull())
    return std::make_unique<BlinkAXActionTarget>(blink_target);

  return std::make_unique<ui::NullAXActionTarget>();
}

}  // namespace content
